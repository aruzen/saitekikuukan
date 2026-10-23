add_executable(reflection-test test.cpp)
target_include_directories(reflection-test PRIVATE ../../include)
target_compile_options(reflection-test PRIVATE $<$<CXX_COMPILER_ID:GNU>:-freflection>)

add_test(NAME reflection-test COMMAND reflection-test)
