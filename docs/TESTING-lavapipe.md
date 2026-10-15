# Testing without a GPU: lavapipe

The development machine has no GPU, so renderer tests run on **lavapipe**, Mesa's CPU Vulkan driver. SwiftShader
doesn't work: it lacks `shaderInt64`, which the translated shaders need, and it crashes on them.

This is a record of the build that worked. It is not a supported script.

## Build (Mesa 25.0.7)

- **Mesa source:** the GitHub mirror `chaotic-cx/mesa-mirror`, tag for 25.0.7 (commit "VERSION: bump for 25.0.7").
- **LLVM:** 18.1.8 from the official release tarball (`clang+llvm-18.1.8-x86_64-linux-gnu-ubuntu-18.04`). It is
  linked statically into lavapipe, so the LLVM folder can be deleted after the build.
  - Its `llvm-config` needs `libtinfo.so.5`. A stub library with versioned symbols (`NCURSES_TINFO_5.0.19991023`)
    satisfies it; put it on `LD_LIBRARY_PATH` and `LIBRARY_PATH` while building.
- **flex 2.6.4**, built from source.
- **libdrm:** grate-driver's 2.4.97 tree. Raise its declared version to 2.4.124 and add stubs for the symbols that
  Mesa's common code references.
- **Python modules** on `PYTHONPATH`: mako, markupsafe, pyyaml, packaging. Meson runs from its source folder.
- **Configure:**

  ```
  meson setup build --buildtype=release -Dgallium-drivers=llvmpipe -Dvulkan-drivers=swrast -Dplatforms= \
    -Dglx=disabled -Degl=disabled -Dgbm=disabled -Dopengl=false -Dgles1=disabled -Dgles2=disabled \
    -Dllvm=enabled -Dshared-llvm=disabled -Dxmlconfig=disabled -Dvalgrind=disabled -Dlibunwind=disabled \
    -Dzstd=disabled -Dexpat=disabled -Dbuild-tests=false -Dvulkan-layers=[] -Dtools=[] -Dcpp_rtti=false
  ```

- **Patch:** with `-Dplatforms=` empty, lavapipe doesn't build its WSI, so SDL's offscreen driver gets no surface.
  This makes the WSI always on:

  ```diff
  --- a/src/gallium/frontends/lavapipe/lvp_device.c
  +++ b/src/gallium/frontends/lavapipe/lvp_device.c
  @@ -58,7 +58,7 @@
       defined(VK_USE_PLATFORM_WIN32_KHR) || \
       defined(VK_USE_PLATFORM_XCB_KHR) || \
       defined(VK_USE_PLATFORM_XLIB_KHR) || \
  -    defined(VK_USE_PLATFORM_METAL_EXT)
  +    defined(VK_USE_PLATFORM_METAL_EXT) || 1 /* Ark test build: headless surface only */
   #define LVP_USE_WSI_PLATFORM
   #endif
  ```

## Run

```
export SDL_VIDEO_DRIVER=offscreen
export VK_ICD_FILENAMES=<prefix>/share/vulkan/icd.d/lvp_icd.x86_64.json
export LD_LIBRARY_PATH=<prefix>/lib
LARECOMP_SHOT_AT=40,200 ARK_RUN_SECONDS=120 build/ark <game dir>
```

### Debug settings

| Variable | Effect |
|---|---|
| `LARECOMP_SHOT_AT=a,b,...` | saves those frames to `build/frames/` in the working folder |
| `LARECOMP_INPUT=frame:BUTTON:len,...` | presses buttons on a script, e.g. `220:A:6` (also `STICK_UP`, …) |
| `LARECOMP_SAVE_MSL=1` | keeps each translated shader's GLSL in `build/glsl/` |
| `ARK_RUN_SECONDS=N` | exits after N seconds |
| `ARK_LOG_OPEN=1` | logs every file open |

On two CPU cores the logo runs at about 5 fps. The opening scene, with effects, runs at about 1.5 fps.
