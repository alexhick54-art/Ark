# Status and handoff

Read this first when resuming work. It records what is done, how it was measured, and the next concrete steps.

Target: Pokémon Legends: Arceus v1.1.1, main build ID `AEE8F150DDA1B5A838806E1A5EA6827AD9F3C51E`.

## Where it stands (2026-10-15)

**The recompiled game boots headless on Linux up to its first graphics call.** It loads the four modules, runs the
SDK's start-up and the game's 1,895 constructors, enters `nnMain`, starts 9 threads, talks to 12 system services,
opens its RomFS, and stops when it creates its NVN graphics device (`nvnDeviceInitialize`), because NVN has no host
implementation yet. No other system function was missing on the way.

| Milestone | State |
|---|---|
| 1. Translate all code to C, compile it | **done** |
| 2. Headless boot to the first GPU call | **done** (Linux x86-64) |
| 3. NVN on Vulkan + shader translation: first frame | next |
| 4. Audio out, input, saves, the field | later |

## Milestone 1: translation

| Module | Functions | Resolved switch tables | Functions on the "any instruction" fallback | Generated C |
|---|---|---|---|---|
| main | 161,320 | 1,805 | 39 | ~800 MB |
| sdk | 38,295 | 574 | 31 | ~95 MB |
| multimedia (subsdk0) | 13,691 | 330 | 2 | ~55 MB |
| glslc (subsdk1) | 13,944 | 1,195 | 30 | ~100 MB |

- **Untranslatable instructions: 0.** Ark added 64-bit `LDXP`/`STXP` (and acquire/release forms), ~3,200 sites in
  main (`lifter.py`; `cpu.h: ldxp64/stxp64`, 128-bit `cmpxchg16b`; unit-tested for success, stale reservation and
  interference).
- **Translator correctness:** `tools/lifter/test_lift.py` runs each translated instruction in C and in Unicorn from
  random states and compares registers, flags, SIMD registers and memory. On this game's own instructions: main 20,000
  encodings, sdk 10,000, multimedia 6,000, glslc 6,000. That's ~126,000 cases, **0 mismatches**. Exclusives are skipped
  by that tester, as in LAS.
- **Switch tables** (Ark changes to `gen.py`): PLA's switches load the index from memory before the bounds check
  (pages are now mapped on demand during the emulated replay), some have no bounds check (masked index), and all use
  `adr` for the table base, which LAS's generator took for a function start, splitting each switch from its own cases.
  Resolved tables in main went from 1 to 1,805.
- **Fallback only where needed:** LAS gives every function with a jump table a label at every instruction plus a
  switch over all of them. Ark does that only for functions with an *unresolved* table (102 in all). A resolved table
  whose target isn't one of its verified cases stops with `rt_jt_miss` (`ARK_TABLE_FALLBACK=1` restores LAS's
  behaviour). One glslc function went from over 5 minutes of compile time to 2 seconds.
- **Compile:** `make -j objs` builds all 1,796 files with clang `-O1`: **0 errors**, 38 minutes on 2 cores (370 MB of
  objects). Windows: chunks of all four modules cross-compile with llvm-mingw clang 18 without errors (spot check;
  the full Windows build is not done yet).

## Milestone 2: headless boot

`make -j ark` links runtime + game into `build/ark` (311 MB). Run: `build/ark <game dir>` with
`<game dir>/exefs/` (main, sdk, subsdk0, subsdk1, main.npdm) and `<game dir>/romfs.bin`.

The runtime is LAS Recomp's (MIT), adapted:

- **Loader:** four modules (`main`, `multimedia`, `glslc`, `sdk`) at the `tools/layout.py` bases; rtld's job done
  by `loader.c`.
- **Entry:** this SDK's `nn::init::Start` takes three callbacks (`_ZN2nn4init5StartEmmPFvvES2_S2_`). The order, read
  from the translated code: `_init_libc0`, `nnosInitialize`, **x2** (init sdk, multimedia, glslc), `_init_libc1`,
  `nninitInitializeSdkModule`, `nninitStartup`, `_init_libc2`, **x3** (init main), `nnMain`, **x4** (fini),
  `nninitFinalizeSdkModule`, `nnosQuickExit`.
