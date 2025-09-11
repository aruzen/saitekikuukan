cmake_minimum_required(VERSION 3.28)
project(saitekikuukan)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native -ftree-vectorize")

add_definitions(-DBOOST_ERROR_CODE_HEADER_ONLY)

find_package(Boost COMPONENTS system QUIET)
if(NOT Boost_FOUND)
    message(STATUS "Boost Config mode failed. Retrying with FindBoost...")
    set(Boost_NO_BOOST_CMAKE ON)
    find_package(Boost REQUIRED COMPONENTS system)
endif()
message("Boost: ${Boost_INCLUDE_DIRS}, ${Boost}")

add_subdirectory(test/speed-test)
add_subdirectory(test/develop-test)