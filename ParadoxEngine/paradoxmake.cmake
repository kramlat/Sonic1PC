# ParadoxEngine: what paradoxmakefile.yml cannot say yet (backends, the SMPS library and its tools). ${PM_DIR} is this project's folder.
set_target_properties(ParadoxEngine PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
set_target_properties(ParadoxEngine PROPERTIES
                      LIBRARY_OUTPUT_DIRECTORY "${BUILD_DIRECTORY}/$<CONFIG>"
                      RUNTIME_OUTPUT_DIRECTORY "${BUILD_DIRECTORY}/$<CONFIG>")
install(TARGETS ParadoxEngine LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

# Backends #
# ##############################################################################

# Find PkgConfig for dependency linking
find_package(PkgConfig QUIET)

if(TRUE)
  target_compile_definitions(ParadoxEngine PUBLIC SCP_BACKEND_SDL2)
  target_sources(
    ParadoxEngine PRIVATE "${PM_DIR}/src/Backend/SDL2/System.c" "${PM_DIR}/src/Backend/SDL2/Render.c"
                      "${PM_DIR}/src/Backend/SDL2/Input.c")

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


  # Link to the actual SDL2 library. SDL2::SDL2 is the shared SDL library,
  # SDL2::SDL2-static is the static SDL libarary. PUBLIC so it also propagates
  # to whatever links ParadoxEngine (Sonic itself, and SonicTests).
  target_link_libraries(ParadoxEngine PUBLIC SDL2::SDL2)

  # The window: a QMainWindow (menu bar) with the game frame drawn as a texture on a
  # QOpenGLWidget; see engine/Backend/Qt/QtHost.h.
  find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGLWidgets Multimedia)
  find_package(yaml-cpp REQUIRED) # the settings file (engine/Backend/Qt/Settings.cpp)
  target_sources(ParadoxEngine PRIVATE "${PM_DIR}/src/Backend/Qt/QtHost.cpp" "${PM_DIR}/src/Backend/Qt/QtHost.h"
                                "${PM_DIR}/src/Backend/Qt/ConsoleDrawer.cpp" "${PM_DIR}/src/Backend/Qt/ConsoleDrawer.h"
                                "${PM_DIR}/src/Backend/Qt/QtAudio.cpp" "${PM_DIR}/src/Backend/Qt/QtAudio.h"
                                "${PM_DIR}/src/Backend/Qt/Settings.cpp" "${PM_DIR}/src/Backend/Qt/Settings.h"
                                "${PM_DIR}/src/Backend/Qt/DemoTools.cpp" "${PM_DIR}/src/Backend/Qt/DemoTools.h"
                                "${PM_DIR}/src/Backend/Qt/DebugViewers.cpp" "${PM_DIR}/src/Backend/Qt/DebugViewers.h"
                                "${PM_DIR}/src/Backend/Qt/SmpsInspector.cpp" "${PM_DIR}/src/Backend/Qt/SmpsInspector.h"
                                "${PM_DIR}/src/Backend/Qt/HelpWindow.cpp" "${PM_DIR}/src/Backend/Qt/HelpWindow.h"
                                "${PM_DIR}/src/Backend/Qt/ControlsDialog.cpp" "${PM_DIR}/src/Backend/Qt/ControlsDialog.h")
  target_link_libraries(ParadoxEngine PUBLIC Qt6::Widgets Qt6::OpenGLWidgets Qt6::Multimedia yaml-cpp::yaml-cpp)
  target_compile_definitions(ParadoxEngine PRIVATE SONIC_VERSION="${PROJECT_VERSION}")

  # Help > About is KDE's standard About dialog (KAboutApplicationDialog) where the KDE Frameworks are installed, and a plain
  # message box where they are not (Windows builds, for one).
  find_package(KF6CoreAddons QUIET)
  find_package(KF6XmlGui QUIET)
  if(KF6CoreAddons_FOUND AND KF6XmlGui_FOUND)
    message(STATUS "KDE Frameworks found: Help > About uses KAboutApplicationDialog")
    target_link_libraries(ParadoxEngine PUBLIC KF6::CoreAddons KF6::XmlGui)
    target_compile_definitions(ParadoxEngine PRIVATE SONIC_HAVE_KDE_ABOUT)
  endif()
endif()

# libm -- Sound.c computes PSG note periods from note frequencies (pow/exp2)
if(NOT WIN32)
  target_link_libraries(ParadoxEngine PUBLIC m)
endif()


# ParadoxSMPS (JSON song/SFX format) #
# ##############################################################################
# libparadoxsmps (libparadoxsmps/json.c + compiler.c) is the ONE canonical
# ParadoxSMPS compiler -- both this real build (via the jsonc2h_tool CLI
# below) and ParadoxComposer (linked directly) use the exact same code, so
# there is no second hand-ported implementation left to drift out of sync
# (the retired smps2asmc/.asm pipeline below this comment used to be the
# real build's source of truth; ParadoxComposer separately carried its own
# Qt/C++ port -- "a C based converter and compiler can be hooked into by the
# daw directly and guarantee the same code quality consistently").
#
# jsonc2h compiles one .jsonc straight to a header in a single step -- no
# intermediate .asm, no generated-C song-builder program to build and run,
# unlike the old three-step translate/build/run pipeline. Same relationship
# moc has to a Qt header.
add_library(paradoxsmps SHARED
  "${PM_DIR}/libparadoxsmps/json.c"
  "${PM_DIR}/libparadoxsmps/json.h"
  "${PM_DIR}/libparadoxsmps/compiler.c"
  "${PM_DIR}/libparadoxsmps/compiler.h"
  "${PM_DIR}/libparadoxsmps/converter.c"
  "${PM_DIR}/libparadoxsmps/converter.h")
set_target_properties(paradoxsmps PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
                      LIBRARY_OUTPUT_DIRECTORY "${BUILD_DIRECTORY}/$<CONFIG>" RUNTIME_OUTPUT_DIRECTORY "${BUILD_DIRECTORY}/$<CONFIG>")
install(TARGETS paradoxsmps LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

# ParadoxEngine links this directly now too -- the SMPS runtime engine (Sound.c)
# walks PJValue JSON trees straight at playback time (see the "SMPS runtime:
# walk JSON directly" plan) instead of a precompiled byte stream, so the
# actual game binary needs the JSON value model at runtime, not just the
# offline compiler tools.
target_link_libraries(ParadoxEngine PUBLIC paradoxsmps)
target_include_directories(ParadoxEngine PUBLIC "${PM_DIR}/libparadoxsmps")

add_executable(jsonc2h_tool "${PM_DIR}/libparadoxsmps/jsonc2h_main.c")
target_link_libraries(jsonc2h_tool PRIVATE paradoxsmps)
target_include_directories(jsonc2h_tool PRIVATE "${PM_DIR}/libparadoxsmps")
set_target_properties(jsonc2h_tool PROPERTIES OUTPUT_NAME jsonc2h)

# jsonc2rawh: companion to jsonc2h_tool above, but embeds a song's own
# SOURCE TEXT (not compiled bytes) as a C string -- for the JSON
# tree-walking runtime engine, which parses and plays a song's PJValue
# tree directly rather than a compiled byte stream.
add_executable(jsonc2rawh_tool "${PM_DIR}/libparadoxsmps/jsonc2rawh_main.c")
target_link_libraries(jsonc2rawh_tool PRIVATE paradoxsmps)
target_include_directories(jsonc2rawh_tool PRIVATE "${PM_DIR}/libparadoxsmps")
set_target_properties(jsonc2rawh_tool PROPERTIES OUTPUT_NAME jsonc2rawh)

# asm2json: migration tool (not part of the game build), converts a real
# SMPS2ASM .asm song/SFX file to ParadoxSMPS JSONC -- see converter.c's own
# comment. Kept for re-conversion/verification against
# tools/paradoxsmps/asm_to_json.py, the same relationship jsonc2h_tool has
# to json_to_header.py.
add_executable(asm2json_tool "${PM_DIR}/libparadoxsmps/asm2json_main.c")
target_link_libraries(asm2json_tool PRIVATE paradoxsmps)
target_include_directories(asm2json_tool PRIVATE "${PM_DIR}/libparadoxsmps")

# Force warnings
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  target_compile_options(ParadoxEngine PRIVATE /W4 "/utf-8")
  target_compile_definitions(ParadoxEngine PUBLIC _CRT_SECURE_NO_WARNINGS)
else()
  target_compile_options(ParadoxEngine PRIVATE -Wall -Wextra -pedantic)
endif()

# Determine endianness
include(TestBigEndian)
test_big_endian(ENDIAN)
if(ENDIAN)
  target_compile_definitions(ParadoxEngine PUBLIC SCP_BIG_ENDIAN)
else()
  target_compile_definitions(ParadoxEngine PUBLIC SCP_LIL_ENDIAN)
endif()


# pm_add_song(TARGET IN_DIR OUT_DIR JSONC_FILE ARRAY_NAME): compiles one already-converted ParadoxSMPS .jsonc file into two headers in OUT_DIR (the compiled
# bytes, ARRAY_NAME.h, and the song's own JSON text, ARRAY_NAME_json.h), added to TARGET. ARRAY_NAME is the generated C array's name.
function(pm_add_song TARGET IN_DIR OUT_DIR JSONC_FILE ARRAY_NAME)
  set(OUT_H "${OUT_DIR}/${ARRAY_NAME}.h")

  add_custom_command(
    OUTPUT "${OUT_H}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${OUT_DIR}"
    COMMAND jsonc2h_tool "${IN_DIR}/${JSONC_FILE}" "${OUT_H}" "${ARRAY_NAME}"
    DEPENDS jsonc2h_tool "${IN_DIR}/${JSONC_FILE}"
    VERBATIM)

  target_sources(${TARGET} PRIVATE "${OUT_H}")

  # Companion raw-JSON-text header, alongside the compiled-bytes one above
  # -- Sound.c's sound_table_json[] (parallel to sound_table[]) includes
  # these so the sound test (and anything else, eventually) can play a
  # song through the JSON engine instead of the byte-VM.
  set(OUT_JSON_H "${OUT_DIR}/${ARRAY_NAME}_json.h")
  add_custom_command(
    OUTPUT "${OUT_JSON_H}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${OUT_DIR}"
    COMMAND jsonc2rawh_tool "${IN_DIR}/${JSONC_FILE}" "${OUT_JSON_H}" "${ARRAY_NAME}"
    DEPENDS jsonc2rawh_tool "${IN_DIR}/${JSONC_FILE}"
    VERBATIM)

  target_sources(${TARGET} PRIVATE "${OUT_JSON_H}")
endfunction()
