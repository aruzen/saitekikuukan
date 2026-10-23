add_executable(buffer-test test.cpp)
target_include_directories(buffer-test PRIVATE ../../include)
add_test(NAME buffer-test COMMAND buffer-test)
