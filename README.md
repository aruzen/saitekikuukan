# SaitekiKuukan (for Data Oriented Design)
> High-performance Data-Oriented Design (DOD) Library for C++

`saitekikuukan` is a **data-oriented container library** designed for game engines and simulations.  
It leverages template parameter packs and metaprogramming to easily handle SoA, AoSoA, and chunk-based data structures.

---

## Features

- **Data-Oriented Design Ready** ✅  
  - Chunked (AoSoA) / SoA data structures for maximum CPU cache efficiency  

- **Type Pack Operations & Metaprogramming** ✅  
  - Static type manipulation using type lists (`type_list`) and filters (`_filter`)  
  - Safe and readable code with `if constexpr` and `concepts`

- **Modern C++ Support** ✅  
  - Compatible with C++17/20/26  
  - Utilizes `std::expected`, `constexpr`, and `concepts`

- **Memory-Efficient Design** ✅  
  - Supports stateless types with EBO (Empty Base Optimization)  
  - Minimizes unnecessary padding via type lists and chunk management

- **Performance & Optimization**  
  - Improved cache hit rate with AoSoA / chunk management

- **Header-Only** ✅  
  - Single-file inclusion  
  - Easy to include and use in your project

---

## Installation

Since it’s header-only, just copy `include/saitekikuukan.hpp` into your project!

## Named SoA from a struct (C++26 prototype)

`saitekikuukan_reflection.hpp` generates a column aggregate with the original member names.
Each member owns its array or vector directly and supports both named access and structured bindings.

```cpp
#include "saitekikuukan_reflection.hpp"
#include <string>

struct particle {
    float x, y;
    std::string name;
};

kk::storage::static_soa<particle, 1024> storage{};
storage.x[0] = 1.0f;
storage.name[0] = "first";
auto& [x, y, name] = storage;

kk::storage::dynamic_soa<particle> dynamic;
kk::storage::for_each_column(dynamic, [](auto& column) { column.resize(1024); });
dynamic.x[0] = 2.0f;
```

`static_soa` uses `std::array`; `dynamic_soa` uses `std::vector`.
Use `kk::storage::soa<Record, Column>` for a custom column container template.
This API generates column storage only; it does not include the existing `buffer` ID or event handling.
Callers manage column lengths. Original member initializers are not propagated;
use `{}` when declaring fixed-size storage to zero-initialize scalar columns.

Records must be non-union aggregates without base classes, reference members, or bit-fields.
Static members are omitted; nested structs remain individual column element types.
This header is independent of the existing header. GCC 16 requires `-std=c++26 -freflection`.

```sh
cmake -S . -B build/reflection -DCMAKE_CXX_COMPILER=g++-16 \
  -DSAITEKIKUUKAN_BUILD_REFLECTION_TESTS=ON
cmake --build build/reflection --target reflection-test
ctest --test-dir build/reflection --output-on-failure
```

---

## Future Features (Checklist)

- [ ] SIMD Optimization  
- [ ] GPU Parallel Processing
---
