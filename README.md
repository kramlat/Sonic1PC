# Sonic1PC

Sonic the Hedgehog (1991, Sega Genesis / MegaDrive) C Port

A native Qt/KDE-style recompilation of the original game: the whole game is playable, from the SEGA screen to the credits,
including the special stages, the ending, the credits with their attract demos, the continue screen and the "TRY AGAIN" / "END"
screens. Its logic follows the original (REV01); the differences are listed in the handbook (**Help > Sonic 1 PC
Handbook**, or F1).

## Features

* A real window with a menu bar: pick the picture size (original 320x224, or scaled 16:9, 8:5, 5:4 and 4:3, with a widescreen lives
  counter), fullscreen (F11), separate music and sound-effect volumes, and per-channel sound chip mutes.
* Rebindable keyboard and gamepad controls, and adjustable stick dead zones (box or ring) for a drifting gamepad (**Settings > Configure Sonic the Hedgehog...**).
* A handbook with screenshots, in KDE's DocBook form (shown by KDE Help Center when installed) and as an HTML window in the game.
* The KDE About dialog, a Games-menu entry and icons.
* Easter eggs and cheats: the level select, the sound test, the Japanese credits and an Easter Eggs menu (see the handbook).
* Debug mode (always in debug builds, or by entering the code in any build) with VDP, sound chip, object and variable viewers,
  a console, a demo recorder and the SMPS Inspector.
* Settings are kept in YAML at `~/.local/share/SonicPC/Sonic1Settings.cfg`.

## What is in the repository

Folder | What it is
--------|--------
`Sonic The Hedgehog/` | The Sonic 1 port (the project that `cmake` builds by default). The executable is `Sonic`.
`Sonic the Hedgehog 2/` | The Sonic 2 port, work in progress. The executable is `Sonic2`.
`ParadoxEngine/` | The engine both games are built on (a shared library), with its sound compiler, `libparadoxsmps`, and the third-party code in `contrib/`.
`ParadoxMake/` | The project tool that reads each folder's `paradoxmakefile.yml` and builds the engine and the game.

Everything the games need is in the repository (art, music, level data): nothing has to be downloaded or converted to build or play them.

## Dependencies

* A C99 and C++17 compiler (GCC, Clang or MSVC), CMake 3.16 or newer, git
* Qt 6 (Widgets and OpenGLWidgets) -- the game window, menu bar and debug tools
* Qt 6 Multimedia -- audio
* yaml-cpp -- the settings file, and ParadoxMake itself
* SDL2 -- gamepads and the software renderer that draws the frame
* An OpenGL development package (Linux: Mesa's libGL headers; Qt's OpenGLWidgets needs it)
* Optional: KDE Frameworks 6 CoreAddons and XmlGui (the KDE About dialog; a plain message box is used without them), xsltproc (builds the handbook's HTML for the in-game Help
  window), KDE's DocTools (`meinproc6`, `checkXML6`) to check the handbook
* Optional: pkg-config (for builds that require static-linkage)

The packages, by system (the Linux lines are the ones the project is built and tested with; the macOS and Windows lines are written from the build files and have not been run by the maintainer, so treat them as a starting point):

