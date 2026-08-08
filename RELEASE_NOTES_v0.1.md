# Skate 3 Recompiled — Nintendo Switch — v0.1

First tagged build of the Switch port. It **reaches gameplay** and is playable,
but it is early: expect to relaunch several times to get past the main menu, and
expect about 6 fps once you are in.

This is homebrew for a modded Switch. It contains **no Skate 3 game data** — you
must supply that from your own legally obtained Xbox 360 copy.

## What works

- Boots on hardware under Atmosphere, launched from hbmenu or Sphaira.
- Vulkan through a statically linked **Mesa NVK** driver (Horizon has no Vulkan
  loader). Reports `NVIDIA Tegra X1 (NVK GM20B)`, Mesa 25.0.7.
- Menus at **~30 fps**, the rate the game itself targets.
- Character creation, including the team-logo and team-name steps.
- **Gameplay at ~6 fps.**
- Controller input via a native libnx HID driver, including rumble.
- Software keyboard, and `switch_applet_keyboard = false` to skip it.
- HOME is safe — an applet pump keeps Horizon's message queue serviced, so
  pressing HOME exits to hbmenu instead of wedging the console.
- Audio, the BIG-archive VFS, DLC discovery and TU3 patching are wired up.

## Known problems

1. **A crash before the main menu kills many launches.** The biggest problem for
   anyone trying to play. Relaunch until it gets through — it is not deterministic
   and costs nothing but time. Pressing HOME is safe, so a bad launch will not
   force a power-off.
2. **~6 fps in gameplay** (menus are fine at 30). Of a ~170 ms frame, ~107 ms is
   GPU and ~60 ms is the scene builder.
3. **The UI is stretched.** The guest frontbuffer is 960x720 and the 2D layer is
   authored for 16:9. Cosmetic; three corrections were tried and each broke a
   different set of elements, so it is parked.
4. **Text renders as solid blocks** on some screens. Not diagnosed.
5. **No audio output.** The audio path is wired but there is no native driver yet.

## Setup

You need a modded Switch running Atmosphere (or equivalent) with homebrew access.

1. Copy `skate3.nro` to `sdmc:/switch/skate3/skate3.nro`.
2. Provide the game data at `sdmc:/switch/skate3/game/` — `default.xex` plus the
   `data/` tree, from your own copy. The desktop installer's output directory can
   be copied across as-is.
3. Optional: DLC under `sdmc:/switch/skate3/dlc/`.
4. Launch from hbmenu or Sphaira. **Launch it as an application** (Sphaira's
   "boot as application"), not as an applet — the applet heap is far too small.

Settings are written to `sdmc:/switch/skate3/settings.toml` on first run, logs to
`sdmc:/switch/skate3/logs/` and `sdmc:/switch/skate3/boot.log`.

### Settings worth knowing

Defaults are already tuned; these are the ones to reach for.

| setting | default | notes |
|---|---|---|
| `skate3_native_render_scale` | `1.0` | **Set `0.66` for the framerate above.** Renders the 3D world at 43% of the pixels and upscales; the HUD stays sharp. Values below 0.66 crash. |
| `skate3_nvk_sync_submit` | `32` | Leave alone. This is what keeps the `0xd5c` GPU submit failure away. |
| `skate3_native_render_scene_ssao` | `true` | **Leave on.** It is a performance setting here — it feeds the occlusion cull's depth grid. Turning it off makes the game slower, not faster. |
| `switch_applet_keyboard` | `false` | Skips the software keyboard to reach character creation in seconds. |

## Controls

Face buttons map **by position, not by letter** — Nintendo's A/B and X/Y are
mirrored relative to Xbox, so the button under your thumb does what the on-screen
prompt says.

| Switch | Xbox 360 |
|---|---|
| B (bottom) | A |
| A (right) | B |
| Y (left) | X |
| X (top) | Y |

## Reporting problems

Please include `sdmc:/switch/skate3/boot.log` and the newest
`sdmc:/switch/skate3/logs/skate3_NNN.log` **from the same run**, plus any
Atmosphère crash report from `sdmc:/atmosphere/crash_reports/` with a matching
timestamp. The two together are what make a crash address resolvable.
