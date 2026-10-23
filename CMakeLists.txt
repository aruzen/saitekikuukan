cmake_minimum_required(VERSION 3.23)
project(saitekikuukan VERSION 0.1.0 LANGUAGES CXX)

add_library(saitekikuukan INTERFACE)
add_library(saitekikuukan::saitekikuukan ALIAS saitekikuukan)
target_compile_features(saitekikuukan INTERFACE cxx_std_23)
target_include_directories(saitekikuukan INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

option(SAITEKIKUUKAN_BUILD_TESTS "Build correctness tests" ${PROJECT_IS_TOP_LEVEL})
option(SAITEKIKUUKAN_BUILD_BENCHMARKS "Build development benchmarks" OFF)
if(SAITEKIKUUKAN_BUILD_TESTS)
    enable_testing()
    find_package(GTest REQUIRED)
    include(GoogleTest)
    add_executable(saitekikuukan-tests test/storage_test.cpp test/soa_test.cpp test/buffer_test.cpp)
    target_link_libraries(saitekikuukan-tests PRIVATE saitekikuukan GTest::gtest_main)
    gtest_discover_tests(saitekikuukan-tests)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        foreach(case RANGE 1 4)
            add_test(NAME "SchemaCompile.RejectInvalidCase${case}"
                COMMAND ${CMAKE_COMMAND}
                "-DTEST_COMPILER=${CMAKE_CXX_COMPILER}"
                "-DTEST_INCLUDE=${CMAKE_CURRENT_SOURCE_DIR}/include"
                "-DTEST_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/test/schema_compile.cpp"
                "-DTEST_CASE=${case}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/test/expect_compile_failure.cmake")
        endforeach()
    endif()
endif()

if(SAITEKIKUUKAN_BUILD_BENCHMARKS)
    add_subdirectory(test/speed-test)
    add_subdirectory(test/develop-test)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
configure_package_config_file(cmake/saitekikuukanConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/saitekikuukanConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/saitekikuukan")
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/saitekikuukanConfigVersion.cmake"
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/saitekikuukanConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/saitekikuukanConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/saitekikuukan")
install(TARGETS saitekikuukan EXPORT saitekikuukanTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT saitekikuukanTargets NAMESPACE saitekikuukan::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/saitekikuukan)
