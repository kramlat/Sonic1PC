# Sonic1PC

Sonic the Hedgehog (1991, Sega Genesis / MegaDrive) C Port

A native Qt/KDE-style recompilation of the original game: the whole game is playable, from the SEGA screen to the credits,
including the special stages, the ending, the credits with their attract demos, the continue screen and the "TRY AGAIN" / "END"
screens. Its logic follows the original (REV01 by default); the differences are listed in the handbook (**Help > Sonic 1 PC
Handbook**, or F1).

## Features

* A real window with a menu bar: pick the picture size (original 320x224, or scaled 16:9, 8:5, 5:4 and 4:3, with a widescreen lives
  counter), fullscreen (F11), separate music and sound-effect volumes, and per-channel sound chip mutes.
* Rebindable keyboard and gamepad controls (**Settings > Configure Sonic the Hedgehog...**).
* A handbook with screenshots, in KDE's DocBook form (shown by KDE Help Center when installed) and as an HTML window in the game.
* The KDE About dialog, a Games-menu entry and icons.
* Easter eggs and cheats: the level select, the sound test, the Japanese credits and an Easter Eggs menu (see the handbook).
* Debug mode (always in debug builds, or by entering the code in any build) with VDP, sound chip, object and variable viewers,
  a console, a demo recorder and the SMPS Inspector.
* Settings are kept in YAML at `~/.local/share/SonicPC/Sonic1Settings.cfg`.

## Dependencies

* Qt 6 (Widgets and OpenGLWidgets) -- the game window, menu bar and debug tools
* Qt 6 Multimedia -- audio
* yaml-cpp -- the settings file (`~/.local/share/SonicPC/Sonic1Settings.cfg`)
* SDL2 -- gamepads and the software renderer that draws the frame
* Optional: KDE Frameworks 6 CoreAddons and XmlGui (the KDE About dialog), xsltproc (builds the handbook's HTML for the in-game Help
  window), KDE's DocTools (`meinproc6`, `checkXML6`) to check the handbook
* pkg-config (for builds that require static-linkage)

## Getting the source

The third-party code lives in git submodules (clownassembler, Dear ImGui, Nuked-OPN2), so clone with them:

```
git clone --recursive https://github.com/kramlat/Sonic1PC.git
```

(or run `git submodule update --init --recursive` in an existing clone).

## Building

This project uses CMake, allowing it to be built with a range of compilers.

Switch to the terminal and `cd` into this folder.

After that, generate the files for your build system with:
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
```

MSYS2 users should append `-G"MSYS Makefiles" -DPKG_CONFIG_STATIC_LIBS=ON` to this command, also.

You can also add the following flags:

Name | Function
--------|--------
`-DREV01=ON` | Compile a REV01 ROM
`-DJAPANESE=ON` | Compile a Japanese ROM
`-DFIX_BUGS=ON` | Fix bugs that are blatant screw-ups that may harm performance (not gameplay bugs)
`-DLTO=ON` | Enable link-time optimisation
`-DMSVC_LINK_STATIC_RUNTIME=ON` | Link the static MSVC runtime library, to reduce the number of required DLL files (Visual Studio only)

You can pass your own compiler flags with `-DCMAKE_C_FLAGS` and `-DCMAKE_CXX_FLAGS`.

You can then compile the executable with this command:

```
cmake --build build --config Release
```

The game is `bin/Release/Sonic` (`bin/Sonic` for a build without a build type). Run it from anywhere: everything it needs is built in.

## Installing

To install the game as `/usr/bin/Sonic`, together with its Games-menu entry, icons and handbook, configure with the prefix and install
(build everything first, as your own user, so that `sudo` only copies files):

```
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
sudo cmake --install build
```

The install puts the binary in `<prefix>/bin`, the desktop entry in `<prefix>/share/applications`, the icons in `<prefix>/share/icons`
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
