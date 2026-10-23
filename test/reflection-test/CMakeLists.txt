add_executable(reflection-test test.cpp)
target_link_libraries(reflection-test PRIVATE saitekikuukan::reflection)

add_test(NAME reflection-test COMMAND reflection-test)
