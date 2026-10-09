# What paradoxmakefile.yml cannot say yet, for Sonic 2: Sonic 1's build settings, as far as the reused code needs them. ${PM_DIR} is this project's folder.
# Nick Arcade's VRAM layout for the common art (its PLC_Main): the objects of Sonic 1 that Sonic 2 reuses take their tiles from these (Constants.h lets a game override them)
target_compile_definitions(Sonic2Core PUBLIC ZONE_SLOTS=17 SOUND_IDS_CUSTOM PALETTE_IDS_CUSTOM ArtTile_Lamppost=0x47C ArtTile_Signpost=0x434 ArtTile_Ring=0x6BC ArtTile_Points=0x4AC ArtTile_Shield=0x4BE ArtTile_Invincibility=0x4DE ArtTile_Spikes=0x434 ArtTile_Spring_Horizontal=0x4A8 ArtTile_Spring_Vertical=0x4B8 ArtTile_SpringUp=0x45C ArtTile_SpringSide=0x470 ArtTile_SpringDiag=0x43C SONIC_DPLC_SIZE=0x400 PLC_IDS_EXTRA)
if(JAPANESE)
  target_compile_definitions(Sonic2Core PUBLIC SCP_JP)
endif()
# Premier and Showcase builds: the countdown screen of Sonic 1 (Tools > Countdown), over Sonic 2's own sounds (its default song is its Emerald Hill's: mus_GHZ = 2, and the SSRG splash is not in Sonic 2 yet)
if(SPLASH)
  target_compile_definitions(Sonic2Core PUBLIC SCP_COUNTDOWN COUNTDOWN_DEFAULT_MUSIC=2)
  target_sources(Sonic2Core PRIVATE "${PM_DIR}/../Sonic The Hedgehog/src/GM_Countdown.c")
endif()
if(FIX_BUGS)
  target_compile_definitions(Sonic2Core PUBLIC SCP_FIX_BUGS)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  target_compile_options(Sonic2Core PRIVATE /W4 "/utf-8")
else()
  target_compile_options(Sonic2Core PRIVATE -Wall -Wextra -pedantic)
endif()

set_target_properties(Sonic2Core Sonic2 PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
set_target_properties(Sonic2 PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${BUILD_DIRECTORY}/$<CONFIG>")
set_target_properties(Sonic2 PROPERTIES BUILD_RPATH "$ORIGIN" INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
if(TARGET SDL2::SDL2main)
  target_link_libraries(Sonic2 PRIVATE SDL2::SDL2main)
endif()
install(TARGETS Sonic2 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# ##############################################################################
# Tests #
# ##############################################################################
# Sonic 2's unit tests, with Sonic 1's test.h: they link the exact same compiled game logic (Sonic2Core) as the real Sonic2 executable. Not built in Showcase builds (no developer tools there).
if(BUILD_TESTS AND NOT CMAKE_BUILD_TYPE STREQUAL "Showcase")
  enable_testing()
  add_executable(Sonic2Tests "${PM_DIR}/tests/test_main.c" "${PM_DIR}/tests/test_leveldata.c" "${PM_DIR}/tests/test_objects.c" "${PM_DIR}/tests/test_solid.c" "${PM_DIR}/tests/test_htzquake.c" "${PM_DIR}/tests/test_htzfloor.c" "${PM_DIR}/tests/test_htzfireball.c" "${PM_DIR}/tests/test_htzbackground.c"
                             "${PM_DIR}/tests/test_water.c" "${PM_DIR}/tests/test_signpost.c" "${PM_DIR}/tests/test_demo.c"
                             "${PM_DIR}/tests/test_cpzobjects.c" "${PM_DIR}/tests/test_nghzobjects.c" "${PM_DIR}/tests/test_htzobjects.c" "${PM_DIR}/tests/test_camera.c" "${PM_DIR}/tests/test_dhzobjects.c" "${PM_DIR}/tests/test_oozobjects.c" "${PM_DIR}/tests/test_scrollblock.c" "${PM_DIR}/tests/test_oscillation.c" "${PM_DIR}/../Sonic The Hedgehog/tests/test_math.c" "${PM_DIR}/../Sonic The Hedgehog/tests/test_palette.c" "${PM_DIR}/../Sonic The Hedgehog/tests/test_vdp8bpp.c"
                             "${PM_DIR}/../Sonic The Hedgehog/tests/test_compression.c")
  target_include_directories(Sonic2Tests PRIVATE "${PM_DIR}/../Sonic The Hedgehog/tests")
  link_game(Sonic2Tests)
  set_target_properties(Sonic2Tests PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
  set_target_properties(Sonic2Tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${BUILD_DIRECTORY}/$<CONFIG>")
  add_test(NAME unit_tests_sonic2 COMMAND Sonic2Tests)
endif()