System | Install
--------|--------
Arch Linux, CachyOS | `sudo pacman -S base-devel cmake git qt6-base qt6-multimedia sdl2 yaml-cpp pkgconf` (optional: `libxslt kcoreaddons kxmlgui kdoctools`)
Debian, Ubuntu | `sudo apt install build-essential cmake git pkg-config qt6-base-dev qt6-multimedia-dev libsdl2-dev libyaml-cpp-dev libgl1-mesa-dev` (optional: `xsltproc libkf6coreaddons-dev libkf6xmlgui-dev`)
Fedora | `sudo dnf install gcc gcc-c++ cmake git qt6-qtbase-devel qt6-qtmultimedia-devel SDL2-devel yaml-cpp-devel mesa-libGL-devel`
macOS (Homebrew) | `brew install cmake qt sdl2 yaml-cpp pkg-config` and add `-DCMAKE_PREFIX_PATH="$(brew --prefix qt)"` when configuring
Windows, MSYS2 (UCRT64 shell) | `pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-qt6-base mingw-w64-ucrt-x86_64-qt6-multimedia mingw-w64-ucrt-x86_64-SDL2 mingw-w64-ucrt-x86_64-yaml-cpp mingw-w64-ucrt-x86_64-pkgconf git`
Windows, Visual Studio | Visual Studio 2022 (with the C++ and CMake workloads) and [vcpkg](https://vcpkg.io): `vcpkg install qtbase qtmultimedia sdl2 yaml-cpp --triplet x64-windows`

## Getting the source

The third-party code lives in git submodules (clownassembler, Dear ImGui, Nuked-OPN2), so clone with them:

```
git clone --recursive https://github.com/kramlat/Sonic1PC.git
```

(or run `git submodule update --init --recursive` in an existing clone).

## Building

This project uses CMake. The root CMake project builds ParadoxMake (`ParadoxMake/`, the Paradox Engine project tool, like qmake for Qt), which reads each folder's
`paradoxmakefile.yml` and builds the engine and the game; the real build tree is `build/projects`. Open a terminal and `cd` into the repository folder.

### Linux

Sonic 1:
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
bin/Release/Sonic
```

Sonic 2 (its own build folder: one build tree builds one game):
```
cmake -B build-sonic2 -DPARADOX_PROJECT="Sonic the Hedgehog 2"
cmake --build build-sonic2 -j
bin/Debug/Sonic2
```

Sonic 2 builds as Debug when no build type is given (its debug mode is the way into its tools for now); Sonic 1 builds as Release. You can also run ParadoxMake directly:
`build/ParadoxMake/paradoxmake build "Sonic The Hedgehog"` (`generate`, `configure`, `build`, `install`, `test`, `clean` and `info` are its commands).

The games are one executable plus two shared libraries that sit next to it (`libParadoxEngine.so`, the engine, and `libparadoxsmps.so`, the sound compiler): copy the
folder, not just the executable. The executables are in `bin/<build type>/` (`bin/` for a build without a build type).

### Options

Pass these to the first `cmake` command (`-DNAME=VALUE`; use a separate build folder per build type):

Name | Function
--------|--------
`-DCMAKE_BUILD_TYPE=` | `Debug`, `Release`, or `Premier` / `Showcase` (Release-speed builds that start at the SSRG splash screen; Premier keeps the cheats, Showcase does not)
`-DPARADOX_PROJECT=` | The folder to build: `"Sonic The Hedgehog"` (the default) or `"Sonic the Hedgehog 2"`
`-DJAPANESE=ON` | Compile a Japanese ROM (Sonic 1)
`-DFIX_BUGS=ON` | Fix bugs that are blatant screw-ups that may harm performance (not gameplay bugs)
`-DSPLASH=ON` | Start at the SSRG splash screen
`-DBUILD_TESTS=OFF` | Do not build the unit tests, the smoke tests and the developer tools (on by default)
`-DSANITIZE=ON` | Build with the address and undefined-behaviour sanitizers
`-DLTO=ON` | Enable link-time optimisation
`-DMSVC_LINK_STATIC_RUNTIME=ON` | Link the static MSVC runtime library, to reduce the number of required DLL files (Visual Studio only)

You can pass your own compiler flags with `-DCMAKE_C_FLAGS` and `-DCMAKE_CXX_FLAGS`.

### Tests

The unit tests and the per-zone smoke tests (the real game started in a level) are built by default:

```
ctest --test-dir build/projects --output-on-failure      # Sonic 1 (use build-sonic2/projects for Sonic 2)
bin/Release/SonicTests                                   # or run the unit tests directly (bin/Debug/Sonic2Tests for Sonic 2)
```

### macOS

The same commands as on Linux, with Qt found through Homebrew (`-DCMAKE_PREFIX_PATH="$(brew --prefix qt)"`), and the shared libraries are `.dylib` files that sit next to the
executable. This has not been run by the maintainer.

### Windows

ParadoxMake starts CMake through the system shell with POSIX quoting, which Windows' `cmd.exe` does not understand, so on Windows let ParadoxMake write the build files and run CMake
yourself. Build ParadoxMake first, then:

MSYS2 (UCRT64 shell):
```
cmake -S ParadoxMake -B build/ParadoxMake -G Ninja
cmake --build build/ParadoxMake
build/ParadoxMake/paradoxmake generate -B build/projects "Sonic The Hedgehog"
cmake -S build/projects/paradoxmake -B build/projects -G Ninja -DCMAKE_BUILD_TYPE=Release -DPKG_CONFIG_STATIC_LIBS=ON
cmake --build build/projects
```

Visual Studio (a "Developer PowerShell"; vcpkg's folder in `VCPKG_ROOT`):
```
cmake -S ParadoxMake -B build/ParadoxMake -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build/ParadoxMake --config Release
build/ParadoxMake/Release/paradoxmake.exe generate -B build/projects "Sonic The Hedgehog"
cmake -S build/projects/paradoxmake -B build/projects -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" -DMSVC_LINK_STATIC_RUNTIME=ON
cmake --build build/projects --config Release
```

(For Sonic 2 give `"Sonic the Hedgehog 2"` to `generate` and use another build folder.) The executable and its DLLs end up in `bin/Release`. On Windows, copy the Qt, SDL2 and
yaml-cpp DLLs next to the executable (Qt's `windeployqt` does it for Qt's), or run from the MSYS2 shell. This has not been run by the maintainer: please report what needs changing.

## Installing

To install the game as `/usr/bin/Sonic`, together with its Games-menu entry, icons and handbook, configure with the prefix and install
(build everything first, as your own user, so that `sudo` only copies files):

```
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
sudo cmake --install build
```

The install puts the binary in `<prefix>/bin`, the two shared libraries in `<prefix>/lib`, the desktop entry in `<prefix>/share/applications`, the icons in `<prefix>/share/icons`
and the handbook in `<prefix>/share/doc/HTML/en/sonic1pc` (KDE Help Center) and `<prefix>/share/doc/sonic1pc/html` (the game's Help window).

## Controls

Default keyboard: arrow keys or WASD to move, B / N / M for A / B / C, Return for Start, F11 for fullscreen. On a gamepad: D-pad or
left stick, X / A / B for A / B / C, Start. All of it can be changed in **Settings > Configure Sonic the Hedgehog...**

## Command line

`Sonic --resolution N` starts with picture size N (0 original, 1 16:9, 2 8:5, 3 5:4, 4 4:3), `--zone Z [--act A]` starts in a level, and
`--no-debug` runs a debug build without its debug mode. Test hooks: `--special N`, `--ending N [--ship]`, `--credits N [--emeralds N]`,
`--continue N`, `--demo`. See the handbook for the details.

## Disclaimer

This project is not endorsed by SEGA or Sonic Team.

## Credits

Sonic Team - Original game

Sonic Retro - Sonic 1 Github Disassembly

Clownacy - Additional porting (compression algorithms)
           and for clownassembler, used to compile mappings and animation scripts

CuckyDev - Original C Port
