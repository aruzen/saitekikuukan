cmake_minimum_required(VERSION 3.28)
project(saitekikuukan-test)

set(CMAKE_CXX_STANDARD 26)

add_executable(saitekikuukan-test test.cpp)
target_include_directories(saitekikuukan-test PRIVATE ./include)

find_package(Boost REQUIRED COMPONENTS system)
message("Boost: ${Boost_INCLUDE_DIRS}, ${Boost}")
target_include_directories(saitekikuukan-test PRIVATE ${Boost_INCLUDE_DIRS})
target_link_libraries(saitekikuukan-test PRIVATE ${Boost_LIBRARIES})