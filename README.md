# Ark

A static recompilation of **Pokémon Legends: Arceus** (Nintendo Switch, v1.1.1) into a native PC program, in the
style of [LAS Recomp](https://github.com/peiikai/LASRecomp) and
[ssbu-recomp](https://github.com/Denkishi/ssbu-recomp).

The game's AArch64 machine code is translated to C ahead of time and compiled for the PC. The plan is for the
Switch's system libraries and its graphics API (NVN) to be re-implemented natively, with NVN running on Vulkan.
This is not an emulator: there is no CPU interpreter or JIT.

> **This repository contains no game code, no game assets, no keys and no firmware.**
> You need your own legally obtained dump of the game, updated to v1.1.1
> (build ID `AEE8F150DDA1B5A838806E1A5EA6827AD9F3C51E`). Everything derived from it, including the translated C,
> is generated on your machine and is git-ignored.

## Status

**Milestone 1 (translation) is done. Nothing runs yet.** See [docs/STATUS.md](docs/STATUS.md) for numbers and
what comes next.

| Step | State |
|---|---|
| Translate all four code modules (`main`, `sdk`, `multimedia`, `glslc`) to C | done: ~227,000 functions, 0 untranslatable instructions |
| Translator checked against Unicorn on the game's own instructions | 42,000 encodings / ~126,000 random cases, 0 mismatches |
| Generated C compiles (Linux x86-64; Windows cross-compile spot-checked) | see docs/STATUS.md |
| Runtime: loader, kernel, system services | next (milestone 2) |
| NVN on Vulkan, shader translation | later (milestone 3) |
| Boots to title, plays | not yet |

## Layout

```
tools/
  gen_all.py        translate every module from your exefs dump (uses tools/layout.py for addresses)
  layout.py         where each module lives in guest memory
  setup_unicorn.sh  builds Unicorn from source if `pip install unicorn` is not an option
  lifter/           AArch64 -> C translator (from LAS Recomp, MIT; changes listed in THIRD_PARTY.md)
runtime/
  include/          CPU state and helpers the generated C uses
docs/
  STATUS.md         progress, measurements, next steps (start here when resuming work)
  INVENTORY.md      what the game's modules contain and which system pieces it needs
```

## Translating (milestone 1)

Requirements: Python 3.10+, clang, and Unicorn's Python bindings (`pip install unicorn`, or
`source tools/setup_unicorn.sh`).

1. Extract the exefs of the v1.1.1 update with hactool (`--exefsdir=exefs`), so `exefs/` holds `main`, `sdk`,
   `subsdk0`, `subsdk1`.
2. `make gen EXEFS=exefs` writes the C to `generated/` (about 1.1 GB, a few minutes).
3. `make -j objs` compiles it into `build/obj/`.
4. `make test-lift EXEFS=exefs` (optional) compares the translator with Unicorn on 20,000 of the game's
   instruction encodings.

## Credits

The translator and CPU helpers come from **LAS Recomp** by Peiikai (MIT); see [THIRD_PARTY.md](THIRD_PARTY.md).
Ark is a fan project and is not affiliated with Nintendo, The Pokémon Company or Game Freak.
