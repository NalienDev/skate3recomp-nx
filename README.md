<picture>
  <source media="(prefers-color-scheme: dark)" srcset="banner.png">
  <source media="(prefers-color-scheme: light)" srcset="banner-light.png">
  <img alt="Skate 3 Recompilation - Nintendo Switch" src="banner-light.png">
</picture>

# Skate 3 Recompiled — Nintendo Switch port

An unofficial **Nintendo Switch homebrew** port of the Xbox 360 version of Skate 3,
built by static recompilation. This repository is the Switch fork; the desktop
(Windows/Linux/macOS) builds live upstream and are not maintained here.

> **⚠️ This port is NOT playable yet.** It boots, initialises the GPU, loads game
> data and renders roughly one frame, then the guest deadlocks. See
> [Current status](#current-status) below. **Help is very welcome** — the
> investigation is documented in detail so anyone can pick it up.

The project does not include Skate 3 retail game files. To run or build it you
must provide files from your own legally obtained Xbox 360 copy of Skate 3.

## Current status

### What works

- **Builds clean** for `aarch64` / Horizon via devkitA64 — `skate3.nro`, ~95 MB.
- **Boots on hardware** under Atmosphere, launched from hbmenu/Sphaira.
- **Vulkan works** through a statically linked **Mesa NVK** driver (there is no
  Vulkan loader on Horizon). Reports `NVIDIA Tegra X1 (NVK GM20B)`, Mesa 25.0.7.
- **Guest memory** — a libnx `virtmem` backend commits the guest address space on
  demand and correctly aliases the physical mirrors (`0x7F000000`, `0xA0000000`,
  `0xC0000000`, `0xE0000000`, physical membase) onto one backing, so the GPU
  command processor and the guest see the same bytes.
- **The Xenos command processor runs**: the PM4 ring is read correctly, draws
  execute, fences are written, and one frame reaches `IssueSwap` and is presented.
- **Display and presentation are proven working** (overlays render on the panel).
- **Controller input** via a native libnx HID driver, including rumble.
- **Audio, the BIG-archive VFS, DLC discovery and TU3 patching** are wired up.

### What does not work — the blocker

After the first frame the guest stops making progress. Terminal state, measured
on hardware:

- 8–9 guest threads parked in kernel waits, several in a mutual wait.
- The main guest thread blocked on an event whose only producers are themselves
  blocked.
- Three job-pool workers spin-polling private events at ~1000 waits/second that
  **nothing ever signals** (`XEvent::Set` is traced at the object level, so this
  covers kernel-internal signalling too).
- **One guest thread blocked forever on a kernel `TIMER` object with an infinite
  timeout** — the current prime suspect and the most recent finding. It is one of
  only two producers of the event the main thread needs, so if the timer never
  fires the whole graph is stuck.
- The GPU is idle and fully drained; the 60 Hz vblank interrupt and the guest's
  graphics ISR keep running normally the whole time.

**It is a race, not a fixed point.** Adding as little as ~15 log lines during
thread startup moves the stall earlier (from "one frame presented" to "before the
first swap"). Anything timing-sensitive changes the outcome.

### Ruled out, with on-device evidence

Please don't spend time re-testing these — each was disproven by measurement, not
by reasoning:

| Ruled out | Evidence |
|---|---|
| Both renderers | Identical stall with `skate3_native_render` true and false |
| Rendering / presentation | One frame reaches `IssueSwap` and is presented |
| A crash | No Atmosphere crash report for any stalled run |
| vblank / the graphics ISR dying | `[isr]` heartbeat runs at 59.7 Hz indefinitely, `prev=0 ms` |
| Event primitive bugs | Auto/manual reset, initial state, set/reset all verified correct |
| Handle or object aliasing | Every reference to a given handle maps to the same guest object |
| Async I/O completion events | Every `NtReadFile` uses `ev=0 apc=0` — no completion events at all |
| I/O completion APCs | `apc_queued=0` (loader uses synchronous reads), `apc_deliver` healthy |
| Guest spinlocks | Zero `[spinlock]` stalls over a 200k-spin threshold |
| Thread death | No guest thread ever exits (`[guestthread]` EXIT/TERMINATE) |
| `sched_yield` being a stub | libnx defines it (`nm` on `libnx.a`) → `svcSleepThread` |
| `XEvent::Set`'s faked previous state | Guest always passes `previous_state_ptr = NULL` |
| Global-kernel-lock starvation | vblank pacing fix landed, changed nothing |

### Where to pick it up

1. **The timer.** Find out whether the timer the guest waits on is ever *armed*.
   `NtSetTimerEx` is now traced (`[timer] SetTimerEx handle=... due=... period_ms=...`).
   If it is never armed, the guest expects something else to arm it. If it is armed
   and never fires, the fault is in `PosixCondition<Timer>` /
   `src/core/timer_queue.cpp` on Horizon — note that its dispatch thread is a raw
   `std::jthread` (newlib gives raw threads small stacks — see the porting notes)
   and that it passes `clock::time_point::max()` into
   `condition_variable::wait_until`, which is the same overflow class that already
   had to be fixed in `PosixConditionBase::Wait`.
2. **The job pool.** All the parked threads belong to one worker pool sharing guest
   entry `0x82F54D58`, dispatched via trampoline `0x82EEF128`, with the pool's wait
   call site at `0x82EE78B8`. Disassembling the pool's protocol would say what is
   *supposed* to signal those events.
3. **Perf, once it runs.** `NtCreateEvent` costs a median **50 ms** on device
   because every guest-object allocation hits the three-syscall commit path in
   `guest_memory_switch.cpp`. Not the deadlock, but it will matter.

### Diagnostics built in

The port carries a lot of instrumentation, all of it writing to
`sdmc:/switch/skate3/logs/skate3_NNN.log`:

- `[stall]` — a watchdog samples every guest thread every 4 s: PPC `lr`, `r1`, a
  per-thread kernel-call counter, and the authoritative current wait target
  (`inwait=`, `on=`, `objtype=`, `timeout=`). Rising `calls` with a frozen `lr`
  means looping; flat means genuinely blocked.
- `[waitenter]` / `[waitexit ... blocked=<us>]` — microsecond resolution, which is
  what distinguishes "woken by a signal" from "passed straight through because the
  object was already signalled".
- `[signal]`, `[evset]`, `[evcreate]`, `[evreuse]`, `[evquery]` — every event and
  semaphore operation, traced at the object level so kernel-internal signalling is
  visible too.
- `[isr]`, `[guestthread]`, `[spinlock]`, `[timer]`, `[io]`, `[apc]`, and `GMEM`
  guest-memory mapping in `boot.log`.

## Installing

You need a modded Switch running Atmosphere (or equivalent) and homebrew access.

1. Copy `skate3.nro` to `sdmc:/switch/skate3/skate3.nro`.
2. Provide the game data at `sdmc:/switch/skate3/game/` (`default.xex` plus the
   `data/` tree from your own legally obtained copy). The desktop installer's
   output directory can be copied across as-is.
3. Optional: DLC under `sdmc:/switch/skate3/dlc/`.
4. Launch it from hbmenu or Sphaira. Settings are written to
   `sdmc:/switch/skate3/settings.toml`, logs to `sdmc:/switch/skate3/logs/`.

Because it deadlocks after the first frame, this is currently only useful for
development.

## Controls

The native libnx HID driver maps the pad to the Xbox 360 controller the guest
expects. Face buttons are mapped **by position, not by letter** — Nintendo's A/B
and X/Y are mirrored relative to Xbox, so the button under your thumb does what
the on-screen prompt says.

| Switch | Xbox 360 |
|---|---|
| B (bottom) | A |
| A (right) | B |
| Y (left) | X |
| X (top) | Y |
| L / R | LB / RB |
| ZL / ZR | LT / RT |
| Minus / Plus | Back / Start |
| Stick click | LS / RS |

Handheld and docked Joy-Con/Pro Controller both work without reconfiguration
(`padConfigureInput(4, HidNpadStyleSet_NpadStandard)`). Rumble is implemented for
both the handheld pair and a detached pair, gated on the `hid_rumble_enabled`
setting. The settings overlay opens with the `menu_chord` binding (default
**R + Plus**).

## Building

devkitPro with devkitA64 and libnx is required.

**The build must run inside devkitPro's own MSYS2 bash.** Mixing Git Bash with
devkitPro's CMake fails with a `Cannot create temporary file in C:\Windows`
broken-compiler error.

```sh
"D:/devkitPro/msys2/usr/bin/bash.exe" --login -c '
  cd /d/Projetos/skate3recomp-nx &&
  cmake --preset switch-release &&
  cmake --build --preset switch-release -- -k 0'
```

`-k 0` (ninja keep-going) surfaces all independent errors per pass. Configure
takes about 2.5 minutes; a full build recompiles the third-party tree (SDL3,
FFmpeg, spirv-tools, glslang), which is the slow part. Wipe
`out/build/switch-release` after changing `CMAKE_C/CXX_FLAGS_INIT` in
`cmake/Switch.cmake`, since flag-init only seeds a fresh cache.

Output: `out/build/switch-release/skate3.nro`.

### Deploying over FTP

With an FTP server running on the Switch (Sphaira's, for example, on port 5000):

```bash
curl -T out/build/switch-release/skate3.nro ftp://<switch-ip>:5000/switch/skate3/skate3.nro
```

### Porting notes worth knowing before you touch the build

These were all painful to find:

- **Codegen is host-only.** `generated/` must be produced by a host build; the
  recompiler cannot run on `aarch64`. Run a host configure first.
- **CMake's `UNIX` variable is not set** for `CMAKE_SYSTEM_NAME=NintendoSwitch`.
  Without `set(UNIX 1)` in `cmake/Platform/NintendoSwitch.cmake`, every
  `elseif(UNIX)` branch is skipped, so rexglue's whole POSIX platform layer and
  glslang's `OSDependent` target are silently dropped — the archives still build
  but are hollow and fail to link.
- **TLS must be local-exec.** devkitA64 mis-relaxes initial-exec TLS and faults at
  `tp*2`.
- **NVK needs `--whole-archive`**, or Mesa's weak references silently null out 471
  dispatch entrypoints; it also needs a **non-conformant opt-in** env var or GM20B
  is rejected without a word. `SKATE3_NVK_LIBRARY` must be set in the preset.
- **Thread stack sizes must be explicit.** Raw `std::thread` gets newlib's small
  default; deep recompiled call chains overrun it and corrupt the host stack,
  which surfaces much later as a `ret` into a garbage `x30`.
- **Horizon delivers no CPU faults as POSIX signals**, so the runtime's exception
  handler, signal-based thread suspend, and guarded-copy paths are all inert.
  Guest memory is therefore never unmapped.

`third_party/rexglue-sdk` is a Git submodule pinned to the Switch port branch of
the Skate-specific rexglue fork. Clone recursively, or:

```sh
git submodule sync --recursive
git submodule update --init --recursive
```

## Contributing

Help is genuinely wanted, particularly from anyone who knows Horizon threading or
Xbox 360 kernel timer semantics. Useful right now:

- Anything that explains the timer that never fires.
- Reading the job pool's protocol at `0x82F54D58` / `0x82EE78B8`.
- Comparing against another Switch recomp port's threading layer — the
  Marathon/Unleashed NX ports share this memory architecture.

If you report a stall, please attach both `boot.log` and the matching
`logs/skate3_NNN.log` **from the same run** — the PC symbolication workflow needs
the pair.

## Credits

- [rexglue SDK](https://github.com/rexglue/rexglue-sdk), the recompilation SDK
  used by this project.
- [Xenia](https://github.com/xenia-project/xenia), whose Xbox 360 research and
  tooling underpin the kernel and GPU emulation this port inherits.
- [Mesa / NVK](https://www.mesa3d.org/), the Vulkan driver that makes rendering on
  Horizon possible.
- [libnx / devkitPro](https://github.com/switchbrew/libnx), the Switch toolchain.
- The Marathon and Unleashed NX ports, whose guest-memory approach this port
  follows.
