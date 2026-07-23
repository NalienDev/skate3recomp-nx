# Skate 3 Recomp - Nintendo Switch Port & Installation Guide

This guide details how to install devkitPro on drive `D:`, configure Switch libraries with Vulkan support, compile `skate3.nro`, and transfer your Skate 3 Xbox 360 game dump from PC to your Nintendo Switch.

---

## Step 1: Install devkitPro on Disk `D:`

1. Run the devkitPro Installer (`D:\devkitProUpdater.exe` or download the latest from [devkitPro Releases](https://github.com/devkitPro/installer/releases)).
2. When prompted for destination, set the install path to:
   ```text
   D:\devkitpro
   ```
3. Open **devkitPro MSYS2 terminal** (or Command Prompt with administrator rights) and run `dkp-pacman` to install all required Nintendo Switch packages:
   ```sh
   dkp-pacman -Syu
   dkp-pacman -S switch-dev switch-tools switch-sdl2 switch-mesa switch-glad switch-libdrm_nouveau switch-pkg-config
   ```

---

## Step 2: Extract Game Dump & Generate Code on PC

1. Place your legally obtained Xbox 360 Skate 3 dump (`default.xex` and `data/webkit/EAWebkit.xex`) into the `game/` folder in the project root (or pass `-DSKATE3_GAME_DATA_ROOT="/path/to/game"`).
2. Generate the recompiled C++ sources:
   ```sh
   cmake --preset switch-release
   cmake --build --preset switch-release --target generate-all --parallel
   ```

---

## Step 3: Build `skate3.nro`

Run CMake configure and build targeting the Switch release preset:

```sh
cmake --preset switch-release
cmake --build --preset switch-release --parallel
```

Once the build completes, your Nintendo Switch Homebrew package will be located at:
```text
out/build/switch-release/skate3.nro
```

---

## Step 4: Transfer Files to Nintendo Switch SD Card

1. Insert your Switch SD Card into your PC (or use FTP / DBI / NxFileViewer).
2. Create the following folder structure on your Switch SD Card:
   ```text
   sdmc:/switch/skate3/
   ├── skate3.nro
   ├── game/
   │   ├── default.xex
   │   ├── data/
   │   └── ... (all extracted game dump files)
   └── dlc/ (optional: place Xbox 360 DLC packages here)
   ```
3. Copy `out/build/switch-release/skate3.nro` to `sdmc:/switch/skate3/skate3.nro`.
4. Copy the entire `game/` folder from your PC project root into `sdmc:/switch/skate3/game/`.

---

## Step 5: Launch on Nintendo Switch

1. Boot your Switch into Atmosphere / Homebrew Launcher (hbmenu).
2. Launch **Skate 3 Recomp** (`skate3.nro`).
3. Enjoy Skate 3 natively recompiled on Nintendo Switch with Vulkan graphics!
