add_executable(recycling-test test.cpp)
target_link_libraries(recycling-test PRIVATE saitekikuukan::saitekikuukan)
add_test(NAME recycling-test COMMAND recycling-test)
