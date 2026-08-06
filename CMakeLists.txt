cmake_minimum_required(VERSION 3.12)

# ##############################################################################
# Options #
# ##############################################################################
set(BACKEND
    "SDL2"
    CACHE STRING "Which backend to use (SDL2)")
option(REV01 "Compile REV01 ROM" ON)
option(JAPANESE "Compile Japanese ROM" OFF)
option(FIX_BUGS "Fix bugs (completely screwed up code, not gameplay bugs)" OFF)
option(SPLASH "Enable the SSRG splash screen (for my own demo releases)" OFF)

option(SANITIZE "Enable sanitization" OFF)
option(LTO "Enable link-time optimization" OFF)
option(BUILD_TESTS "Build SonicTests (unit tests) and SonicSmoke (headless level-injection smoke test), and register them with ctest" ON)
option(MSVC_LINK_STATIC_RUNTIME
       "Link the static MSVC runtime library (Visual Studio only)" OFF)

#
# Setup #
#

# Define project, source, and includes
project(Sonic LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# All game sources except Main.c (the entry point), so that Main.c's real
# SDL2 entry point and the test suite's own entry point (tests/test_main.c)
# can each link against the exact same compiled game logic as a static lib.
set(
  SONIC_CORE_SOURCES
  # Game
  "src/Constants.h"
  "src/Macros.h"
  "src/Kosinski.c"
  "src/Kosinski.h"
  "src/Nemesis.c"
  "src/Nemesis.h"
  "src/MathUtil.c"
  "src/MathUtil.h"
  "src/Game.c"
  "src/Game.h"
  "src/Demo.c"
  "src/Demo.h"
  "src/Video.c"
  "src/Video.h"
  "src/Palette.c"
  "src/Palette.h"
  "src/PLC.c"
  "src/PLC.h"
  "src/Sound.c"
  "src/Sound.h"
  "src/Enigma.c"
  "src/Enigma.h"
  "src/PaletteCycle.c"
  "src/PaletteCycle.h"
  "src/Oscillatory Routines.c"
  "src/Oscillatory Routines.h"
  "src/GM_Sega.c"
  "src/GM_Sega.h"
  "src/GM_Title.c"
  "src/GM_Title.h"
  "src/GM_Level.c"
  "src/GM_Level.h"
  "src/GM_Special.c"
  "src/GM_Special.h"
  "src/Level.c"
  "src/Level.h"
  "src/LevelDraw.c"
  "src/LevelDraw.h"
  "src/LevelScroll.c"
  "src/LevelScroll.h"
  "src/LZWaterFeatures.c"
  "src/LZWaterFeatures.h"
  "src/LevelCollision.c"
  "src/LevelCollision.h"
  "src/SpecialStage.c"
  "src/SpecialStage.h"
  "src/HUD.c"
  "src/HUD.h"
  "src/Object.c"
  "src/Object.h"
  "src/Object/Sonic.c"
  "src/Object/SpecialSonic.c"
  "src/Object/Signpost.c"
  "src/Object/TitleSonic.c"
  "src/Object/PSB.c"
  "src/Object/GHZBridge.c"
  "src/Object/Crabmeat.c"
  "src/Object/HUD.c"
  "src/Object/BuzzBomber.c"
  "src/Object/SwingingPlatform.c"
  "src/Object/BigSpikeBall.c"
  "src/Object/Ring.c"
  "src/Object/Waterfall.c"
  "src/Object/Waterfall.h"
  "src/Object/Monitor.c"
  "src/Object/Explosion.c"
  "src/Object/Checkpoint.c"
  "src/Object/Chopper.c"
  "src/Object/TitleCard.c"
  "src/Object/Spikes.c"
  "src/Object/ShieldInvincibility.c"
  "src/Object/GameOverCard.c"
  "src/Object/GHZRock.c"
  "src/Object/Motobug.c"
  "src/Object/Spring.c"
  "src/Object/Newtron.c"
  "src/Object/GHZEdge.c"
  "src/Object/Credits.c"
  "src/Object/Animal.c"
  "src/Object/Points.c"
  # Backend
  "src/Backend/MegaDrive.c"
  "src/Backend/MegaDrive.h"
  "src/Backend/VDP.c"
  "src/Backend/VDP.h"
  "src/Backend/Joypad.c"
  "src/Backend/Joypad.h"
  "src/Backend/SN76489.c"
  "src/Backend/YM2612.cpp"
  "src/Backend/YM2612.h"
  "ymfm/src/ymfm_opn.cpp"
  "ymfm/src/ymfm_ssg.cpp"
  "ymfm/src/ymfm_adpcm.cpp"
  "src/Backend/SN76489.h")

add_library(SonicCore STATIC ${SONIC_CORE_SOURCES})

add_executable(Sonic WIN32 "src/Main.c")
target_link_libraries(Sonic PRIVATE SonicCore)

set(ASM
    "Animation/Sonic"
    "Animation/Signpost"
    "Animation/TitleSonic"
    "Animation/PSB"
    "Animation/Crabmeat"
    "Animation/BuzzBomber"
    "Animation/BuzzMissile"
    "Animation/Ring"
    "Animation/Monitor"
    "Animation/Chopper"
    "Animation/ShieldInvincibility"
    "Animation/Motobug"
    "Animation/Spring"
    "Animation/Newtron"
    "Mappings/Sonic"
    "Mappings/SonicDPLC"
    "Mappings/Signpost"
    "Mappings/TitleSonic"
    "Mappings/PSB"
    "Mappings/GHZBridge"
    "Mappings/Crabmeat"
    "Mappings/HUD"
    "Mappings/BuzzBomber"
    "Mappings/BuzzMissile"
    "Mappings/RingREV00"
    "Mappings/RingREV01"
    "Mappings/Monitor"
    "Mappings/Checkpoint"
    "Mappings/Animals1"
    "Mappings/Animals2"
    "Mappings/Animals3"
    "Mappings/Points"
    "Mappings/Explosion"
    "Mappings/Chopper"
    "Mappings/TitleCard"
    "Mappings/GotThrough"
    "Mappings/SpecialResult"
    "Mappings/Spikes"
    "Mappings/ShieldInvincibility"
    "Mappings/GameOver"
    "Mappings/GHZRock"
    "Mappings/GHZSwing"
    "Mappings/GHZBall"
    "Mappings/SLZSwing"
    "Mappings/BigSpikedBall"
    "Mappings/Motobug"
    "Mappings/Spring"
    "Mappings/Newtron"
    "Mappings/GHZEdge"
    "Mappings/Bumper"
    "Mappings/Credits"
    "Mappings/SSRotate"
    "Mappings/SSGlass"
    "Mappings/SSUp"
    "Mappings/SSDown"
    "Mappings/SSEmerald"
    "Mappings/SSWall"
    "Mappings/Animals1"
    "Mappings/Animals2"
    "Mappings/Animals3"
    "Mappings/SSResultEmerald")

set(RESOURCES
    "Icon"
    "Art/SegaREV00"
    "Art/SegaREV01"
    "Art/JapaneseCredits"
    "Art/TitleFG"
    "Art/Sonic"
    "Art/TitleSonic"
    "Art/TitleTM"
    "Art/TitleCard"
    "Art/GameOver"
    "Art/CreditsFont"
    "Art/Lamppost"
    "Art/HUD"
    "Art/HUDLife"
    "Art/Ring"
    "Art/Points"
    "Art/Monitor"
    "Art/Shield"
    "Art/Invincibility"
    "Art/Explosion"
    "Art/GHZStalk"
    "Art/GHZRock"
    "Art/Crabmeat"
    "Art/BuzzBomber"
    "Art/Chopper"
    "Art/Newtron"
    "Art/Motobug"
    "Art/Spikes"
    "Art/SpringH"
    "Art/SpringV"
    "Art/GHZSwing"
    "Art/GHZBridge"
    "Art/GHZLog"
    "Art/GHZBall"
    "Art/GHZWall1"
    "Art/GHZWall2"
    "Art/LZBlock1"
    "Art/LZBlock2"
    "Art/LZBlock3"
    "Art/Splash"
    "Art/Water"
    "Art/LZSpikeBall"
    "Art/FlapDoor"
    "Art/Bubbles"
    "Art/LZDoor1"
    "Art/LZDoor2"
    "Art/Harpoon"
    "Art/Burrobot"
    "Art/LZPole"
    "Art/LZWheel"
    "Art/Gargoyle"
    "Art/LZSonic"
    "Art/LZPlatfm"
    "Art/Orbinaut"
    "Art/Jaws"
    "Art/LZSwitch"
    "Art/Cork"
    "Art/MZMetal"
    "Art/MZFire"
    "Art/MZGlass"
    "Art/Lava"
    "Art/Yadrin"
    "Art/Basaran"
    "Art/Caterkiller"
    "Art/MZSwitch"
    "Art/MZBlock"
    "Art/Bomb"
    "Art/SLZBlock"
    "Art/SLZWall"
    "Art/Seesaw"
    "Art/Fan"
    "Art/Pylon"
    "Art/SLZSwing"
    "Art/SLZCannon"
    "Art/SLZSpike"
    "Art/Bumper"
    "Art/Roller"
    "Art/SYZSpike1"
    "Art/SYZSpike2"
    "Art/GHZ1"
    "Art/GHZ2"
    "Art/LZ"
    "Art/MZ"
    "Art/SLZ"
    "Art/SYZ"
    "Art/SBZ"
    "Art/Signpost"
    "Art/HiddenBonus"
    "Art/BigFlash"
    "Art/BigRing"
    "Art/GHZWaterfall"
    "Art/GHZFlowerLarge"
    "Art/GHZFlowerSmall"
    "Art/MZLava1"
    "Art/MZLava2"
    "Art/MZTorch"
    "Art/HUDNum"
    "Art/LifeNum"
    "Art/Text"
    "Art/SSClouds"
    "Art/SSBack"
    "Art/SSWall"
    "Art/SSGoal"
    "Art/SSSpeed"
    "Art/SSRotate"
    "Art/SSLife"
    "Art/SSTwinkle"
    "Art/SSChecker"
    "Art/SSGhost"
    "Art/SSWarp"
    "Art/SSGlass"
    "Art/SSEmerald"
    "Art/SSZone1"
    "Art/SSZone2"
    "Art/SSZone3"
    "Art/SSZone4"
    "Art/SSZone5"
    "Art/SSZone6"
    "Art/Rabbit"
    "Art/Flicky"
    "Art/Penguin"
    "Art/Seal"
    "Art/Squirrel"
    "Art/Pig"
    "Art/Chicken"
    "Art/SSResultEmerald"
    "Tilemap/SegaREV00"
    "Tilemap/SegaREV01"
    "Tilemap/JapaneseCredits"
    "Tilemap/TitleFG"
    "Palette/SegaBG"
    "Palette/Title"
    "Palette/LevelSel"
    "Palette/Sonic"
    "Palette/GHZ"
    "Palette/LZ"
    "Palette/MZ"
    "Palette/SYZ"
    "Palette/SLZ"
    "Palette/SBZ1"
    "Palette/Special"
    "Palette/LZWater"
    "Palette/SBZ3"
    "Palette/SBZ3Water"
    "Palette/SBZ2"
    "Palette/SonicLZ"
    "Palette/SonicSBZ"
    "Palette/SSResults"
    "Palette/Continue"
    "Palette/Ending"
    "Palette/Sega1"
    "Palette/Sega2"
    "Palette/TitleCycle"
    "Palette/GHZCycle"
    ${ASM}
    "Layout/GHZ1"
    "Layout/GHZ2"
    "Layout/GHZ3"
    "Layout/GHZBG"
    "Layout/LZ1"
    "Layout/LZ2"
    "Layout/LZ3"
    "Layout/LZBG"
    "Layout/MZ1"
    "Layout/MZ1BG"
    "Layout/MZ2"
    "Layout/MZ2BG"
    "Layout/MZ3"
    "Layout/MZ3BG"
    "Layout/SLZ1"
    "Layout/SLZ2"
    "Layout/SLZ3"
    "Layout/SLZBG"
    "Layout/SYZ1"
    "Layout/SYZ2"
    "Layout/SYZ3"
    "Layout/SYZBGREV00"
    "Layout/SYZBGREV01"
    "Layout/SBZ1"
    "Layout/SBZ1BG"
    "Layout/SBZ2"
    "Layout/SBZ2BG"
    "Layout/SBZ3"
    "Layout/Ending"
    "ObjectLayout/GHZ1"
    "ObjectLayout/GHZ2"
    "ObjectLayout/GHZ3REV00"
    "ObjectLayout/GHZ3REV01"
    "ObjectLayout/LZ1PF1"
    "ObjectLayout/LZ1PF2"
    "ObjectLayout/LZ1REV00"
    "ObjectLayout/LZ1REV01"
    "ObjectLayout/LZ2"
    "ObjectLayout/LZ2PF1"
    "ObjectLayout/LZ2PF2"
    "ObjectLayout/LZ3PF1"
    "ObjectLayout/LZ3PF2"
    "ObjectLayout/LZ3REV00"
    "ObjectLayout/LZ3REV01"
    "ObjectLayout/MZ1REV00"
    "ObjectLayout/MZ1REV01"
    "ObjectLayout/MZ2"
    "ObjectLayout/MZ3"
    "ObjectLayout/SLZ1"
    "ObjectLayout/SLZ2"
    "ObjectLayout/SLZ3"
    "ObjectLayout/SYZ1"
    "ObjectLayout/SYZ2"
    "ObjectLayout/SYZ3REV00"
    "ObjectLayout/SYZ3REV01"
    "ObjectLayout/SBZ1PF1"
    "ObjectLayout/SBZ1PF2"
    "ObjectLayout/SBZ1PF3"
    "ObjectLayout/SBZ1PF4"
    "ObjectLayout/SBZ1PF5"
    "ObjectLayout/SBZ1PF6"
    "ObjectLayout/SBZ1REV00"
    "ObjectLayout/SBZ1REV01"
    "ObjectLayout/SBZ2"
    "ObjectLayout/SBZ3"
    "ObjectLayout/FZ"
    "ObjectLayout/Ending"
    "Map16/GHZ"
    "Map16/LZ"
    "Map16/MZ"
    "Map16/SBZ"
    "Map16/SLZ"
    "Map16/SYZ"
    "Map256/GHZ"
    "Map256/LZ"
    "Map256/MZREV00"
    "Map256/MZREV01"
    "Map256/SBZREV00"
    "Map256/SBZREV01"
    "Map256/SLZ"
    "Map256/SYZ"
    "CollisionIndex/GHZ"
    "CollisionIndex/LZ"
    "CollisionIndex/MZ"
    "CollisionIndex/SLZ"
    "CollisionIndex/SYZ"
    "CollisionIndex/SBZ"
    "Collision/AngleMap"
    "Collision/HeightMap"
    "Collision/WidthMap"
    "Demo/IntroGHZ"
    "Demo/IntroMZ"
    "Demo/IntroSYZ"
    "Demo/IntroSS"
    "Demo/EndingGHZ1"
    "Demo/EndingMZ"
    "Demo/EndingSYZ"
    "Demo/EndingLZ"
    "Demo/EndingSLZ"
    "Demo/EndingSBZ1"
    "Demo/EndingSBZ2"
    "Demo/EndingGHZ2"
    "SSLayout/1"
    "SSLayout/2"
    "SSLayout/3"
    "SSLayout/4"
    "SSLayout/5REV00"
    "SSLayout/5REV01"
    "SSLayout/6REV00"
    "SSLayout/6REV01"
    "DAC/kick"
    "DAC/snare"
    "DAC/timpani"
    "PCM/sega"
    "PSG/psg1"
    "PSG/psg2"
    "PSG/psg3"
    "PSG/psg4"
    "PSG/psg5"
    "PSG/psg6"
    "PSG/psg7"
    "PSG/psg8"
    "PSG/psg9")

target_include_directories(SonicCore PUBLIC "src")

# ##############################################################################
# Compiler flags #
# ##############################################################################

# Revision
if(REV01)
  target_compile_definitions(SonicCore PUBLIC SCP_REV01)
else()
  target_compile_definitions(SonicCore PUBLIC SCP_REV00)
endif()

# Region
if(JAPANESE)
  target_compile_definitions(SonicCore PUBLIC SCP_JP)
endif()

# Bug fixes
if(FIX_BUGS)
  target_compile_definitions(SonicCore PUBLIC SCP_FIX_BUGS)
endif()

# Sanitization
if(SANITIZE)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Og -ggdb3 -fsanitize=address")
endif()

# Splash
if(SPLASH)
  target_compile_definitions(SonicCore PUBLIC SCP_SPLASH)
  target_sources(SonicCore PRIVATE "src/GM_SSRG.c" "src/GM_SSRG.h")
  list(
    APPEND
    RESOURCES
    "SSRG/ArtLink"
    "SSRG/ArtMain"
    "SSRG/ArtSonic"
    "SSRG/ArtSquare"
    "SSRG/MapLink"
    "SSRG/MapMain"
    "SSRG/MapSquare")
endif()

# Strip release builds
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s")

# Force warnings
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  target_compile_options(SonicCore PRIVATE /W4)
else()
  target_compile_options(SonicCore PRIVATE -Wall -Wextra -pedantic)
endif()

# Use LTO if requested
if(LTO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT result)
  if(result)
    set_target_properties(Sonic SonicCore PROPERTIES INTERPROCEDURAL_OPTIMIZATION
                                               TRUE)
  endif()
endif()

# ##############################################################################
# Other compile-time defines #
# ##############################################################################

# Determine endianness
include(TestBigEndian)
test_big_endian(ENDIAN)
if(ENDIAN)
  target_compile_definitions(SonicCore PUBLIC SCP_BIG_ENDIAN)
else()
  target_compile_definitions(SonicCore PUBLIC SCP_LIL_ENDIAN)
endif()

#
# MSVC #
#

# This is messy as hell, and has been replaced by CMAKE_MSVC_RUNTIME_LIBRARY,
# but that's a very recent CMake addition, so we're still doing it this way for
# now
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND MSVC_LINK_STATIC_RUNTIME)
  # Statically-link the CRT (vcpkg static libs do this)
  foreach(flag_var CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
                   CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO)
    if(${flag_var} MATCHES "/MD")
      string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
    endif()
  endforeach()
