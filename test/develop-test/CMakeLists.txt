cmake_minimum_required(VERSION 3.28)
project(develop-test LANGUAGES CXX)

add_executable(develop-test test.cpp)
target_include_directories(develop-test PRIVATE ./../../include)


target_include_directories(develop-test PRIVATE ${Boost_INCLUDE_DIRS})
target_compile_definitions(develop-test PRIVATE BOOST_ERROR_CODE_HEADER_ONLY)
# target_link_libraries(develop-test PRIVATE ${Boost_LIBRARIES})