- **Virtual-address memory** (the game's heap): `main.npdm` has a 16 MB system resource size, so the SDK's
  `nn::mem::StandardAllocator` runs in VAMM mode and reserves **32 GB** of address space from the *alias region*
  (`svcGetInfo` 2/3). Changes:
  1. Guest window 12.5 GB → 64 GB (`GUEST_HI` = `0x80_0000_0000`).
  2. Alias region 4 GB → 48 GB at `0x74_0000_0000`.
  3. `svcGetInfo` 16/17 report the system resource size.
  4. `svcMapPhysicalMemory` keeps the contents of pages already mapped (LAS zeroed the whole range, which wiped the
     heap's own header), `svcUnmapPhysicalMemory` discards pages (`plat_discard`).
  5. Physical mappings are merged in the region table (LAS's 1,024-entry table would overflow).
- **Headless:** `-DARK_HEADLESS`, with a stand-in for the few SDL3 audio calls (`runtime/headless/SDL3/SDL.h`): no
  output device, the audio services still work. The log goes to stderr (`ARK_LOG_DIR=<dir>` writes `ark.log` there).
- LAS-only features (randomizer, save editor, overlay menu) are no-ops in `runtime/stubs.c`.

### The boot log, in order

Services: `lm`, `apm`, `appletOE`, `aoc:u`, `ectx:aw` (logging stub), `pctl` (+`:s :a :r`), `time:u`, `hid`,
`fsp-srv`. Then `init main`, 9 threads, RomFS opened and indexed, then NVN: `nvnDeviceBuilderSetDefaults`,
`nvnDeviceBuilderSetFlags`, `nvnDeviceInitialize`, then `nvnTextureBuilder*` (SetDefaults, SetDevice, SetFlags,
SetLevels, SetFormat, SetDepthStencilMode, SetStride, SetSwizzle, SetTarget, SetSize2D, GetStorageAlignment,
GetStorageSize). It crashes right after, because the stubbed `GetStorageSize` returned 0 and the game used the
result. That is the start of milestone 3.

## Next: milestone 3, NVN on Vulkan

1. **Decide the NVN layer.** `runtime/hle/nvn_boot.c` already routes every NVN function the game asks for
   (`nvnBootstrapLoader` / `nvnDeviceGetProcAddress`) to a named host function; unimplemented ones log and return 0.
   LAS has an NVN implementation (`runtime/nvn/`: objects, command buffers, Maxwell shader translation, Vulkan
   backend, ~7,000 lines plus `nvn/vulkan/` and `nvn/maxwell/`). ssbu-recomp has another (GPLv3; not copied). Start
   from LAS's: it's the same API.
2. **Device, memory pools, textures, buffers:** builders and `Get*` queries need real answers (storage sizes and
   alignment of block-linear textures) before anything is drawn.
3. **Window and present:** SDL3 window and Vulkan swapchain (LAS's Windows/Linux path), so the headless flag goes.
4. **Shaders:** the game's shaders are Maxwell machine code in `.bnsh`. LAS's `nvn/maxwell/translate.c` translates
   the subset LAS needed; PLA's shaders (compute, tessellation for grass?) need more. The game also links NVIDIA's
   GLSL compiler (`glslc`) and imports `glslcCompile*`: check which shaders it compiles at run time.
5. **Windows build:** `platform/windows.c` (from LAS) is in the tree but untested in Ark; the user's PC is the test
   machine.

## Design decisions so far

- **Guest addresses are host addresses** (LAS's model). The modules sit at fixed bases: main `0x7000000000`,
  multimedia `0x7008000000`, glslc `0x700A000000`, sdk `0x700C000000`. The other regions (`runtime/rt.h`): HLE
  stubs `0x7080…`, TLS `0x7090…`, stacks `0x72_0000_0000`, heap `0x72_4000_0000` (3.25 GB), alias
  `0x74_0000_0000` (48 GB). The whole window `0x70_0000_0000`–`0x80_0000_0000` is reserved at start-up.
- **All four modules are translated, the SDK included**, as LAS does. System behaviour comes from below the SDK
  (SVCs, service IPC) plus host replacements for chosen SDK functions (`HLE_FUNC`, bound by symbol name at load).
- **x86-64 hosts use the portable C SIMD lowering** (`--simd c`, `simd.h`); AArch64 hosts can use inline asm.
- **License:** Ark is MIT. LAS-derived files keep LAS's notice (THIRD_PARTY.md). ssbu-recomp is GPLv3 and is not
  copied.
- **No game-derived bytes in git.** `generated/`, `build/` and dumps are ignored. Docs carry measurements and names
  only.

## Rebuilding the environment (for a new session)

This environment is ephemeral. To get back to the current state:

1. Get the exefs (the user's dump; never commit it). The user can re-send `exefs.rar`. For a boot test, a RomFS image:
   `hactool --romfs=romfs.bin` on the update NCA, or `build/mkromfs <romfs folder> romfs.bin` (`make mkromfs`).
   Without the full romfs, a partial image of a few files still boots to the NVN stop.
2. Unicorn: `pip install unicorn`, or `source tools/setup_unicorn.sh` (builds 2.1.1 from GitHub; ~5 min).
3. `make gen EXEFS=<exefs>` (~2 min), `make -j objs` (~40 min on 2 cores), `make -j ark`.
4. Windows cross-compiler: llvm-mingw from
   `https://github.com/mstorsjo/llvm-mingw/releases/download/20240619/llvm-mingw-20240619-ucrt-ubuntu-20.04-x86_64.tar.xz`.
5. Wine for running Windows test builds:
   `https://github.com/Kron4ek/Wine-Builds/releases/download/9.20/wine-9.20-amd64-wow64.tar.xz`.
6. Useful while debugging: `gdb --args build/ark <dir>`, break on `f_<guest addr>`; the `Cpu *` is in `rdi`.
   Symbol names for guest addresses come from the NSOs' symbol tables (the SDK's exports name almost everything).
