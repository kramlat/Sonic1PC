# Compiler flags #
# ##############################################################################

# Region
if(JAPANESE)
  target_compile_definitions(Sonic1Core PUBLIC SCP_JP)
endif()

# Bug fixes
if(FIX_BUGS)
  target_compile_definitions(Sonic1Core PUBLIC SCP_FIX_BUGS)
endif()

# Splash
if(SPLASH)
  target_compile_definitions(Sonic1Core PUBLIC SCP_SPLASH SCP_COUNTDOWN)
  target_sources(Sonic1Core PRIVATE "${PM_DIR}/src/GM_SSRG.c" "${PM_DIR}/src/GM_SSRG.h" "${PM_DIR}/src/GM_Countdown.c" "${PM_DIR}/src/GM_Countdown.h")
  list(
    APPEND
    PM_RESOURCES
    "SSRG/ArtLink"
    "SSRG/ArtMain"
    "SSRG/ArtSonic"
    "SSRG/ArtSquare"
    "SSRG/MapLink"
    "SSRG/MapMain"
    "SSRG/MapSquare")
endif()

# Force warnings
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  target_compile_options(Sonic1Core PRIVATE /W4)
else()
  target_compile_options(Sonic1Core PRIVATE -Wall -Wextra -pedantic)
endif()

# Use LTO if requested
if(LTO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT result)
  if(result)
    set_target_properties(Sonic ParadoxEngine Sonic1Core PROPERTIES INTERPROCEDURAL_OPTIMIZATION
                                               TRUE)
  endif()
endif()

