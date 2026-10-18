# Building Ark on Windows

Requirements: Windows 10 or 11 (x64), a CPU with AVX2, a GPU with Vulkan 1.3, and about 6 GB of free disk for the
build (plus the game files). Your own dump of Pokémon Legends: Arceus updated to **v1.1.1**.

## 1. One-time setup

1. Install Git for Windows (PowerShell):

   ```
   winget install --id Git.Git -e
   ```

2. Clone Ark somewhere with space, e.g. `F:\Ark`. Ark is a private repository, so Git opens a browser window to sign
   in to GitHub the first time:

   ```
   git clone https://github.com/alexhick54-art/Ark F:\Ark
   ```

3. Run the setup script (PowerShell, from the clone). It installs MSYS2 (clang, make, SDL3, Vulkan, shaderc, VMA,
   libopus) and Python 3.12 with the translator's packages. It takes a while the first time:

   ```
   cd F:\Ark
   powershell -ExecutionPolicy Bypass -File tools\setup_windows.ps1
   ```

## 2. Game files

Ark needs the v1.1.1 **exefs** (as hactool's `--exefsdir` writes it: `main`, `sdk`, `subsdk0`, `subsdk1`,
`main.npdm`) and the **romfs** (the extracted folder is fine; step 3 turns it into one image file).

Copy the exefs folder to `F:\Ark\game\exefs`.

## 3. Build

Open **MSYS2 UCRT64** from the Start menu (not the plain "MSYS2" one) and run:

```
cd /f/Ark
make gen EXEFS=game/exefs        # translate the game's code to C (a few minutes)
make -j$(nproc) objs             # compile it (the long step: ~10-30 minutes depending on the CPU)
make -j$(nproc) ark mkromfs      # the runtime, the executable and the RomFS tool
build/mkromfs.exe "/f/Switch Test/Decomp/hactool-1.4.0-win/romfs" game/romfs.bin
```

(`/f/...` is how the MSYS2 shell writes `F:\...`. Change the romfs path if yours is elsewhere.)

## 4. Run

From the same shell:

```
build/ark.exe game
```

The log goes to the console. To keep it in a file as well: `build/ark.exe game 2>&1 | tee ark.log`.

Or use the launcher: `make launcher` once, then start `build\ArkLauncher.exe` (from Explorer, or pin it to Start). It
has the options as checkboxes and lists (60 fps, draw distance, shadows, resolution, texture filtering, mods, game
settings, and a box for test options), remembers them in `build\ark_launcher.ini`, writes the log to `ark.log` in the
Ark folder, and says when the game exits. It puts `C:\msys64\ucrt64\bin` on the game's PATH for MSYS2's DLLs (set
`msys2_bin=` in `ark_launcher.ini` if MSYS2 is elsewhere).

## Updating

```
git pull
make gen EXEFS=game/exefs && make -j$(nproc) objs ark
```

`make` only recompiles what changed, so updates that touch the runtime alone take seconds. Generator changes
recompile the translated code.

## Status

This build path is **untested on real Windows so far**: the runtime cross-compiles for Windows, and the generated
code compiles for Windows, but the full build has not been run on a Windows PC yet. Error messages from any step
are exactly what is needed to fix it.
