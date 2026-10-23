add_executable(soa-test test.cpp)
target_link_libraries(soa-test PRIVATE saitekikuukan::saitekikuukan)
add_test(NAME soa-test COMMAND soa-test)
