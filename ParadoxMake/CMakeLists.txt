# ParadoxMake: the build tool for Paradox Engine projects (see src/main.cpp). It is the only thing the root CMake project builds by hand; it
# then reads each project's paradoxmakefile.yml and builds the rest.
cmake_minimum_required(VERSION 3.16)
project(ParadoxMake LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(yaml-cpp REQUIRED)
add_executable(paradoxmake src/main.cpp)
target_link_libraries(paradoxmake PRIVATE yaml-cpp::yaml-cpp)
target_compile_definitions(paradoxmake PRIVATE PARADOXMAKE_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Installed, it finds its helper scripts at <prefix>/share/paradoxmake/cmake (see dataDir() in main.cpp)
install(TARGETS paradoxmake RUNTIME DESTINATION bin)
install(DIRECTORY cmake/ DESTINATION share/paradoxmake/cmake)
