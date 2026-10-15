# Status and handoff

Read this first when resuming work. It records what is done, how it was measured, and the next concrete steps.

## Milestone 1: translate the game's code (done)

Target: Pokémon Legends: Arceus v1.1.1, main build ID `AEE8F150DDA1B5A838806E1A5EA6827AD9F3C51E`.

| Module | Functions | Resolved switch tables | Switches left on the slow "any instruction" path | Generated C |
|---|---|---|---|---|
| main | 161,320 | 1,805 | 55 | ~800 MB |
| sdk | 38,295 | 574 | 41 | ~95 MB |
| multimedia (subsdk0) | 13,691 | 330 | 2 | ~55 MB |
| glslc (subsdk1) | 13,944 | 1,195 | 49 | ~100 MB |

- **Untranslatable instructions: 0.** LAS's translator handled everything except 64-bit exclusive pairs
  (`LDXP`/`STXP` and acquire/release forms, ~3,200 sites, all in `main`), which Ark added
  (`lifter.py`, `cpu.h: ldxp64/stxp64`, 128-bit `cmpxchg16b`). The helper was unit-tested: success,
  stale reservation, and interference.
- **Translator correctness:** `tools/lifter/test_lift.py` runs each translated instruction in C and in Unicorn
  from random states and compares registers, flags, SIMD registers and memory. On this game's own instructions:
  main 20,000 encodings, sdk 10,000, multimedia 6,000, glslc 6,000. That's ~126,000 cases, **0 mismatches**.
  (Exclusive instructions are skipped by that tester, as in LAS.)
- **Switch tables:** before Ark's generator fixes, 1 switch site in main resolved. Two causes:
  1. PLA's switch code loads the index from memory before the bounds check, and the emulated replay faulted
     on unmapped memory. Fixed by mapping pages on demand.
  2. Switches use `adr` for the table base (the first case), and the generator treated every `adr` target as a
     function start, splitting each switch from its own cases. Fixed by ignoring `adr`s that feed a nearby `br`.
  Masked-index switches (no bounds check) are also handled now. Function count dropped from 205,503 to
  161,320 in main as the false splits went away.
- **Compile:** the generated C is compiled with clang `-O1` (`make -j objs`), on Linux x86-64 in this
  environment. Result: see "Compile check" below. A main chunk (~1,000 functions, 1.5 MB of C) also
  cross-compiles for Windows with llvm-mingw clang 18 without errors.

### Compile check

<!-- filled in when the full compile finishes -->

## Design decisions so far

- **Guest addresses are host addresses** (LAS's model). Modules are mapped at fixed bases (`tools/layout.py`):
  main `0x7000000000`, multimedia `0x7008000000`, glslc `0x700A000000`, sdk `0x700C000000`. The runtime
  must reserve that range at start-up.
- **All four modules are translated, the SDK included**, as LAS does. System behaviour is provided below the
  SDK, at kernel calls (SVC) and service IPC, plus host replacements ("HLE") for selected SDK functions where
  that is simpler or faster (LAS's `hle_symbols.txt` mechanism: `gen.py --hle`).
- **x86-64 hosts use the portable C SIMD lowering** (`--simd c`, `simd.h`); AArch64 hosts can use inline asm.
- **License:** Ark is MIT; LAS-derived files keep LAS's MIT notice (THIRD_PARTY.md). ssbu-recomp is GPLv3,
  so its code is not copied into Ark.
- **No game-derived bytes in git.** `generated/`, `build/` and dumps are ignored. Docs carry measurements
  and names only.

## Next: milestone 2, a headless boot

Goal: run the game's start-up with no graphics or audio output and log every kernel call and service request
until it first needs the GPU. This can be done and tested without a GPU.

1. **Runtime skeleton** (start from LAS `runtime/`, MIT): `loader.c` (map the four NSOs at the bases above,
   apply relocations, resolve imports across modules), `dispatch.c` (address -> translated-function table,
   built from each module's `<name>_table.c`), `kernel.c` (SVCs: memory, threads, sync, TLS, ticks), `ipc.c`
   (HIPC message parsing), `services/` (sm, fsp-srv on the user's romfs, set, time, acc, applet, hid, nvdrv
   stub, audren stub).
2. **Threads:** Havok, Wwise and the streaming system use many threads. LAS maps guest threads to host
   threads; check its `kernel.c` and the synchronisation SVCs (0x18-0x1D, arbitration 0x34/0x35).
3. **Entry:** `nninitStartup` and `nnMain` (exported by main, imported by sdk), via the SDK's start-up code.
4. **Logging:** first unimplemented SVC / service command stops with a clear message. This is the work list.
5. Then milestone 3: NVN. The NVN driver is inside `sdk` (main imports only `nvnBootstrapLoader`). Either
   intercept at the NVN API level (as ssbu-recomp and LAS do) by replacing `nvnBootstrapLoader`'s procedure
   table with host functions, or below it at `nvdrv`. The API level is the proven route.

## Rebuilding the environment (for a new session)

This environment is ephemeral. To get back to the current state:

1. Get the exefs (the user's dump; never commit it). The user can re-send `exefs.rar`.
2. Unicorn: `pip install unicorn`, or `source tools/setup_unicorn.sh` (builds 2.1.1 from GitHub; ~5 min).
3. `make gen EXEFS=<exefs>` (~2 minutes), then `make -j objs`.
4. Windows cross-compiler, if needed: llvm-mingw from
   `https://github.com/mstorsjo/llvm-mingw/releases/download/20240619/llvm-mingw-20240619-ucrt-ubuntu-20.04-x86_64.tar.xz`.
5. Wine for running Windows test builds headless: `https://github.com/Kron4ek/Wine-Builds/releases/download/9.20/wine-9.20-amd64-wow64.tar.xz`.
