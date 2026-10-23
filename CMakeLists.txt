cmake_minimum_required(VERSION 3.28)
project(saitekikuukan)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native -ftree-vectorize")

add_subdirectory(test/speed-test)
add_subdirectory(test/develop-test)
enable_testing()
add_subdirectory(test/buffer-test)

option(SAITEKIKUUKAN_BUILD_REFLECTION_TESTS "Build the C++26 reflection tests" OFF)
if(SAITEKIKUUKAN_BUILD_REFLECTION_TESTS)
    add_subdirectory(test/reflection-test)
endif()