# ##############################################################################
# Other compile-time defines #
# ##############################################################################

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
  target_compile_definitions(ParadoxEngine PUBLIC _CRT_SECURE_NO_WARNINGS)

  # Make it so source files are recognized as UTF-8 by MSVC
  target_compile_options(ParadoxEngine PRIVATE "/utf-8")
  target_compile_options(Sonic1Core PRIVATE "/utf-8")

  # Use `main` instead of `WinMain`
  set_target_properties(Sonic PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup")
endif()

# ##############################################################################

set_target_properties(
  Sonic1Core Sonic
  PROPERTIES C_STANDARD 99
             C_STANDARD_REQUIRED ON
             C_EXTENSIONS OFF)


set_target_properties(Sonic PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${BUILD_DIRECTORY}/$<CONFIG>")
set_target_properties(Sonic PROPERTIES BUILD_RPATH "$ORIGIN" INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")

# Handbook #
# ##############################################################################
# doc/index.docbook is the manual in KDE's DocBook form (what KDE's Help Center shows: install it, with its screenshots, in
# share/doc/HTML/en/sonic1pc). The game's own Help window shows its HTML version, which xsltproc builds into doc/html.
find_program(XSLTPROC xsltproc)
set(HB_DOC "${PM_DIR}/doc")
set(S1_PKG "${PM_DIR}/packaging")
if(XSLTPROC)
  file(GLOB HANDBOOK_SCREENSHOTS "${PM_DIR}/doc/screenshots/*.png")
  add_custom_command(
    OUTPUT "${PM_DIR}/doc/html/index.html"
    COMMAND ${CMAKE_COMMAND} "-DDOC_DIR=${HB_DOC}" -DXSLTPROC=${XSLTPROC} -P "${HB_DOC}/BuildHandbook.cmake"
    DEPENDS "${HB_DOC}/index.docbook" "${HB_DOC}/handbook-html.xsl" "${HB_DOC}/BuildHandbook.cmake" ${HANDBOOK_SCREENSHOTS}
    VERBATIM)
  add_custom_target(handbook ALL DEPENDS "${PM_DIR}/doc/html/index.html")
else()
  message(STATUS "xsltproc not found: the game's Help window will have no handbook to show")
endif()

# The game itself: <prefix>/bin/Sonic (configure with -DCMAKE_INSTALL_PREFIX=/usr for /usr/bin; the data it needs is built in, and its
# settings live in the user's own data directory).
install(TARGETS Sonic RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
# The desktop entry puts the game in the Games menu of Plasma and other desktops (its icon, sonic1pc, is installed from packaging/icons/)
install(FILES "${S1_PKG}/sonic1pc.desktop" DESTINATION share/applications)
install(DIRECTORY "${S1_PKG}/icons/" DESTINATION share/icons OPTIONAL)
install(FILES "${HB_DOC}/index.docbook" DESTINATION share/doc/HTML/en/sonic1pc)
install(DIRECTORY "${HB_DOC}/screenshots" DESTINATION share/doc/HTML/en/sonic1pc)
if(XSLTPROC)
  install(DIRECTORY "${HB_DOC}/html/" DESTINATION share/doc/sonic1pc/html OPTIONAL)
endif()

# ##############################################################################

set_target_properties(asm2json_tool PROPERTIES OUTPUT_NAME asm2json)

# Compiles one already-converted ParadoxSMPS .jsonc file, adding its
# resulting resource header to Sonic1Core. JSONC_FILE is relative to
# tools/paradoxsmps/converted/ (e.g. "music/Mus81 - GHZ.jsonc"); ARRAY_NAME
# becomes both the generated C array's name and the output header's
# filename (Resource/Music/${ARRAY_NAME}.h).

# Full music/SFX catalog. The spin dash rev is Sonic 2's (res/Music/E0 - Spin Dash Rev.asm); it only uses driver 1
# commands, and its climbing pitch is the driver's own doing (see Sound.c's PrepareSpindashRev).


if(SPLASH)
  pm_add_song(Sonic1Core "${PM_DIR}/res/SMPS/converted" "${PM_DIR}/src/Resource/Music" "music/Mus94 - SSRG.jsonc" Mus94_SSRG)
endif()

# ##############################################################################
# Tests #
# ##############################################################################

if(BUILD_TESTS)
  enable_testing()

  # Showcase only wants the tools relevant to producing/recording pre-scripted
  # footage (Sonic's --demo to play back scripted movement; demos are recorded in
  # the main Sonic app's Tools menu) plus the main Sonic binary itself -- not the rest of the
  # developer-diagnostic toolkit (unit tests, joypad/sound-trace tools, the
  # FM voice editor, etc), which have no role in producing showcase footage.
  if(CMAKE_BUILD_TYPE STREQUAL "Showcase")
    set(SONIC_FULL_DEV_TOOLS OFF)
  else()
    set(SONIC_FULL_DEV_TOOLS ON)
  endif()

  if(SONIC_FULL_DEV_TOOLS)
    # Unit tests: link the exact same compiled game logic (Sonic1Core) as the
    # real Sonic executable, exercising real production functions directly
    # (see tests/test_plc.c, tests/test_leveldraw.c) rather than
    # reimplementing their logic.
    add_executable(SonicTests "${PM_DIR}/tests/test_main.c" "${PM_DIR}/tests/test_plc.c"
                               "${PM_DIR}/tests/test_leveldraw.c" "${PM_DIR}/tests/test_levelcollision.c"
                               "${PM_DIR}/tests/test_prison.c" "${PM_DIR}/tests/test_lz3wall.c" "${PM_DIR}/tests/test_fmalgorithm.c" "${PM_DIR}/tests/test_objectsmanager.c" "${PM_DIR}/tests/test_ringsmanager.c" "${PM_DIR}/tests/test_sprites.c" "${PM_DIR}/tests/test_vdpsplit.c" "${PM_DIR}/tests/test_levelplane.c"
                               "${PM_DIR}/tests/test_bosslz.c" "${PM_DIR}/tests/test_bossslz.c" "${PM_DIR}/tests/test_scrapeggman.c" "${PM_DIR}/tests/test_bossfinal.c" "${PM_DIR}/tests/test_specialstage.c" "${PM_DIR}/tests/test_ending.c" "${PM_DIR}/tests/test_points.c" "${PM_DIR}/tests/test_lzconveyor.c"
                               "${PM_DIR}/tests/test_lzraft.c" "${PM_DIR}/tests/test_pushblock.c" "${PM_DIR}/tests/test_lzblocks.c" "${PM_DIR}/tests/test_drowncount.c" "${PM_DIR}/tests/test_roller.c" "${PM_DIR}/tests/test_spikeball.c" "${PM_DIR}/tests/test_staircase.c" "${PM_DIR}/tests/test_oscillator.c" "${PM_DIR}/tests/test_scenery.c" "${PM_DIR}/tests/test_audiomute.c" "${PM_DIR}/tests/test_demorecord.c" "${PM_DIR}/tests/test_sounddriver.c" "${PM_DIR}/tests/test_objcollision.c"
                               "${PM_DIR}/tests/test_giantring.c" "${PM_DIR}/tests/test_checkpoint.c" "${PM_DIR}/tests/test_spring.c" "${PM_DIR}/tests/test_spikes.c" "${PM_DIR}/tests/test_compression.c" "${PM_DIR}/tests/test_math.c" "${PM_DIR}/tests/test_palette.c" "${PM_DIR}/tests/test_renderflags.c" "${PM_DIR}/tests/test_vdp8bpp.c" "${PM_DIR}/tests/test_tilebank.c")
    link_game(SonicTests)
    set_target_properties(SonicTests PROPERTIES C_STANDARD 99
                                                 C_STANDARD_REQUIRED ON
                                                 C_EXTENSIONS OFF)
    set_target_properties(SonicTests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${BUILD_DIRECTORY}/$<CONFIG>")
    add_test(NAME unit_tests COMMAND SonicTests)
  endif()

  if(SONIC_FULL_DEV_TOOLS)
    # Joypad test tool: a small SDL2 window showing a row of icons (the four
    # D-pad directions, then A/B/C/Start) that light up while the matching
    # button is held -- see tests/joypad_test_main.c. Reads input through the
    # same Joypad_GetState1()/Input_HandleEvents() path the real game uses,
    # so it doubles as a manual sanity check for the keyboard scheme and any
    # connected SDL_GameController (including Steam Input). Interactive tool,
    # not registered as a ctest.
    add_executable(SonicJoypadTest "${PM_DIR}/tests/joypad_test_main.c")
    link_game(SonicJoypadTest)
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
    add_executable(SonicSoundTrace "${PM_DIR}/tests/sound_trace_main.c")
    link_game(SonicSoundTrace)

    # Renders a real song/SFX to raw 16-bit stereo PCM for offline waveform
    # inspection, independent of any live audio backend. Usage:
    # SonicSoundWav [sound_id_hex] [seconds] [out.pcm]
    add_executable(SonicSoundWav "${PM_DIR}/tests/sound_wav_main.c")
    link_game(SonicSoundWav)

    # Throwaway smoke test for SMPS driver-version-3 (Sonic 3/Flamedriver-
    # compatible coordination flags) -- see the SMPS driver-version-3 plan's
    # own verification section. Not registered as a ctest.
    add_executable(V3FlagTest "${PM_DIR}/tests/v3_flag_test_main.c")
    link_game(V3FlagTest)

    # A/B verification for the "SMPS runtime: walk JSON directly" plan --
    # renders the same song through the byte-VM and the JSON engine and
    # diffs the PCM. Not registered as a ctest.
    add_executable(JsonABTest "${PM_DIR}/tests/json_ab_test_main.c")
    link_game(JsonABTest)

    # Voice isolation tool: plays a real song at real speed through the actual
    # gameplay driver, muting every FM channel except one currently loaded
    # with a chosen voice index (PSG/DAC muted entirely) -- see
    # tests/voice_isolate_main.c. Live stdin control (type a voice index +
    # Enter to switch), not registered as a ctest. Usage: SonicVoiceIsolate
    # [song_id_hex]
    add_executable(SonicVoiceIsolate "${PM_DIR}/tests/voice_isolate_main.c")
    link_game(SonicVoiceIsolate)
    target_link_libraries(SonicVoiceIsolate PRIVATE SDL2::SDL2)
    # Not C_EXTENSIONS OFF here -- needs POSIX select()/fd_set for non-blocking
    # stdin polling, which strict ISO C99 mode doesn't expose without extra
    # feature-test-macro plumbing.

    set(IMGUI_DIR "${PM_ROOT}/ParadoxEngine/contrib/imgui")
    set(NUKED_OPN2_DIR "${PM_ROOT}/ParadoxEngine/contrib/nuked-opn2")

    # ParadoxComposer: Qt6 editor/DAW for the ParadoxSMPS JSON song/SFX
    # format (tools/paradoxsmps/), replacing SonicVoiceEditor entirely --
    # see tools/paradoxcomposer/ for the individual panel source files.
    # Compiles in-process by linking libparadoxsmps directly (NativeCompiler.cpp
    # is just the QJsonObject/QByteArray boundary wrapper around it -- the
    # SAME static library jsonc2h_tool and the real CMake build use, so there
    # is only one compiler implementation anywhere in this project), and
    # plays back either a single voice directly (Backend/YM2612.h, same as
    # the old voice editor) or a full compiled song through Sonic1Core's real
    # sequencer (Sound_DebugPlayRawSong, Sound.c).
    find_package(Qt6 REQUIRED COMPONENTS Widgets)
    add_executable(ParadoxComposer
      "${PM_ROOT}/ParadoxEngine/tools/paradoxcomposer/main.cpp"
      "${PM_ROOT}/ParadoxEngine/tools/paradoxcomposer/MainWindow.cpp"
      "${PM_ROOT}/ParadoxEngine/tools/paradoxcomposer/MainWindow.h"
      "${PM_ROOT}/ParadoxEngine/tools/paradoxcomposer/SongDocument.cpp"
      "${PM_ROOT}/ParadoxEngine/tools/paradoxcomposer/SongDocument.h"
      "${PM_ROOT}/ParadoxEngine/tools/paradoxcomposer/NativeCompiler.cpp"
      "${PM_ROOT}/ParadoxEngine/tools/paradoxcomposer/NativeCompiler.h"
      "${PM_ROOT}/ParadoxEngine/tools/paradoxcomposer/AudioEngine.cpp"
      "${PM_ROOT}/ParadoxEngine/tools/paradoxcomposer/AudioEngine.h"
      "${PM_ROOT}/ParadoxEngine/tools/paradoxcomposer/SourcePanel.cpp"
      "${PM_ROOT}/ParadoxEngine/tools/paradoxcomposer/SourcePanel.h"
      "${PM_ROOT}/ParadoxEngine/tools/paradoxcomposer/BytesPanel.cpp"
      "${PM_ROOT}/ParadoxEngine/tools/paradoxcomposer/BytesPanel.h"
      "${PM_ROOT}/ParadoxEngine/tools/paradoxcomposer/VoiceBankPanel.cpp"
      "${PM_ROOT}/ParadoxEngine/tools/paradoxcomposer/VoiceBankPanel.h"
      "${PM_ROOT}/ParadoxEngine/tools/paradoxcomposer/VoiceWidgets.cpp"
      "${PM_ROOT}/ParadoxEngine/tools/paradoxcomposer/VoiceWidgets.h"
      "${PM_ROOT}/ParadoxEngine/tools/paradoxcomposer/PlaylistPanel.cpp"
      "${PM_ROOT}/ParadoxEngine/tools/paradoxcomposer/PlaylistPanel.h"
      "${PM_ROOT}/ParadoxEngine/tools/paradoxcomposer/PianoRollWidgets.cpp"
      "${PM_ROOT}/ParadoxEngine/tools/paradoxcomposer/PianoRollWidgets.h"
      "${PM_ROOT}/ParadoxEngine/tools/paradoxcomposer/PianoRollPanel.cpp"
      "${PM_ROOT}/ParadoxEngine/tools/paradoxcomposer/PianoRollPanel.h"
      "${PM_ROOT}/ParadoxEngine/tools/paradoxcomposer/WaveformVUWidget.cpp"
      "${PM_ROOT}/ParadoxEngine/tools/paradoxcomposer/WaveformVUWidget.h"
      "${PM_ROOT}/ParadoxEngine/tools/paradoxcomposer/ChannelsPanel.cpp"
      "${PM_ROOT}/ParadoxEngine/tools/paradoxcomposer/ChannelsPanel.h"
      "${PM_ROOT}/ParadoxEngine/tools/paradoxcomposer/PlaybackPanel.cpp"
      "${PM_ROOT}/ParadoxEngine/tools/paradoxcomposer/PlaybackPanel.h"
      "${PM_ROOT}/ParadoxEngine/tools/paradoxcomposer/DockableContainer.cpp"
      "${PM_ROOT}/ParadoxEngine/tools/paradoxcomposer/DockableContainer.h"
      "${PM_ROOT}/ParadoxEngine/tools/paradoxcomposer/BlockMetadata.cpp"
      "${PM_ROOT}/ParadoxEngine/tools/paradoxcomposer/BlockMetadata.h")
    link_game(ParadoxComposer)
    target_link_libraries(ParadoxComposer PRIVATE paradoxsmps SDL2::SDL2 Qt6::Widgets)
    target_compile_definitions(ParadoxComposer PRIVATE
      PARADOXCOMPOSER_REPO_ROOT="${PM_ROOT}")
    set_target_properties(ParadoxComposer PROPERTIES
      CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON
      AUTOMOC ON)

    # Throwaway correctness check, not part of the shipped GUI: compiles a
    # .jsonc to raw bytes via NativeCompiler (-> libparadoxsmps) and writes
    # them to stdout, so they can be byte-diffed against jsonc2h_tool's own
    # output on the same file -- see native_compiler_test_main.cpp.
    add_executable(NativeCompilerTest
      "${PM_ROOT}/ParadoxEngine/tools/paradoxcomposer/native_compiler_test_main.cpp"
      "${PM_ROOT}/ParadoxEngine/tools/paradoxcomposer/SongDocument.cpp"
      "${PM_ROOT}/ParadoxEngine/tools/paradoxcomposer/SongDocument.h"
      "${PM_ROOT}/ParadoxEngine/tools/paradoxcomposer/NativeCompiler.cpp"
      "${PM_ROOT}/ParadoxEngine/tools/paradoxcomposer/NativeCompiler.h")
    target_link_libraries(NativeCompilerTest PRIVATE Qt6::Core paradoxsmps)
    set_target_properties(NativeCompilerTest PROPERTIES
      CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON
      AUTOMOC ON)

    # fmcore's interactive test rig -- home-row-piano input driving a full
    # 4-operator FMVoice (fm_voice.h) through a free-form, Flowstone-style
    # patch-cable UI (see fmcore/fmcore_synth_main.cpp's own design note).
    # Evolved from an earlier single-operator-only version (fmcore_gui_main.cpp,
    # now removed/superseded). Vendors Dear ImGui (imgui/ submodule) directly,
    # independent of ParadoxComposer's Qt6 UI, but otherwise
    # has nothing to do with the engine or the game -- fmcore is a from-scratch
    # YM2612 side project, not wired into gameplay yet.
    add_executable(FMCoreSynth
      "${PM_ROOT}/ParadoxEngine/fmcore/fm_operator.c"
      "${PM_ROOT}/ParadoxEngine/fmcore/fm_operator.h"
      "${PM_ROOT}/ParadoxEngine/fmcore/fm_voice.c"
      "${PM_ROOT}/ParadoxEngine/fmcore/fm_voice.h"
      "${PM_ROOT}/ParadoxEngine/fmcore/fmcore_synth_main.cpp"
      "${IMGUI_DIR}/imgui.cpp"
      "${IMGUI_DIR}/imgui_draw.cpp"
      "${IMGUI_DIR}/imgui_tables.cpp"
      "${IMGUI_DIR}/imgui_widgets.cpp"
      "${IMGUI_DIR}/imgui_demo.cpp"
      "${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp"
      "${IMGUI_DIR}/backends/imgui_impl_sdlrenderer2.cpp")
    target_include_directories(FMCoreSynth PRIVATE "${PM_ROOT}/ParadoxEngine/fmcore" "${IMGUI_DIR}" "${IMGUI_DIR}/backends")
    target_link_libraries(FMCoreSynth PRIVATE SDL2::SDL2)
    set_target_properties(FMCoreSynth PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
    set_source_files_properties("${PM_ROOT}/ParadoxEngine/fmcore/fm_operator.c" "${PM_ROOT}/ParadoxEngine/fmcore/fm_voice.c" PROPERTIES LANGUAGE C)

    # FMCompare: the game's FM output (fmcore) against Nuked-OPN2, a cycle-accurate YM2612, on the same
    # register writes -- see tests/fm_compare_main.c.
    # SoundSync: per-channel loop lengths of every song (channels drifting out of sync) -- see tests/sound_sync_main.c.
    add_executable(SoundSync "${PM_DIR}/tests/sound_sync_main.c")
    link_game(SoundSync)
    add_executable(FMCompare "${PM_DIR}/tests/fm_compare_main.c" "${NUKED_OPN2_DIR}/ym3438.c")
    target_include_directories(FMCompare PRIVATE "${NUKED_OPN2_DIR}")
    link_game(FMCompare)

    set_target_properties(
      SonicJoypadTest SonicSoundTrace SonicSoundWav V3FlagTest ParadoxComposer SonicVoiceIsolate FMCoreSynth FMCompare SoundSync
      PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${BUILD_DIRECTORY}/$<CONFIG>")
  endif()

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
        ${CMAKE_COMMAND} -DTEST_EXE=$<TARGET_FILE:Sonic>
        "-DTEST_ARGS=--zone;${ZONE_NUM}" -DTEST_TIMEOUT=15 -P
        "${PM_DIR}/tests/RunSmoke.cmake")
    # The smoke runs boot the real game, so Qt needs a platform plugin that works without a display.
    set_tests_properties(smoke_${ZONE_NAME} PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen;SDL_AUDIODRIVER=dummy")
  endforeach()
endif()

# SDL2::SDL2main may or may not be available. It is e.g. required by Windows GUI applications. Only the real Sonic executable needs it -- SonicTests
# has its own plain console main() and must not get SDL's main-shimming. It has an implicit dependency on SDL2 functions, so it MUST be added before
# SDL2::SDL2 (the engine's, linked by link_game).
if(TARGET SDL2::SDL2main)
  target_link_libraries(Sonic PRIVATE SDL2::SDL2main)
endif()
