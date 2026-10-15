# One-time setup of a Windows PC for building Ark (see docs/BUILD-windows.md). Safe to re-run.
# Installs MSYS2 with clang, make, SDL3, the Vulkan headers/loader, shaderc, Vulkan Memory Allocator and libopus;
# then Python 3.12 (python.org build: it has prebuilt Unicorn wheels) and tools/venv with the translator's packages.
# Adapted from LAS Recomp's tools/setup_windows.ps1 (MIT).
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

if (-not (Test-Path C:\msys64\usr\bin\pacman.exe)) {
    Write-Host 'Installing MSYS2...'
    winget install --id MSYS2.MSYS2 -e --accept-source-agreements --accept-package-agreements --silent
}
$env:MSYSTEM = 'UCRT64'
$env:CHERE_INVOKING = '1'
$bash = 'C:\msys64\usr\bin\bash.exe'
Write-Host 'Updating MSYS2 (twice: the first pass may update the package manager itself)...'
& $bash -lc 'pacman -Syuu --noconfirm' | Out-Null
& $bash -lc 'pacman -Syuu --noconfirm' | Out-Null
$pkgs = 'make', 'git', 'mingw-w64-ucrt-x86_64-clang', 'mingw-w64-ucrt-x86_64-lld', 'mingw-w64-ucrt-x86_64-sdl3',
        'mingw-w64-ucrt-x86_64-vulkan-devel', 'mingw-w64-ucrt-x86_64-shaderc', 'mingw-w64-ucrt-x86_64-spirv-tools',
        'mingw-w64-ucrt-x86_64-vulkan-memory-allocator', 'mingw-w64-ucrt-x86_64-opus', 'mingw-w64-ucrt-x86_64-pkgconf',
        'mingw-w64-ucrt-x86_64-gdb'
Write-Host 'Installing build tools and libraries...'
& $bash -lc "pacman -S --needed --noconfirm $($pkgs -join ' ')"

$py = "$env:LOCALAPPDATA\Programs\Python\Python312\python.exe"
if (-not (Test-Path $py)) {
    Write-Host 'Installing Python 3.12...'
    winget install --id Python.Python.3.12 -e --scope user --accept-source-agreements --accept-package-agreements --silent
}
if (-not (Test-Path "$root\tools\venv\Scripts\python.exe")) { & $py -m venv "$root\tools\venv" }
& "$root\tools\venv\Scripts\python.exe" -m pip install -q unicorn lz4

# LF line endings in this clone (the shell tools break with CRLF; Git for Windows defaults to autocrlf=true)
git -C $root config core.autocrlf false
git -C $root config core.eol lf
Write-Host 'Done. Next: open "MSYS2 UCRT64" from the Start menu, cd to this folder, and follow docs/BUILD-windows.md.'
