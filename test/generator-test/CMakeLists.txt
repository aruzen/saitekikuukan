add_executable(generator-test test.cpp)
target_link_libraries(generator-test PRIVATE saitekikuukan::saitekikuukan)
add_test(NAME generator-test COMMAND generator-test)
