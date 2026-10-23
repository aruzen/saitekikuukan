add_executable(storage-test test.cpp)
target_link_libraries(storage-test PRIVATE saitekikuukan::saitekikuukan)
add_test(NAME storage-test COMMAND storage-test)