endif()

# Do some other MSVC fixes
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  # Disable warnings that normally fire up on MSVC when using "unsafe" functions
  # instead of using MSVC's "safe" _s functions
  target_compile_definitions(SonicCore PUBLIC _CRT_SECURE_NO_WARNINGS)

  # Make it so source files are recognized as UTF-8 by MSVC
  target_compile_options(SonicCore PRIVATE "/utf-8")

  # Use `main` instead of `WinMain`
  set_target_properties(Sonic PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup")
endif()

# ##############################################################################
# Further compiler setup #
# ##############################################################################

# Force strict C99
set_target_properties(
  SonicCore Sonic
  PROPERTIES C_STANDARD 99
             C_STANDARD_REQUIRED ON
             C_EXTENSIONS OFF)

# Set debug executable name
set_target_properties(Sonic PROPERTIES DEBUG_OUTPUT_NAME "Sonic_debug")

# Set output directory
set(BUILD_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

set_target_properties(
  Sonic
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${BUILD_DIRECTORY}
             RUNTIME_OUTPUT_DIRECTORY_RELEASE ${BUILD_DIRECTORY}
             RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${BUILD_DIRECTORY}
             RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${BUILD_DIRECTORY}
             RUNTIME_OUTPUT_DIRECTORY_DEBUG ${BUILD_DIRECTORY})

# ##############################################################################
# Backends #
# ##############################################################################

# Find PkgConfig for dependency linking
find_package(PkgConfig QUIET)

if(BACKEND MATCHES "SDL2")
  target_compile_definitions(SonicCore PUBLIC SCP_BACKEND_SDL2)
  target_sources(
    SonicCore PRIVATE "src/Backend/SDL2/System.c" "src/Backend/SDL2/Render.c"
                      "src/Backend/SDL2/Input.c" "src/Backend/SDL2/Audio.c")

  # Compile and link SDL2
  if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(LIBC
        ON
        CACHE INTERNAL "" FORCE) # Needed to prevent possible 'symbol already
                                 # defined' errors
  endif()

  # 1. Look for a SDL2 package,
  # 2. look for the SDL2 component and
  # 3. fail if none can be found
  find_package(SDL2 REQUIRED CONFIG REQUIRED COMPONENTS SDL2)

  # SDL2::SDL2main may or may not be available. It is e.g. required by Windows
  # GUI applications. Only the real Sonic executable needs it -- SonicTests
  # has its own plain console main() and must not get SDL's main-shimming.
  if(TARGET SDL2::SDL2main)

    # It has an implicit dependency on SDL2 functions, so it MUST be added
    # before SDL2::SDL2 (or SDL2::SDL2-static)
    target_link_libraries(Sonic PRIVATE SDL2::SDL2main)
  endif()

  # Link to the actual SDL2 library. SDL2::SDL2 is the shared SDL library,
  # SDL2::SDL2-static is the static SDL libarary. PUBLIC so it also propagates
  # to whatever links SonicCore (Sonic itself, and SonicTests).
  target_link_libraries(SonicCore PUBLIC SDL2::SDL2)
endif()

# libm -- Sound.c computes PSG note periods from note frequencies (pow/exp2)
if(NOT WIN32)
  target_link_libraries(SonicCore PUBLIC m)
endif()

# ##############################################################################
# Resource conversion #
# ##############################################################################

# Build bin2h externally, so it isn't cross-compiled when Sonic is
# (Emscripten, cross-GCC, MinGW on Linux, etc.)
include(ExternalProject)

ExternalProject_Add(
  bin2h
  SOURCE_DIR "${CMAKE_SOURCE_DIR}/bin2h"
  DOWNLOAD_COMMAND ""
  UPDATE_COMMAND ""
  BUILD_BYPRODUCTS "<INSTALL_DIR>/bin/bin2h"
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> -DCMAKE_BUILD_TYPE=Release
  INSTALL_COMMAND ${CMAKE_COMMAND} --build . --config Release --target install)

ExternalProject_Get_Property(bin2h INSTALL_DIR)

add_executable(bin2h_tool IMPORTED)
add_dependencies(bin2h_tool bin2h)
set_target_properties(bin2h_tool PROPERTIES IMPORTED_LOCATION
                                            "${INSTALL_DIR}/bin/bin2h")

# Convert resources to header files
foreach(FILENAME IN LISTS RESOURCES)
  set(IN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/res")
  set(OUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/Resource")
  get_filename_component(DIRECTORY "${FILENAME}" DIRECTORY)
  # Safety margin: Nemesis/Kosinski/Enigma decompression speculatively reads
  # a byte or two past the logical end of their compressed input as an
  # artifact of their bit-refill logic. Harmless on real hardware
  # (contiguous ROM) but an actual out-of-bounds read here, since each
  # resource is its own fixed-size array. Map16_* (see src/Enigma.c) used to
  # be an exception to this -- pre-decompressed and padding-free, since
  # sizeof() on it was relied on elsewhere -- back when it stored plain
  # decompressed data instead of genuine compressed Enigma data.
  set(PAD_BYTES 8)
  add_custom_command(
    OUTPUT "${OUT_DIR}/${FILENAME}.h"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${OUT_DIR}/${DIRECTORY}"
    COMMAND bin2h_tool "${IN_DIR}/${FILENAME}" "${OUT_DIR}/${FILENAME}.h" ${PAD_BYTES}
    DEPENDS bin2h_tool "${IN_DIR}/${FILENAME}")
  target_sources(SonicCore PRIVATE "${OUT_DIR}/${FILENAME}.h")
endforeach()

include(ExternalProject)

ExternalProject_Add(
  clownassembler
  SOURCE_DIR "${CMAKE_SOURCE_DIR}/clownassembler"
  DOWNLOAD_COMMAND ""
  UPDATE_COMMAND ""
  BUILD_BYPRODUCTS "<INSTALL_DIR>/bin/clownassembler"
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> -DCMAKE_BUILD_TYPE=Release
  INSTALL_COMMAND ${CMAKE_COMMAND} --build . --config Release --target install)

ExternalProject_Get_Property(clownassembler INSTALL_DIR)

add_executable(clownassembler_tool IMPORTED)
add_dependencies(clownassembler_tool clownassembler)
set_target_properties(
  clownassembler_tool PROPERTIES IMPORTED_LOCATION
                                 "${INSTALL_DIR}/bin/clownassembler")

# Assemble asm code
foreach(FILENAME IN LISTS ASM)
  set(IN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/asm")
  set(OUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/res")
  get_filename_component(DIRECTORY "${FILENAME}.asm" DIRECTORY)
  add_custom_command(
    OUTPUT "${OUT_DIR}/${FILENAME}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${OUT_DIR}/${DIRECTORY}"
    COMMAND clownassembler_tool -i "${IN_DIR}/${FILENAME}.asm" -o
            "${OUT_DIR}/${FILENAME}"
    DEPENDS clownassembler_tool "${IN_DIR}/${FILENAME}.asm")
  target_sources(SonicCore PRIVATE "${OUT_DIR}/${FILENAME}")
endforeach()

# ##############################################################################
# Music/SFX (SMPS2ASM) #
# ##############################################################################
# smps2asmc translates a SMPS2ASM-format .asm song/SFX file into a small C
# program that #includes smps2asmc/smps.h (a C dialect of smps2asm.asm,
# specialized to the real Sonic 1 sound driver -- see that header's own
# comment) and calls its functions in the same sequence as the original
# file's macro invocations. Building *that* generated program and running it
# produces the actual resource header -- so each song needs three steps:
# translate, build, run. See add_smps_song() below.
#
# NOTE: unlike bin2h/clownassembler (built via ExternalProject_Add
# specifically so they're always host-native even when Sonic itself is being
# cross-compiled), smps2asmc and each per-song "song builder" program below
# are currently built as ordinary add_executable() targets, which use
# whatever compiler the current build is configured for. That's fine for a
# native build (this project's only configuration so far) but would need
# the same ExternalProject_Add isolation bin2h/clownassembler have before
# this pipeline could be trusted under cross-compilation.

add_executable(smps2asmc_tool "smps2asmc/smps2asmc.c")
set_target_properties(smps2asmc_tool PROPERTIES OUTPUT_NAME smps2asmc)

# Translates, builds, and runs one song/SFX .asm file, adding its resulting
# resource header to SonicCore. ASM_FILE is relative to res/Music/;
# ARRAY_NAME becomes both the generated C array's name and the output
# header's filename (Resource/Music/${ARRAY_NAME}.h).
function(add_smps_song ASM_FILE ARRAY_NAME)
  set(IN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/res/Music")
  set(OUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/Resource/Music")
  set(GEN_C "${CMAKE_CURRENT_BINARY_DIR}/smps_gen/${ARRAY_NAME}.gen.c")
  set(OUT_H "${OUT_DIR}/${ARRAY_NAME}.h")

  if(FIX_BUGS)
    set(SONG_FIX_BUGS 1)
  else()
    set(SONG_FIX_BUGS 0)
  endif()

  add_custom_command(
    OUTPUT "${GEN_C}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/smps_gen"
    COMMAND smps2asmc_tool "${IN_DIR}/${ASM_FILE}" "${GEN_C}" "${ARRAY_NAME}" "${OUT_H}" "${SONG_FIX_BUGS}"
    DEPENDS smps2asmc_tool "${IN_DIR}/${ASM_FILE}")

  add_executable(songbuild_${ARRAY_NAME} "${GEN_C}" "${CMAKE_SOURCE_DIR}/smps2asmc/smps.c")
  target_include_directories(songbuild_${ARRAY_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/smps2asmc")

  add_custom_command(
    OUTPUT "${OUT_H}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${OUT_DIR}"
    COMMAND songbuild_${ARRAY_NAME}
    DEPENDS songbuild_${ARRAY_NAME}
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

  target_sources(SonicCore PRIVATE "${OUT_H}")
endfunction()

# Full music/SFX catalog. res/Music/ also contains "E0 - Spin Dash Rev.asm",
# deliberately NOT wired up here: it's a SonicDriverVer=2 test file, and
# smps.h/smps.c only implement driver 1 (see smps.h's own comment).
add_smps_song("Mus81 - GHZ.asm" Mus81_GHZ)
add_smps_song("Mus82 - LZ.asm" Mus82_LZ)
add_smps_song("Mus83 - MZ.asm" Mus83_MZ)
add_smps_song("Mus84 - SLZ.asm" Mus84_SLZ)
add_smps_song("Mus85 - SYZ.asm" Mus85_SYZ)
add_smps_song("Mus86 - SBZ.asm" Mus86_SBZ)
add_smps_song("Mus87 - Invincibility.asm" Mus87_Invincibility)
add_smps_song("Mus88 - Extra Life.asm" Mus88_Extra_Life)
add_smps_song("Mus89 - Special Stage.asm" Mus89_Special_Stage)
add_smps_song("Mus8A - Title Screen.asm" Mus8A_Title_Screen)
add_smps_song("Mus8B - Ending.asm" Mus8B_Ending)
add_smps_song("Mus8C - Boss.asm" Mus8C_Boss)
add_smps_song("Mus8D - FZ.asm" Mus8D_FZ)
add_smps_song("Mus8E - Sonic Got Through.asm" Mus8E_Sonic_Got_Through)
add_smps_song("Mus8F - Game Over.asm" Mus8F_Game_Over)
add_smps_song("Mus90 - Continue Screen.asm" Mus90_Continue_Screen)
add_smps_song("Mus91 - Credits.asm" Mus91_Credits)
add_smps_song("Mus92 - Drowning.asm" Mus92_Drowning)
add_smps_song("Mus93 - Get Emerald.asm" Mus93_Get_Emerald)

add_smps_song("SndA0 - Jump.asm" SndA0_Jump)
add_smps_song("SndA1 - Lamppost.asm" SndA1_Lamppost)
add_smps_song("SndA2.asm" SndA2)
add_smps_song("SndA3 - Death.asm" SndA3_Death)
add_smps_song("SndA4 - Skid.asm" SndA4_Skid)
add_smps_song("SndA5.asm" SndA5)
add_smps_song("SndA6 - Hit Spikes.asm" SndA6_Hit_Spikes)
add_smps_song("SndA7 - Push Block.asm" SndA7_Push_Block)
add_smps_song("SndA8 - SS Goal.asm" SndA8_SS_Goal)
add_smps_song("SndA9 - SS Item.asm" SndA9_SS_Item)
add_smps_song("SndAA - Splash.asm" SndAA_Splash)
add_smps_song("SndAB.asm" SndAB)
add_smps_song("SndAC - Hit Boss.asm" SndAC_Hit_Boss)
add_smps_song("SndAD - Get Bubble.asm" SndAD_Get_Bubble)
add_smps_song("SndAE - Fireball.asm" SndAE_Fireball)
add_smps_song("SndAF - Shield.asm" SndAF_Shield)
add_smps_song("SndB0 - Saw.asm" SndB0_Saw)
add_smps_song("SndB1 - Electric.asm" SndB1_Electric)
add_smps_song("SndB2 - Drown Death.asm" SndB2_Drown_Death)
add_smps_song("SndB3 - Flamethrower.asm" SndB3_Flamethrower)
add_smps_song("SndB4 - Bumper.asm" SndB4_Bumper)
add_smps_song("SndB5 - Ring.asm" SndB5_Ring)
add_smps_song("SndB6 - Spikes Move.asm" SndB6_Spikes_Move)
add_smps_song("SndB7 - Rumbling.asm" SndB7_Rumbling)
add_smps_song("SndB8.asm" SndB8)
add_smps_song("SndB9 - Collapse.asm" SndB9_Collapse)
add_smps_song("SndBA - SS Glass.asm" SndBA_SS_Glass)
add_smps_song("SndBB - Door.asm" SndBB_Door)
add_smps_song("SndBC - Teleport.asm" SndBC_Teleport)
add_smps_song("SndBD - ChainStomp.asm" SndBD_ChainStomp)
add_smps_song("SndBE - Roll.asm" SndBE_Roll)
add_smps_song("SndBF - Get Continue.asm" SndBF_Get_Continue)
add_smps_song("SndC0 - Basaran Flap.asm" SndC0_Basaran_Flap)
add_smps_song("SndC1 - Break Item.asm" SndC1_Break_Item)
add_smps_song("SndC2 - Drown Warning.asm" SndC2_Drown_Warning)
add_smps_song("SndC3 - Giant Ring.asm" SndC3_Giant_Ring)
add_smps_song("SndC4 - Bomb.asm" SndC4_Bomb)
add_smps_song("SndC5 - Cash Register.asm" SndC5_Cash_Register)
add_smps_song("SndC6 - Ring Loss.asm" SndC6_Ring_Loss)
add_smps_song("SndC7 - Chain Rising.asm" SndC7_Chain_Rising)
add_smps_song("SndC8 - Burning.asm" SndC8_Burning)
add_smps_song("SndC9 - Hidden Bonus.asm" SndC9_Hidden_Bonus)
add_smps_song("SndCA - Enter SS.asm" SndCA_Enter_SS)
add_smps_song("SndCB - Wall Smash.asm" SndCB_Wall_Smash)
add_smps_song("SndCC - Spring.asm" SndCC_Spring)
add_smps_song("SndCD - Switch.asm" SndCD_Switch)
add_smps_song("SndCE - Ring Left Speaker.asm" SndCE_Ring_Left_Speaker)
add_smps_song("SndCF - Signpost.asm" SndCF_Signpost)
add_smps_song("SndD0 - Waterfall.asm" SndD0_Waterfall)

if(SPLASH)
  add_smps_song("Mus94 - SSRG.asm" Mus94_SSRG)
endif()

# ##############################################################################
# Tests #
# ##############################################################################

if(BUILD_TESTS)
  enable_testing()

  # Unit tests: link the exact same compiled game logic (SonicCore) as the
  # real Sonic executable, exercising real production functions directly
  # (see tests/test_plc.c, tests/test_leveldraw.c) rather than reimplementing
  # their logic.
  add_executable(SonicTests "tests/test_main.c" "tests/test_plc.c"
                             "tests/test_leveldraw.c")
  target_link_libraries(SonicTests PRIVATE SonicCore)
  set_target_properties(SonicTests PROPERTIES C_STANDARD 99
                                               C_STANDARD_REQUIRED ON
                                               C_EXTENSIONS OFF)

  # Headless smoke runner: same entry point as the real game, but with
  # --zone/--act CLI injection to skip straight into a level (see
  # tests/smoke_main.c). Also handy for manual debugging -- no more
  # hand-editing fprintf() calls into GM_Level.c/PLC.c and remembering to
  # revert them afterward.
  add_executable(SonicSmoke "tests/smoke_main.c")
  target_link_libraries(SonicSmoke PRIVATE SonicCore)
  set_target_properties(SonicSmoke PROPERTIES C_STANDARD 99
                                               C_STANDARD_REQUIRED ON
                                               C_EXTENSIONS OFF)

  # Demo recorder: play a level/special stage for real (or via the --ai
  # control hook) and record the input to a demo file in the same format
  # Demo.c plays back -- see tests/demo_record_main.c. Interactive tool, not
  # registered as a ctest.
  add_executable(SonicDemoRecord "tests/demo_record_main.c" "tests/ai_pipe.c"
                                  "tests/timestamp.c")
  target_link_libraries(SonicDemoRecord PRIVATE SonicCore)
  set_target_properties(SonicDemoRecord PROPERTIES C_STANDARD 99
                                                     C_STANDARD_REQUIRED ON
                                                     C_EXTENSIONS OFF)

  # Default --out location: bindir/demos, alongside the built executables,
  # so recordings land somewhere predictable regardless of the working
  # directory SonicDemoRecord is invoked from.
  set(DEMO_OUTPUT_DIR "${BUILD_DIRECTORY}/demos")
  file(MAKE_DIRECTORY "${DEMO_OUTPUT_DIR}")
  target_compile_definitions(SonicDemoRecord PRIVATE DEMO_OUTPUT_DIR="${DEMO_OUTPUT_DIR}")

  # Joypad test tool: a small SDL2 window showing a row of icons (the four
  # D-pad directions, then A/B/C/Start) that light up while the matching
  # button is held -- see tests/joypad_test_main.c. Reads input through the
  # same Joypad_GetState1()/Input_HandleEvents() path the real game uses,
  # so it doubles as a manual sanity check for the keyboard scheme and any
  # connected SDL_GameController (including Steam Input). Interactive tool,
  # not registered as a ctest.
  add_executable(SonicJoypadTest "tests/joypad_test_main.c")
  target_link_libraries(SonicJoypadTest PRIVATE SonicCore)
  if(NOT WIN32)
    target_link_libraries(SonicJoypadTest PRIVATE m)
  endif()
  set_target_properties(SonicJoypadTest PROPERTIES C_STANDARD 99
                                                     C_STANDARD_REQUIRED ON
                                                     C_EXTENSIONS OFF)

  # Sound trace tool: loads a real song (by sound ID, default $81 = GHZ) and
  # logs every note/duration event Sound.c's TickChannel decodes to stderr,
  # for direct comparison against the song's own .asm source. Usage:
  # SonicSoundTrace [sound_id_hex] [frames]
  add_executable(SonicSoundTrace "tests/sound_trace_main.c")
  target_link_libraries(SonicSoundTrace PRIVATE SonicCore)

  set_target_properties(
    SonicTests SonicSmoke SonicDemoRecord SonicJoypadTest SonicSoundTrace
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${BUILD_DIRECTORY}
               RUNTIME_OUTPUT_DIRECTORY_RELEASE ${BUILD_DIRECTORY}
               RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${BUILD_DIRECTORY}
               RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${BUILD_DIRECTORY}
               RUNTIME_OUTPUT_DIRECTORY_DEBUG ${BUILD_DIRECTORY})

  add_test(NAME unit_tests COMMAND SonicTests)

  # One headless smoke test per zone. "Still running when the timeout hits"
  # is success (the game loop never exits on its own) -- a crash, an early
  # exit, or an ASan abort (build with -DSANITIZE=ON for that coverage) is a
  # failure. See tests/RunSmoke.cmake.
  set(SMOKE_TARGETS "0:GHZ" "1:LZ" "2:MZ" "3:SLZ" "4:SYZ" "5:SBZ")
  foreach(ENTRY IN LISTS SMOKE_TARGETS)
    string(REPLACE ":" ";" PAIR "${ENTRY}")
    list(GET PAIR 0 ZONE_NUM)
    list(GET PAIR 1 ZONE_NAME)
    add_test(
      NAME smoke_${ZONE_NAME}
      COMMAND
        ${CMAKE_COMMAND} -DTEST_EXE=$<TARGET_FILE:SonicSmoke>
        "-DTEST_ARGS=--zone;${ZONE_NUM}" -DTEST_TIMEOUT=15 -P
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunSmoke.cmake")
    set_tests_properties(smoke_${ZONE_NAME} PROPERTIES TIMEOUT 30)
  endforeach()
endif()
