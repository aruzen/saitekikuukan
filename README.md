# SaitekiKuukan

A C++23 header-only SoA container. The supported core consists of `storage::DynamicColumns<Types...>` and `SoA<Row>`, with contiguous columns and no mandatory IDs, events, Boost, or GPU dependencies.

See [README-JP.md](README-JP.md) for examples and full contracts. Register fields through `Schema<Row>::Members = MemberList<&Row::field, ...>`, then access a contiguous column through `column<&Row::field>()`. Member pointers distinguish fields with identical types.

`SoA` supports row defaults, checked read/write/at, lightweight RowRef access, and empty schemas. Structural updates preserve equal column lengths on failure. Field assignment may partially update a row if assignment throws. Reserve, growth, deletion, and ownership changes can invalidate views or references; see the Japanese documentation.

AoS and AoSoA are not implemented. The legacy buffer is still experimental; its basic operations use the shared columns core, but events, UUIDs, and coroutine generators remain outside the supported scope. Legacy dynamic storage no longer exposes mutable vectors, and buffer getters now return spans/reference_wrapper values.

## CMake

```cmake
add_subdirectory(path/to/saitekikuukan)
target_link_libraries(app PRIVATE saitekikuukan::saitekikuukan)
```

Installed packages support `find_package(saitekikuukan CONFIG REQUIRED)`. Tests require GTest and are enabled by default only when this is the top-level project.

```sh
cmake -S . -B build -DSAITEKIKUUKAN_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Development benchmarks are opt-in through `SAITEKIKUUKAN_BUILD_BENCHMARKS`. The library does not modify parent compiler flags or guarantee automatic vectorization or performance improvements.
