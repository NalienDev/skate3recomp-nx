<picture>
  <source media="(prefers-color-scheme: dark)" srcset="banner.png">
  <source media="(prefers-color-scheme: light)" srcset="banner-light.png">
  <img alt="Skate 3 Recompilation - Nintendo Switch" src="banner-light.png">
</picture>

# Skate 3 Recompiled — Nintendo Switch port

An unofficial **Nintendo Switch homebrew** port of the Xbox 360 version of Skate 3,
built by static recompilation. This repository is the Switch fork; the desktop
(Windows/Linux/macOS) builds live upstream and are not maintained here.

> **⚠️ This port is playable-ish, but not yet good.** It boots, loads, plays
> through the menus at ~30 fps, gets through character creation, and enters the
> game at about **6 fps**. The `0xd5c` GPU submit failure that used to abort runs
> is understood and mitigated (zero failures across a session), but **an
> unexplained crash before the main menu still kills many launches** — expect to
> relaunch several times to get into gameplay. See
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
- **It reaches gameplay.** Menus, character creation (including the team-logo and
  team-name steps) and the transition into the game all work.
- **Menus run at ~30 fps**, the rate the game itself targets.
- **The software keyboard** works, and can be bypassed with
  `switch_applet_keyboard = false` to reach character creation in seconds.
- **Kernel timers work** (this was a long-standing blocker — see below).
- **Controller input** via a native libnx HID driver, including rumble.
- **HOME is safe.** An applet pump thread keeps Horizon's message queue serviced,
  so pressing HOME exits to hbmenu instead of wedging the console.
- **Audio, the BIG-archive VFS, DLC discovery and TU3 patching** are wired up.

### What does not work

1. **A crash before the main menu kills many launches.** The single biggest
   problem for anyone trying to play it: expect to relaunch several times.
   The log stops mid-stream with the ISR still alive, no crash report and no exit
   breadcrumb, and failing runs log **zero** `0xd5c` — so it is *not* the submit
   failure below. Every in-process exit route is instrumented and none is taken.
   The applet pump's 2-second out-of-focus self-terminate was ruled out (no focus
   transitions are ever logged). It is unexplained, it looks external, and it is
   the top open bug.
2. **~6 fps in gameplay.** Menus hold ~30 fps. Of a ~170 ms gameplay frame,
   ~107 ms is GPU and ~60 ms is the scene builder. The GPU side is now dominated
   by real per-pixel execution, and the CPU side by per-draw texture and constant
   setup (~20-33 µs and ~17-30 µs across ~465 draws) — that is code-level work,
   not a settings knob.
3. **The UI is stretched** on the narrow 960x720 guest frontbuffer. Three
   corrections were tried and reverted; the 2D stream turns out to be MIXED (some
   draws build their ortho from the 960 buffer with geometry authored for 1280,
   others are already self-consistent), so no single layer-wide transform works.
   It needs a per-draw decision from each draw's own ortho width. Cosmetic.
4. **Text renders as solid blocks** on some screens. Not yet diagnosed. The
   `Direct resolve fallback (cvar-disabled)` lines in the log are a red herring —
   that is the opt-in `vulkan_direct_resolve_fast32` fast path being off, and the
   fallback route is the correct one.

### Solved blockers worth knowing about

These were each expensive to find; the details are in the git log.

