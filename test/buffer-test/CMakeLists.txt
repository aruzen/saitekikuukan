add_executable(buffer-test test.cpp)
target_link_libraries(buffer-test PRIVATE saitekikuukan::saitekikuukan)
add_test(NAME buffer-test COMMAND buffer-test)
