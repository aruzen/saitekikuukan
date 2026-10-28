add_executable(aos-test test.cpp)
target_link_libraries(aos-test PRIVATE saitekikuukan::saitekikuukan)
add_test(NAME aos-test COMMAND aos-test)
