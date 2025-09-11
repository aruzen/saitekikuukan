cmake_minimum_required(VERSION 3.28)
project(speed-test LANGUAGES CXX)

add_executable(speed-test test.cpp)
target_include_directories(speed-test PRIVATE ./../../include)


target_include_directories(speed-test PRIVATE ${Boost_INCLUDE_DIRS})
target_compile_definitions(speed-test PRIVATE BOOST_ERROR_CODE_HEADER_ONLY)
# target_link_libraries(speed-test PRIVATE ${Boost_LIBRARIES})