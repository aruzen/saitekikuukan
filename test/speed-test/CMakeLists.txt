cmake_minimum_required(VERSION 3.28)
project(speed-test LANGUAGES CXX)

add_executable(speed-test test.cpp)
target_include_directories(speed-test PRIVATE ./../../include)
