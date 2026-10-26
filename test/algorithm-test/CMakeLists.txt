add_executable(algorithm-test test.cpp)
target_link_libraries(algorithm-test PRIVATE saitekikuukan::saitekikuukan)
add_test(NAME algorithm-test COMMAND algorithm-test)
