<picture>
  <source media="(prefers-color-scheme: dark)" srcset="banner.png">
  <source media="(prefers-color-scheme: light)" srcset="banner-light.png">
  <img alt="Skate 3 Recompilation - Nintendo Switch" src="banner-light.png">
</picture>

# Skate 3 Recompiled — Nintendo Switch port

An unofficial **Nintendo Switch homebrew** port of the Xbox 360 version of Skate 3,
built by static recompilation. This repository is the Switch fork; the desktop
(Windows/Linux/macOS) builds live upstream and are not maintained here.

> **⚠️ This port is NOT playable yet**, but it does now render. It boots, loads
> the game, and reaches the **language select screen** at roughly **9 fps** with
> unreadable (blocky) text, then dies shortly after the Skate 3 logo. See
> [Current status](#current-status). **Help is very welcome** — the whole
> investigation is documented so anyone can pick it up.

The project does not include Skate 3 retail game files. To run or build it you
must provide files from your own legally obtained Xbox 360 copy of Skate 3.

## Current status

### What works

- **Builds clean** for `aarch64` / Horizon via devkitA64 — `skate3.nro`, ~95 MB.
- **Boots on hardware** under Atmosphere, launched from hbmenu/Sphaira.
- **Vulkan works** through a statically linked **Mesa NVK** driver (there is no
  Vulkan loader on Horizon). Reports `NVIDIA Tegra X1 (NVK GM20B)`, Mesa 25.0.7.
- **Guest memory** — a libnx `virtmem` backend commits the guest address space on
  demand and eagerly mirrors every physical commit into all of its windows
  (`0x7F000000`, `0xA0000000`, `0xC0000000`, `0xE0000000` with its 4 KB skew, and
  the physical membase) so any mirror address is valid the moment the page exists.
- **The Xenos command processor runs**: the PM4 ring executes, draws submit,
  fences write back, and frames present continuously.
- **It renders.** The game reaches the language select screen and keeps drawing.
- **Kernel timers work** (this was the long-standing blocker — see below).
- **Controller input** via a native libnx HID driver, including rumble.
- **HOME is safe.** An applet pump thread keeps Horizon's message queue serviced,
  so pressing HOME exits to hbmenu instead of wedging the console.
- **Audio, the BIG-archive VFS, DLC discovery and TU3 patching** are wired up.

### What does not work

Current state, in the order you will hit it:

1. **Text renders as solid blocks.** Font/glyph output is unreadable on the
   language select screen. Not yet diagnosed. Note the `Direct resolve fallback
   (cvar-disabled)` lines in the log are a red herring — that is the opt-in
   `vulkan_direct_resolve_fast32` fast path being off, and the fallback route is
   the correct one.
2. **~9 fps** at the menu, at 640x360 with `draw_resolution_scale 1x1`. No
   profiling done yet. `vulkan_direct_resolve_fast32` may help, but it is
   described in-tree as a narrow prototype and may introduce artefacts.
3. **Dies shortly after the Skate 3 logo** — screen goes black and the process
   stops. Since the applet-pump fix the console itself survives and HOME works,
   but the cause of the app dying here is unknown. **This is the next thing to
   chase.** No Atmosphère crash report was produced for it, so it is likely a hang
   or a clean-ish abort rather than a CPU fault.
4. The language select screen advanced to the logo **without any input**, which
   suggests the input or menu-timing path is not behaving.

### Solved blockers worth knowing about

These were each expensive to find; the details are in the git log.

- **Post-first-frame deadlock — absolute timer due times.** `NtSetTimerEx` with a
  *positive* `due_time` means an ABSOLUTE file time. Skate 3 computes its 60 Hz due
  time as `-(target - now)`; when `now` overtakes `target` the value flips sign, so
  an absolute time ~380 µs after the **1601 epoch** arrives — long past, which NT
  fires immediately. Passing it through unclamped was fatal off-Windows: the POSIX
  timer backend rebases onto `steady_clock` and a ~424-year delta in nanoseconds
  **overflows int64**, so the deadline landed in the far future, the timer never
  fired, and the thread waiting on it never woke — taking the main thread and the
  whole job pool with it. Fixed by clamping a past absolute due time to now.
  **Effect: 1 presented frame → 12, and 20 timer arms → 615.**
- **C-window mirror Data Abort.** Guest code read `0xC3282384` (the `0xC0000000`
  mirror of physical `0x03282384`) on a page that was live via a *different*
  window. Desktop maps all mirrors up front as views of one file mapping; this
  backend only mapped what was explicitly committed, and Horizon delivers no CPU
  faults as POSIX signals so there is nothing to service on demand. Now every
  physical commit is mirrored eagerly.
- **Whole-console freeze on HOME.** Nothing pumped `appletMainLoop()`, so Horizon
  never learned the app had yielded focus and the compositor waited forever on a
  surface it could not reclaim. Only a power-button hold recovered it. Fixed with a
  dedicated applet pump thread that falls back to `svcExitProcess()`.
- **Raw guest loads in the native renderer.** The renderer's raw guest loads rely
  on read-fault recovery, and Switch is on the no-op path. Added
  `GuestReadable()` / `IsCommitted()` gating for the capture hooks.

### Two lessons that cost the most time

- **Never filter an object type out of an untimed-wait trace.** The first wait
  instrumentation excluded timers (`objtype 13`) as "too noisy". That single
  exclusion hid the one thread that mattered for about ten build cycles. Untimed
  waits cannot flood a log — they block.
- **Suspect clock-conversion overflow early.** Three separate bugs here have been
  the same int64-nanosecond overflow (`kEffectivelyInfiniteWait`, the vblank
  deadline, the timer due time). `timer_queue.cpp` still passes
  `clock::time_point::max()` into `condition_variable::wait_until` — same class,
  not yet guarded.

### Where to pick it up

1. **Why the app dies after the logo** (item 3 above) — the open question. Grab
   `logs/skate3_NNN.log` plus `boot.log` from the same run.
2. **The blocky font.** Needs a run that reaches the language screen and a look at
   how glyph textures are being uploaded/resolved.
3. **Perf.** 9 fps at 640x360 has plenty of headroom to investigate;
   `NtCreateEvent` alone costs a median **50 ms** on device because every
   guest-object allocation hits the three-syscall commit path in
   `guest_memory_switch.cpp`.
4. **Remaining raw guest loads.** Only the viewport/scissor capture hooks are
   gated by `GuestReadable()`; the other structure walks in the native scene can
   still fault the same way. Mechanical to fix with the same helper.

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

It gets as far as the language select screen and then dies, so this is still only
useful for development — but pressing HOME is safe now, so trying it will not cost
you a hard power-off.

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

Help is genuinely wanted. Useful right now, roughly in order of value:

- **Why the app dies after the Skate 3 logo.** The current wall.
- **The blocky font** — anyone who knows how Skate 3 uploads its glyph atlases,
  or who can spot a resolve/texture-format problem from a log.
- **Performance.** 9 fps at 640x360; nothing has been profiled yet.
- **Horizon threading and Xbox 360 kernel semantics** generally — three bugs so
  far have been Horizon behaving differently from Windows in ways the shared
  runtime did not anticipate.
- Comparing against another Switch recomp port's threading layer — the
  Marathon/Unleashed NX ports share this memory architecture.

If you report a crash or a stall, please attach **both** `boot.log` and the
matching `logs/skate3_NNN.log` **from the same run** — Horizon re-randomises the
image base every boot, so a report paired with a different run's log produces
nonsense offsets (a negative slide is the tell). Registers are often faster than
the backtrace: `X27` holds the guest membase in recompiled code, so
`fault_address - X27` gives the guest address directly.

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
