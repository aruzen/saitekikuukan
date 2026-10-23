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
  - Built with C++26; reflection features require a supporting compiler
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

## Struct schemas for buffer (C++26)

`record_buffer<T>` is an alias for the existing `buffer` with a `record<T>` schema.
Reflection generates named columns and row references; the existing buffer manages sizes, IDs, insertion, erasure, and iteration.

```cpp
#include "saitekikuukan.hpp"
#include <string>

struct particle {
    float x, y;
    std::string name;
};

kk::record_buffer<particle> particles{};
auto inserted = particles.push_back(particle{1.0f, 2.0f, "first"});
auto& storage = particles.storage.entries;
storage.x[0] += 1.0f;
auto& [x, y, name] = storage;

auto it = particles.begin();
it->x += 1.0f;
it->get<0>() += 1.0f;
auto id = it->id();
particles.get<0>(id).value().get() = 5.0f;
for (auto row : particles)
    row.y += 2.0f;
```

Fixed column capacity is separate from the active row count, `particles.size()`.
`begin()` / `end()` iterate over active rows only.
Select dynamic columns with `record_buffer<particle, generator::iota, eventsystem::dummy, storage::dynamic_storage>`.
The existing `buffer<Generator, Events, Storage, Size, Types...>` and tuple-based `storage.entries` remain supported.
A struct without an explicit `record<T>` schema remains one column element type.

`push_back` accepts either a record or individual column values. Configure existing event registries through `events()`;
both schemas share pre-insertion changes/cancellation and post-insertion notifications.
For ordinary columns, ID-based `get<N>(id)` returns `expected<reference_wrapper<T>, error_code>`.
Record member names `id`, `get`, `parent`, and `index` are reserved for the row API.
Reacquire row references after operations that invalidate column references, such as reallocation.

### Generating column storage alone

`saitekikuukan.hpp` also generates column aggregates with the original member names.
Each member owns its array or vector directly and supports both named access and structured bindings.

```cpp
#include "saitekikuukan.hpp"
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

Use `rows(storage)` to iterate over rows. It provides `begin()` / `end()` and random access;
each row holds references or proxies to the original column elements under the same member names.

```cpp
auto rows = kk::storage::rows(storage);
rows.begin()->x += 1.0f;
auto row = *rows.begin();
row.name = "updated";

for (auto row : rows) {
    row.x += 1.0f;
    row.y += 2.0f;
}

auto readonly = kk::storage::rows(std::as_const(storage));
```

Even with `auto row`, member updates write back to the columns. Const storage uses each column's const element access.
Iteration stops at the shortest column; an empty schema has zero rows.
Recreate the view after resizing columns. Constructing a view from temporary storage is rejected.

`static_soa` uses `std::array`; `dynamic_soa` uses `std::vector`.
Use `kk::storage::soa<Record, Column>` for a custom column container template.
This API generates column storage only; it does not include the existing `buffer` ID or event handling.
Callers manage column lengths. Original member initializers are not propagated;
use `{}` when declaring fixed-size storage to zero-initialize scalar columns.

Records must be non-union aggregates without base classes, reference members, or bit-fields.
Static members are omitted; nested structs remain individual column element types.
GCC 16 requires `-std=c++26 -freflection`. Legacy type-pack buffers build without reflection enabled.
The implementation is integrated into `saitekikuukan.hpp`; `saitekikuukan_reflection.hpp` is a compatibility entry point.

```sh
cmake -S . -B build/reflection -DCMAKE_CXX_COMPILER=g++-16 \
  -DSAITEKIKUUKAN_BUILD_REFLECTION_TESTS=ON
cmake --build build/reflection --target buffer-test reflection-test
ctest --test-dir build/reflection --output-on-failure
```

---

## Future Features (Checklist)

- [ ] SIMD Optimization  
- [ ] GPU Parallel Processing
---
