cmake_minimum_required(VERSION 3.16) # Qt6's CMake package/AUTOMOC requires >= 3.16

# ##############################################################################
# Options #
# ##############################################################################
# The game is a Qt application: a QMainWindow with a menu bar, the frame drawn as a texture on a
# QOpenGLWidget (engine/Backend/Qt). SDL2 is still used for audio, gamepads and the software renderer
# that draws the frame and its overlays. (The old plain-SDL2 window build has been dropped.)
option(JAPANESE "Compile Japanese ROM" OFF)
option(FIX_BUGS "Fix bugs (completely screwed up code, not gameplay bugs)" OFF)

# Two custom CMAKE_BUILD_TYPEs (alongside the usual Debug/Release/etc), both
# defaulting SPLASH (below) to ON -- boot straight into the SSRG (Sonic
# Stuff Research Group) screen
# instead of the title screen, for a polished "preview build" look:
#   - "Premier": Release-level optimization, but deliberately NOT defining
#     NDEBUG, so every #ifndef NDEBUG debug-cheat/level-select shortcut
#     (GM_Level.c, GM_Special.c, GM_Title.c) stays active -- for OBS/YouTube
#     footage where quickly jumping to a specific level/feature to show off
#     matters more than hiding debug-only access.
#   - "Showcase": plain Release build (NDEBUG defined, no debug-cheat
#     access) with the same SSRG-first default -- for release-quality
#     public builds/trailers that don't need cheat access.
# Only takes effect on this variable's FIRST configure in a given build dir,
# same as any other option() default -- use a separate build directory per
# CMAKE_BUILD_TYPE (as this project already does for Debug vs Release).
if(CMAKE_BUILD_TYPE STREQUAL "Premier" OR CMAKE_BUILD_TYPE STREQUAL "Showcase")
  set(SPLASH_DEFAULT ON)
else()
  set(SPLASH_DEFAULT OFF)
endif()
option(SPLASH "Enable the SSRG (Sonic Stuff Research Group) splash screen (for my own demo releases)" ${SPLASH_DEFAULT})

option(SANITIZE "Enable sanitization" OFF)
option(LTO "Enable link-time optimization" OFF)
option(BUILD_TESTS "Build SonicTests (unit tests) and the per-zone smoke tests (the real Sonic executable with --zone), and register them with ctest" ON)
option(MSVC_LINK_STATIC_RUNTIME
       "Link the static MSVC runtime library (Visual Studio only)" OFF)

#
# Setup #
#

# Define project, source, and includes
