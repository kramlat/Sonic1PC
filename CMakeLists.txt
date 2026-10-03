cmake_minimum_required(VERSION 3.16)
project(ParadoxOffice LANGUAGES CXX)

# The only thing this CMake project builds by hand is ParadoxMake, the build tool for Paradox Engine projects. ParadoxMake then reads the
# paradoxmakefile.yml of each project (ParadoxEngine, Sonic The Hedgehog, ...) and builds everything else; the real build tree is <this build dir>/projects.
#
#   cmake -S . -B build && cmake --build build                  builds ParadoxMake and the game
#   cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr && cmake --install build
#
# Choose another project with -DPARADOX_PROJECT=<folder>; the game's own options (SPLASH, FIX_BUGS, JAPANESE, SANITIZE, LTO, BUILD_TESTS) are
# passed through when given.
add_subdirectory(ParadoxMake)

set(PARADOX_PROJECT "Sonic The Hedgehog" CACHE STRING "The project folder ParadoxMake builds")
set(PM_ARGS -B "${CMAKE_BINARY_DIR}/projects")
if(CMAKE_BUILD_TYPE)
  list(APPEND PM_ARGS "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
endif()
foreach(OPT JAPANESE FIX_BUGS SPLASH SANITIZE LTO BUILD_TESTS)
  if(DEFINED ${OPT})
    list(APPEND PM_ARGS "-D${OPT}=${${OPT}}")
  endif()
endforeach()

add_custom_target(projects ALL
  COMMAND paradoxmake build ${PM_ARGS} "${CMAKE_SOURCE_DIR}/${PARADOX_PROJECT}"
  DEPENDS paradoxmake
  USES_TERMINAL VERBATIM)

install(CODE "execute_process(COMMAND \"$<TARGET_FILE:paradoxmake>\" install ${PM_ARGS} --prefix \"\${CMAKE_INSTALL_PREFIX}\" \"${CMAKE_SOURCE_DIR}/${PARADOX_PROJECT}\" COMMAND_ERROR_IS_FATAL ANY)")
