cmake_minimum_required(VERSION 3.28)
project(develop-test LANGUAGES CXX)

add_executable(develop-test test.cpp)
target_include_directories(develop-test PRIVATE ./../../include)
