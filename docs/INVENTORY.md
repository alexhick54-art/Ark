# Inventory: what Legends: Arceus v1.1.1 is made of

Measured from the v1.1.1 exefs (main build ID `AEE8F150DDA1B5A8…`). Numbers are from the generator and from
scanning the modules; nothing here is game code.

## Code modules

| Module | exefs file | Guest base (Ark) | .text | Functions (after splitting) | Notes |
|---|---|---|---|---|---|
| main | `main` | `0x7000000000` | 50.6 MB | 161,320 | the game: Game Freak engine (`gfl::`), Havok physics (`hknp`), Wwise, Oodle, Lua, gRPC, Pia |
| sdk | `sdk` | `0x700C000000` | 5.6 MB | 38,295 | `nnSdk.nss`: Nintendo system libraries **and the NVN driver** (main imports only `nvnBootstrapLoader`) |
| multimedia | `subsdk0` | `0x7008000000` | 3.3 MB | 13,691 | `multimedia.nss`: movie/audio decoding; talks to `mm:u` and `es` |
| glslc | `subsdk1` | `0x700A000000` | 6.0 MB | 13,944 | NVIDIA's GLSL compiler |
| rtld | `rtld` | – | 6 KB | – | dynamic loader; not used (the runtime links modules itself) |

`main` lists its dependencies as `multimedia.nss`, `glslc.nss` and `nnSdk.nss`.

## Linking

| Importer | Imports | Resolved from |
|---|---|---|
| main | 900 | sdk 885, glslc 8, unresolved 7 (linker markers, two gRPC TLS thunks, `getentropy`) |
| sdk | 13 | main 8 (`nnMain`, `nninitStartup`, the allocator `malloc`/`free`/`calloc`/`realloc`/`aligned_alloc`, the exception stack), 5 linker markers |
| multimedia | 657 | sdk 648, main 5 |
| glslc | 87 | sdk 80, main 3 |

**main imports `glslcInitialize` / `glslcCompile` / `glslcCompileSpecialized` / `glslcFinalize`.** The game can
compile GLSL source at run time. That matters for the renderer: some shaders may be available as GLSL, not only
as Maxwell machine code. To check in milestone 3: when and for what these are called.

## Kernel calls (SVCs)

The `sdk` uses 61 SVC numbers:
`1-29, 31, 33-35, 37-42, 44, 45, 47, 50-54, 64, 65, 67-69, 72, 73, 75, 80-83, 112, 114`.
`main` uses only `svc 0x26` (Break). `multimedia` and `glslc` make none directly.

## System services referenced by the SDK

54 names appear in `sdk` (presence only; the game won't use all of them):
`acc:u0 acc:u1 aoc:u apm appletAE appletOE audin:u audout:u audren:u audrec:u bsd:u bsd:s caps:su caps:u csrng
friend:u fsp-srv hid hid:sys hidbus irs ldn:u lm mii:e mii:u nfc:user nfp:user nifm:u npns:u nsd:u ntc nvdrv
pctl pctl:a pctl:s pctl:r pl:u prepo:u set set:sys sfdnsres sm: ssl time:u time:a time:s vi:u vi:s vi:m bcat:u
btm:u pl:s olsc:u ectx:aw`. `multimedia` adds `es`, `mm:u`.

The ones a single-player boot is expected to need first: `sm:`, `fsp-srv` (romfs, saves), `appletOE`, `hid`,
`set`/`set:sys`, `time:*`, `acc:u0`, `vi:m`, `nvdrv` (NVN's kernel driver), `audren:u`, `pl:u` (system fonts),
`pctl`, `nifm:u` (answers "offline").

## Middleware inside main (translated like the rest of the game)

- **Havok** (`hknp`, ~1,000 strings): physics. Multi-threaded through the SDK's thread APIs.
- **Wwise** (`AK::`): audio engine; banks under `bin/sound/NX64/wwise/bank/`. Outputs through `nn::audio`
  (the `audren:u` service).
- **Oodle**: decompression of `.gfpak` archives.
- **Lua**, **gRPC** (probably debug/telemetry), **Pia** (local/online play).
