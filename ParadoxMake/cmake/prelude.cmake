# ParadoxMake build policy shared by every Paradox project (included by the generated CMakeLists.txt after project()).
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(GNUInstallDirs)
set(BUILD_DIRECTORY "${PM_ROOT}/bin")

# Build bin2h externally, so it isn't cross-compiled when Sonic is
# (Emscripten, cross-GCC, MinGW on Linux, etc.)
include(ExternalProject)

ExternalProject_Add(
  bin2h
  SOURCE_DIR "${PM_ROOT}/ParadoxEngine/tools/bin2h"
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


include(ExternalProject)

ExternalProject_Add(
  clownassembler
  SOURCE_DIR "${PM_ROOT}/ParadoxEngine/contrib/clownassembler"
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

# pm_convert_resources(TARGET IN_DIR OUT_DIR files...): each file becomes a C header (bin2h) in OUT_DIR, added to TARGET.
# Safety margin: Nemesis/Kosinski/Enigma decompression speculatively reads a byte or two past the logical end of their compressed input as an
# artifact of their bit-refill logic; harmless on real hardware (contiguous ROM) but an out-of-bounds read here, since each resource is its own array.
function(pm_convert_resources TARGET IN_DIR OUT_DIR)
  foreach(FILENAME IN LISTS ARGN)
    get_filename_component(DIRECTORY "${FILENAME}" DIRECTORY)
    add_custom_command(
      OUTPUT "${OUT_DIR}/${FILENAME}.h"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${OUT_DIR}/${DIRECTORY}"
      COMMAND bin2h_tool "${IN_DIR}/${FILENAME}" "${OUT_DIR}/${FILENAME}.h" 8
      DEPENDS bin2h_tool "${IN_DIR}/${FILENAME}"
      VERBATIM)
    target_sources(${TARGET} PRIVATE "${OUT_DIR}/${FILENAME}.h")
  endforeach()
endfunction()

# pm_assemble(TARGET IN_DIR OUT_DIR names...): IN_DIR/<name>.asm is assembled with clownassembler into OUT_DIR/<name>, added to TARGET.
# Run from IN_DIR so that "include" paths (Mappings/_MapMacros.asm) resolve.
function(pm_assemble TARGET IN_DIR OUT_DIR)
  file(GLOB_RECURSE PM_ASM_INCLUDES "${IN_DIR}/_*.asm")
  foreach(FILENAME IN LISTS ARGN)
    get_filename_component(DIRECTORY "${FILENAME}.asm" DIRECTORY)
    add_custom_command(
      OUTPUT "${OUT_DIR}/${FILENAME}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${OUT_DIR}/${DIRECTORY}"
      COMMAND clownassembler_tool -i "${IN_DIR}/${FILENAME}.asm" -o "${OUT_DIR}/${FILENAME}"
      WORKING_DIRECTORY "${IN_DIR}"
      DEPENDS clownassembler_tool "${IN_DIR}/${FILENAME}.asm" ${PM_ASM_INCLUDES}
      VERBATIM)
    target_sources(${TARGET} PRIVATE "${OUT_DIR}/${FILENAME}")
  endforeach()
endfunction()

# ---- Build types and compiler policy every Paradox project shares ----
# Sanitization
if(SANITIZE)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Og -ggdb3 -fsanitize=address")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
endif()

# Strip release builds
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s")

# Premier: same optimization+strip as Release, but deliberately no -DNDEBUG
# (unlike CMake's built-in Release/MinSizeRel/RelWithDebInfo, which all add
# it) -- see the SPLASH_DEFAULT comment above for why: this build type wants
# every #ifndef NDEBUG debug-cheat path to stay compiled in and active.
set(CMAKE_C_FLAGS_PREMIER
    "-O3 -s"
    CACHE STRING "Flags used by the C compiler for the Premier build type" FORCE)
set(CMAKE_CXX_FLAGS_PREMIER
    "-O3 -s"
    CACHE STRING "Flags used by the CXX compiler for the Premier build type" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_PREMIER
    ""
    CACHE STRING "Flags used by the linker for the Premier build type" FORCE)
mark_as_advanced(CMAKE_C_FLAGS_PREMIER CMAKE_CXX_FLAGS_PREMIER CMAKE_EXE_LINKER_FLAGS_PREMIER)

# Showcase: identical flags to Release (optimization, strip, AND -DNDEBUG --
# unlike Premier, this keeps debug-cheat paths compiled out) -- exists as its
# own build type purely to get the SPLASH-on-by-default behavior above and
# land in its own bin/Showcase/ directory, without affecting plain Release.
set(CMAKE_C_FLAGS_SHOWCASE
    "-O3 -DNDEBUG -s"
    CACHE STRING "Flags used by the C compiler for the Showcase build type" FORCE)
set(CMAKE_CXX_FLAGS_SHOWCASE
    "-O3 -DNDEBUG -s"
    CACHE STRING "Flags used by the CXX compiler for the Showcase build type" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_SHOWCASE
    ""
    CACHE STRING "Flags used by the linker for the Showcase build type" FORCE)
mark_as_advanced(CMAKE_C_FLAGS_SHOWCASE CMAKE_CXX_FLAGS_SHOWCASE CMAKE_EXE_LINKER_FLAGS_SHOWCASE)

