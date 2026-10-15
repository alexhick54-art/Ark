# Third-party code

## LAS Recomp (MIT)

Source: https://github.com/peiikai/LASRecomp, commit 81bf718e43482f129c11343668af5abb7f021894.

Files taken from it, with Ark's changes marked `Ark:` in the code:

| Ark path | LAS path | Ark changes |
|---|---|---|
| `tools/lifter/lifter.py` | `tools/lifter.py` | 64-bit `LDXP`/`LDAXP`/`STXP`/`STLXP` (128-bit exclusive pairs) |
| `tools/lifter/gen.py` | `tools/gen.py` | on-demand page mapping during jump-table emulation; masked-index tables; `adr` jump-table bases no longer split functions |
| `tools/lifter/nso.py` | `tools/nso.py` | pure-Python LZ4 fallback (`tools/lifter/lz4.py`, Ark's own) |
| `tools/lifter/analyze.py` | `tools/analyze.py` | `capstone` only imported by the census |
| `tools/lifter/simd_c.py` | `tools/simd_c.py` | none |
| `tools/lifter/passes.py` | `tools/passes.py` | none |
| `tools/lifter/test_lift.py` | `tools/test_lift.py` | Linux/x86-64 host support (portable SIMD mode, OpenSSL MD5) |
| `tools/lifter/test_harness.c` | `tools/test_harness.c` | Linux/x86-64 host support |
| `runtime/include/cpu.h` | `runtime/cpu.h` | `excl_val2`, `ldxp64`/`stxp64` |
| `runtime/include/simd.h` | `runtime/simd.h` | none |
| `runtime/include/simd_fp.h` | `runtime/simd_fp.h` | none |
| `tools/lifter/gen.py` (again) | | resolved jump tables no longer get the all-instruction fallback (`rt_jt_miss`) |
| `runtime/rt.h` | `runtime/rt.h` | module bases for four modules; 64 GB guest window; 48 GB alias region |
| `runtime/loader.c` | `runtime/loader.c` | four modules (main, multimedia, glslc, sdk) |
| `runtime/dispatch.c` | `runtime/dispatch.c` | four statically linked function tables (no game library loader yet); `rt_jt_miss` |
| `runtime/kernel.c`, `kernel.h` | same | `svcGetInfo` 16/17 (system resource size); `svcMapPhysicalMemory` keeps mapped pages, `svcUnmapPhysicalMemory` discards; merged physical-mapping regions |
| `runtime/ipc.c`, `ipc.h`, `hle.h`, `fnprof.c`, `platform.h` | same | `plat_discard` added to `platform.h` |
| `runtime/platform/linux.c`, `linux_compat.h`, `windows.c`, `win_compat.h` | `runtime/platform/` | headless build (no SDL message box), log redirect only with `ARK_LOG_DIR`, `plat_discard` |
| `runtime/services/*.c` | `runtime/services/` | none (LAS-only hooks satisfied by `runtime/stubs.c`, `rando.h`, `settings.h`) |
| `runtime/hle/heaps.c`, `vi.c`, `pl.c`, `nvn_boot.c`, `swkbd.c` | `runtime/hle/` | none |
| `runtime/romfs_build.c`, `romfs_build.h` | same | none |

Ark's own files in `runtime/`: `main.c` (based on LAS's), `stubs.c`, `rando.h`, `settings.h`, `headless/SDL3/SDL.h`.

LAS Recomp's license:

```
MIT License

Copyright (c) 2026 Peiikai

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

This license covers LAS Recomp's own code. It does not cover The Legend of Zelda: Link's Awakening or any other
Nintendo property, nor the third-party components in third_party/, which keep their own licenses.
```
