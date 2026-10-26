add_executable(event-test test.cpp)
target_link_libraries(event-test PRIVATE saitekikuukan::saitekikuukan)
add_test(NAME event-test COMMAND event-test)
