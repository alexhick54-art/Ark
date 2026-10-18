# Ark

A static recompilation of **Pokémon Legends: Arceus** (Nintendo Switch, v1.1.1) into a native PC program, in the
style of [LAS Recomp](https://github.com/peiikai/LASRecomp) and
[ssbu-recomp](https://github.com/Denkishi/ssbu-recomp).

The game's AArch64 machine code is translated to C ahead of time and compiled for the PC. The Switch's system
services are re-implemented natively, and the plan is for its graphics API (NVN) to run on Vulkan.
This is not an emulator: there is no CPU interpreter or JIT.

> **This repository contains no game code, no game assets, no keys and no firmware.**
> You need your own legally obtained dump of the game, updated to v1.1.1
> (build ID `AEE8F150DDA1B5A838806E1A5EA6827AD9F3C51E`). Everything derived from it, including the translated C,
> is generated on your machine and is git-ignored.

## Status

**The recompiled game reaches its opening scene**: logo, language menu, and Arceus's opening with dialogue text,
rendered through Ark's NVN-on-Vulkan layer (tested so far on a software Vulkan driver). See [docs/STATUS.md](docs/STATUS.md) for the measurements and
what comes next.

| Step | State |
|---|---|
| Translate all four code modules (`main`, `sdk`, `multimedia`, `glslc`) to C | done: ~227,000 functions, 0 untranslatable instructions |
| Translator checked against Unicorn on the game's own instructions | 42,000 encodings / ~126,000 random cases, 0 mismatches |
| Generated C compiles | Linux x86-64: all 1,796 files, 0 errors. Windows: spot-checked |
| Runtime: loader, kernel, system services (from LAS Recomp) | boots through SDK start-up, 1,895 game constructors and `nnMain` |
| NVN on Vulkan, shader translation | logo, language menu, opening scene with text |
| Window, audio out, input, plays | window and audio code in the tree; not played yet |

## Layout

```
tools/
  gen_all.py        translate every module from your exefs dump (uses tools/layout.py for addresses)
  layout.py         where each module lives in guest memory
  setup_unicorn.sh  builds Unicorn from source if `pip install unicorn` is not an option
  lifter/           AArch64 -> C translator (from LAS Recomp, MIT; changes listed in THIRD_PARTY.md)
  mkromfs/          RomFS image from a romfs folder
runtime/            loader, dispatcher, kernel (SVCs), IPC and services, host replacements for SDK functions
  include/          CPU state and helpers the generated C uses
  platform/         host OS layer (Linux, Windows)
docs/
  STATUS.md         progress, measurements, next steps (start here when resuming work)
  INVENTORY.md      what the game's modules contain and which system pieces it needs
```

## Building (Linux)

Windows: see [docs/BUILD-windows.md](docs/BUILD-windows.md).

Requirements: Python 3.10+, clang, Unicorn's Python bindings (`pip install unicorn`, or
`source tools/setup_unicorn.sh`), and SDL3, the Vulkan loader, shaderc, VMA and libopus (system packages, or
`tools/deps/build_linux_deps.sh` builds them into a prefix; pass it as `DEPS=<prefix>`).

1. Extract the v1.1.1 update's exefs with hactool (`--exefsdir=exefs`), so `exefs/` holds `main`, `sdk`, `subsdk0`,
   `subsdk1` and `main.npdm`. Extract its RomFS as an image (`--romfs=romfs.bin`), or build one from a folder with
   `make mkromfs && build/mkromfs romfs/ romfs.bin`.
2. `make gen EXEFS=exefs` writes the C to `generated/` (about 1.1 GB, a few minutes).
3. `make -j objs` compiles it (about 40 minutes on 2 cores, much less on a desktop).
4. `make -j ark` links `build/ark` (`HEADLESS=1`: no window or renderer).
5. Put `exefs/` and `romfs.bin` in one folder and run `build/ark <that folder>`.
6. `make test-lift EXEFS=exefs` (optional) compares the translator with Unicorn on 20,000 of the game's
   instruction encodings.

## Options

Set as environment variables when starting Ark (e.g. `ARK_60FPS=1 build/ark game`).

| Variable | Effect |
|---|---|
| `ARK_60FPS=1` | 60 fps at normal game speed (v1.1.1): the game's frame time tables and 42 fixed 1/30 s steps in its code become 1/60 s. `=2`: 60 fps at double speed. |
| `ARK_60FPS_SITES=<mask>` | with `ARK_60FPS=1`, which groups of code sites switch (default 79 = all: 1, 2, 4, 8 and 64; see `tools/lifter/variants.py`). For narrowing down something that runs at the wrong speed. |
| `ARK_LOD=1` / `2` / `3` | draw distance: Pokémon/NPC load radius, tree and detail fade distances, animation level-of-detail (1, the community LoD mod's values); plus terrain detail further out (2); as far as the settings go, a 20 km far plane and every model at full detail (3). Applied to the game's own config files in memory. |
| `ARK_LOD0=1` / `0` | models always at full detail (on by default with `ARK_LOD=3`; `0` turns it off there). |
| `ARK_SHADOWS=1` | tighter shadow cascades and the shadow map at 2x resolution (`ARK_SHADOW_RES=2..4` sets the factor). |
| `ARK_ANISO=<n>` | anisotropic filtering, 2..16, for every mipmapped texture. |
| `ARK_TEX_LOD_BIAS=<f>` | mip bias (e.g. `-0.5`: sharper distant textures). |
| `ARK_MODS=<folder>` | files under `<folder>/romfs/` replace the game's (Atmosphere's layered RomFS layout: community RomFS mods work as they are). |
| `LARECOMP_SCALE=<f>` | render resolution scale. |
| `LARECOMP_SAMPLE=<s>` | sampling profiler: the hottest game functions every `<s>` seconds. |
| `LARECOMP_PROF_SVC=1` | per-thread CPU time and kernel wait times every 2 s. |

## Credits

The translator, CPU helpers and the runtime's core come from **LAS Recomp** by Peiikai (MIT); see
[THIRD_PARTY.md](THIRD_PARTY.md).
Ark is a fan project and is not affiliated with Nintendo, The Pokémon Company or Game Freak.
