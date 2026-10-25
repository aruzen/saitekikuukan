add_executable(strategy-test test.cpp)
target_link_libraries(strategy-test PRIVATE saitekikuukan::saitekikuukan)
add_test(NAME strategy-test COMMAND strategy-test)
