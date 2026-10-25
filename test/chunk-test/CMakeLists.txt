add_executable(chunk-test test.cpp)
target_link_libraries(chunk-test PRIVATE saitekikuukan::saitekikuukan)
add_test(NAME chunk-test COMMAND chunk-test)
