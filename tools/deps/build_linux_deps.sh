#!/bin/bash
# Build the renderer's dependencies from source into <prefix> (default .deps/prefix), for Linux machines without
# packages for them: Vulkan headers + loader, Vulkan Memory Allocator, shaderc (glslang + SPIRV-Tools), SDL3, and
# optionally SwiftShader (a CPU Vulkan driver, for testing without a GPU: ARK_SWIFTSHADER=1).
# Distribution packages work too (libvulkan-dev libvulkan-memory-allocator-dev libshaderc-dev libsdl3-dev).
# Usage: tools/deps/build_linux_deps.sh [src dir=.deps/src] [prefix=.deps/prefix]
set -e
SRC=$(realpath -m ${1:-.deps/src}); PREFIX=$(realpath -m ${2:-.deps/prefix}); J=${J:-$(nproc)}
mkdir -p "$SRC" "$PREFIX"
clone() { [ -d "$SRC/$1" ] || git clone -q --depth 1 --branch "$3" "$2" "$SRC/$1"; }
cm() {  # cm <dir> [cmake args...]: configure, build, install
    local d=$1; shift
    # (each step returns its failure itself: set -e doesn't apply inside `cm ... || fallback`)
    cmake -S "$SRC/$d" -B "$SRC/$d/build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" \
        -DCMAKE_PREFIX_PATH="$PREFIX" "$@" > "$SRC/$d.cmake.log" 2>&1 || { echo "configure $d failed: $SRC/$d.cmake.log"; return 1; }
    ninja -C "$SRC/$d/build" -j"$J" > "$SRC/$d.build.log" 2>&1 || { echo "build $d failed: $SRC/$d.build.log"; return 1; }
    ninja -C "$SRC/$d/build" install > /dev/null || return 1
    echo "built $d"
}
clone Vulkan-Headers https://github.com/KhronosGroup/Vulkan-Headers v1.3.296
cm Vulkan-Headers
clone Vulkan-Loader https://github.com/KhronosGroup/Vulkan-Loader v1.3.296
cm Vulkan-Loader -DBUILD_WSI_XCB_SUPPORT=OFF -DBUILD_WSI_XLIB_SUPPORT=OFF -DBUILD_WSI_WAYLAND_SUPPORT=OFF -DBUILD_TESTS=OFF
clone VulkanMemoryAllocator https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator v3.1.0
cm VulkanMemoryAllocator
clone shaderc https://github.com/google/shaderc v2024.3
if [ ! -d "$SRC/shaderc/third_party/glslang" ]; then (cd "$SRC/shaderc" && python3 utils/git-sync-deps > "$SRC/shaderc.deps.log" 2>&1); fi
cm shaderc -DSHADERC_SKIP_TESTS=ON -DSHADERC_SKIP_EXAMPLES=ON -DSHADERC_SKIP_COPYRIGHT_CHECK=ON -DSPIRV_SKIP_TESTS=ON \
   -DSPIRV_SKIP_EXECUTABLES=ON -DENABLE_GLSLANG_BINARIES=OFF -DSKIP_GLSLANG_INSTALL=OFF
clone SDL https://github.com/libsdl-org/SDL release-3.4.16
# (machines without X11 / Wayland development files, e.g. a build server: SDL with only its offscreen video driver,
#  which still offers Vulkan through VK_EXT_headless_surface)
cm SDL -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF || { rm -rf "$SRC/SDL/build"; \
    cm SDL -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF -DSDL_X11=OFF -DSDL_WAYLAND=OFF -DSDL_UNIX_CONSOLE_BUILD=ON; }
if [ -n "$ARK_SWIFTSHADER" ]; then
    clone swiftshader https://swiftshader.googlesource.com/SwiftShader master || \
        clone swiftshader https://github.com/google/swiftshader master
    cmake -S "$SRC/swiftshader" -B "$SRC/swiftshader/build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DSWIFTSHADER_BUILD_TESTS=OFF -DSWIFTSHADER_BUILD_BENCHMARKS=OFF -DSWIFTSHADER_WARNINGS_AS_ERRORS=OFF \
        -DREACTOR_BACKEND=Subzero > "$SRC/swiftshader.cmake.log" 2>&1
    ninja -C "$SRC/swiftshader/build" -j"$J" vk_swiftshader > "$SRC/swiftshader.build.log" 2>&1
    mkdir -p "$PREFIX/swiftshader"
    cp "$SRC/swiftshader/build/"*/libvk_swiftshader.so "$SRC/swiftshader/build/"*/vk_swiftshader_icd.json "$PREFIX/swiftshader/" 2>/dev/null || \
        find "$SRC/swiftshader/build" -name 'libvk_swiftshader.so' -o -name 'vk_swiftshader_icd.json' | xargs -I{} cp {} "$PREFIX/swiftshader/"
    echo "built swiftshader (use VK_ICD_FILENAMES=$PREFIX/swiftshader/vk_swiftshader_icd.json)"
fi
echo "deps ready in $PREFIX"
