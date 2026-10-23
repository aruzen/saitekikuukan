cmake_minimum_required(VERSION 3.28)
project(saitekikuukan)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native -ftree-vectorize")

add_subdirectory(test/speed-test)
add_subdirectory(test/develop-test)
