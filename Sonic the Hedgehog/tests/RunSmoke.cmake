# Runs TEST_EXE headlessly for up to TEST_TIMEOUT seconds and checks it
# didn't crash. The game's main loop never exits on its own, so "still
# running when the timeout hits" is the *success* case here -- anything
# else (crash, early exit, an ASan abort) is a failure.
#
# Usage: cmake -DTEST_EXE=<path> [-DTEST_ARGS="--zone;0"] [-DTEST_TIMEOUT=20]
#              -P RunSmoke.cmake

if(NOT DEFINED TEST_EXE)
  message(FATAL_ERROR "TEST_EXE not set")
endif()
if(NOT DEFINED TEST_TIMEOUT)
  set(TEST_TIMEOUT 20)
endif()

# SDL_VIDEODRIVER/AUDIODRIVER=dummy so this also works on a headless CI
# runner with no real display or audio device.
execute_process(
  COMMAND ${CMAKE_COMMAND} -E env SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
          ${TEST_EXE} ${TEST_ARGS}
  TIMEOUT ${TEST_TIMEOUT}
  RESULT_VARIABLE result
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err)

if(result STREQUAL "Process terminated due to timeout")
  message(STATUS "Smoke test: still running after ${TEST_TIMEOUT}s "
                  "(expected -- the game loop never exits on its own)")
elseif(result EQUAL 0)
  message(STATUS "Process exited cleanly (result=0)")
else()
  message(STATUS "--- stdout ---\n${out}")
  message(STATUS "--- stderr ---\n${err}")
  message(FATAL_ERROR "Smoke test FAILED: process exited abnormally (result=${result})")
endif()