- **The character-creation wall — kernel resource exhaustion.** For a long time the
  port died at character creation with a symptom that rotated between an app crash,
  a livelock, a silent abort and a *whole-console* wedge. It was not graphics.
  `svcMapProcessCodeMemory` was failing with `0xCE01` =
  `KernelError_ResourceExhausted`: not host RAM (`mallinfo` showed ~1.5 GB of arena
  headroom) but the process's **kernel system-resource pool**, fixed at process
  creation, which every distinct mapping consumes for page-table and memory-block
  metadata. It is invisible to `mallinfo` *and* to `svcGetInfo(UsedMemorySize)`,
  which is why every memory probe came back clean. The guest backend was making
  **785 of 870 commits a single 4 KiB page**. Rounding each commit out to the 2 MiB
  block granule collapsed that to a handful of mappings: `CommitFresh` went from 870
  calls to **1**, and the wall disappeared. **Cost: ~20 test cycles and five wrong
  hypotheses** (swapchain overlap, image self-corruption, host OOM, the software
  keyboard applet, GOT corruption) — each ruled out with evidence in the git log.

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
- **A crash report can name a pure bystander.** Six different threads were named
  across the wall investigation, every one of them *parked in a wait*, with the PC
  reported as exactly the image base. They were being reported because the process
  was already being torn down. Symbolicating them produced confident, wrong stories
  for many cycles. (User exception handlers do not work under hbloader either — the
  NPDM flag comes from the takeover title — so catching the first fault is not an
  option here.)
- **Measure the thing you are claiming.** `[wsi-prof]` reports *host presents*, and
  the presenter repaints ~3x per guest frame, so it reads roughly 3x the real frame
  rate. It was quoted as fps for several cycles before the discrepancy was caught.
  `Vd: Swap` in the run log is the guest frame rate; cross-check against it.
- **Suspect clock-conversion overflow early.** Three separate bugs here have been
  the same int64-nanosecond overflow (`kEffectivelyInfiniteWait`, the vblank
  deadline, the timer due time). `timer_queue.cpp` still passes
  `clock::time_point::max()` into `condition_variable::wait_until` — same class,
  not yet guarded.

### Where to pick it up

1. **The pre-menu crash.** The top bug, and orthogonal to everything else — every
   performance gain below holds regardless of when it is found. Log-reading has
   been exhausted; the port reached gameplay reliably at an earlier commit, so
   **bisect** rather than adding more instrumentation.
2. **Per-draw CPU cost.** `tex` (~20-33 µs) and `const` (~17-30 µs) per draw
   across ~465 draws is ~25 ms/frame. Enable
   `skate3_native_render_scene_perf_log` + `_perf_items` for the
   `native-scene perf-items:` breakdown. This is the largest remaining CPU item.
3. **Sub-native rendering below 0.66.** `skate3_native_render_scale = 0.55`
   renders correctly but crashes every run. Since 0.66 is stable, something about
   cheaper frames still destabilises the port even with the drain in place.
4. **The 2D aspect correction** — see "What does not work" #3. Needs a per-draw
   ortho-width test, and a capture of the real constants to confirm the two
   populations first.
5. **Remaining raw guest loads.** Only the viewport/scissor capture hooks are
   gated by `GuestReadable()`; the other structure walks in the native scene can
   still fault the same way. Mechanical to fix with the same helper.
5. **`gen_flush_cmdlist` is generated into the channel cmdbuf and never submitted**
   — dead code where libdrm_nouveau emits it between submits.

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

## Performance settings

Gameplay went from 3.85 to ~6 fps by way of three settings. The first two are
compiled in as Switch defaults; the third is opt-in because it costs image
quality. All live in `sdmc:/switch/skate3/settings.toml`.

| setting | value | what it does |
|---|---|---|
| `skate3_nvk_sync_submit` | `32` | Block on the GPU every 32nd kernel submit. **This is what keeps `0xd5c` away.** |
| `skate3_native_render_scene_ssao` | `true` | Enables the **occlusion cull** — see below. Keep it on. |
| `skate3_native_render_scale` | `0.66` | Renders the 3D world at 43% of the pixels and upscales; the HUD stays sharp. |

Two of those are counter-intuitive enough to be worth explaining, because turning
them "off to save performance" makes things slower:

