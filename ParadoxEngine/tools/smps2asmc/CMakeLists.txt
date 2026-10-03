cmake_minimum_required(VERSION 3.8)

project(smps2asmc LANGUAGES C)
set(CMAKE_C_STANDARD 99)

add_executable(smps2asmc "smps2asmc.c")

install(TARGETS smps2asmc RUNTIME DESTINATION bin)
