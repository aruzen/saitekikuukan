add_executable(selection-test test.cpp)
target_link_libraries(selection-test PRIVATE saitekikuukan::saitekikuukan)
add_test(NAME selection-test COMMAND selection-test)