- **`0xd5c` tracks whether the CPU ever *blocks*, not how often it submits.** An
  expensive frame blocks 15-200 ms per frame in `nvFenceWait`, a cheap one 0.4 ms
  — and a GPU that keeps up stops nvgpu retiring finished submit jobs, until the
  per-submit allocation fails. Draining periodically restores that. Swept on
  hardware: period 4/16/32/64 gave 4.7/5.2/**5.9**/5.0 fps, and 64 brought the
  failures back, so 32 is both the fastest and the safe side of the edge.
- **SSAO is a performance setting here.** The occlusion cull's depth grid is a
  by-product of the SSAO pass's reduce, so with AO off the grid never refreshes,
  the cull never engages, and every static item pays full per-item CPU whether or
  not it is visible. It also gates the guest-side dispatch filter. Turning SSAO on
  took draws/frame from 842 to 465 and per-draw cost from 91 µs to 43-69 µs, with
  **no rise in GPU time** — the AO pass pays for itself.

Measured dead, so please do not spend runs on them: submit batching (0 failures →
8, and no main menu), draw distance / LOD at 0.6 (*worse*, and submits per frame
rose), capping the settle pass (net zero — the work moves into the draw path and
frame spikes get worse), and shadows / haze / shadow-atlas size (flat).

## Installing

You need a modded Switch running Atmosphere (or equivalent) and homebrew access.

1. Copy `skate3.nro` to `sdmc:/switch/skate3/skate3.nro`.
2. Provide the game data at `sdmc:/switch/skate3/game/` (`default.xex` plus the
   `data/` tree from your own legally obtained copy). The desktop installer's
   output directory can be copied across as-is.
3. Optional: DLC under `sdmc:/switch/skate3/dlc/`.
4. Launch it from hbmenu or Sphaira. Settings are written to
   `sdmc:/switch/skate3/settings.toml`, logs to `sdmc:/switch/skate3/logs/`.

It reaches gameplay and is playable at ~6 fps, but **a crash before the main menu
kills many launches** — expect to relaunch several times. Pressing HOME is safe,
so trying it will not cost you a hard power-off.

There is no prebuilt `skate3.nro` release. The binary produced by this project
contains the statically recompiled game executable, which is why `generated/` is
gitignored and why no build is distributed: **build it yourself from your own
copy of the game.** See [Building](#building).

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

The build has **two stages**, and only the second is Switch-specific:

1. **Recompile the game, on your host machine.** `rex::rexglue` (the recompiler
   CLI in `third_party/rexglue-sdk`) reads *your* `default.xex` and emits C++ into
   `generated/` — about 289 MB of it. That directory is gitignored and is never
   distributed, which is why there is no prebuilt binary: it is derived from the
   retail executable.
2. **Cross-compile for Switch**, which consumes `generated/` and produces
   `skate3.nro`.

### Stage 1 - codegen (host)

Point the build at your extracted dump and run the codegen target. The
recompiler does not build for Switch, so this stage uses an ordinary host
configure (any desktop generator):

```sh
cmake -B out/build/host -DSKATE3_GAME_DATA_ROOT=/path/to/your/skate3/dump
cmake --build out/build/host --target generate-all
```

`SKATE3_GAME_DATA_ROOT` defaults to `game/` in the source tree and must contain
`default.xex` and `data/webkit/EAWebkit.xex`. `SKATE3_TITLE_UPDATE_PACKAGE` can
point at a TU3 STFS package; without it, codegen falls back to the retail XEX and
the title-update-only sources are skipped.

This step is slow and only has to be rerun when the game files change.

### Stage 2 - the Switch build

devkitPro with devkitA64 and libnx is required, plus a Switch build of **Mesa
NVK** (`libvulkan.a`) — Horizon has no Vulkan loader, so the driver is linked
statically. Building that yourself needs the `switch-nvk` tree and a Docker mesa
build; the archive contains no game code and is published as a release asset, so
you can drop it in instead. Point `SKATE3_NVK_ROOT`/`SKATE3_NVK_LIBRARY` at it
(see `CMakePresets.json`).

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
