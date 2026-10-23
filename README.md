# SaitekiKuukan (for Data Oriented Design)
> High-performance Data-Oriented Design (DOD) Library for C++

`saitekikuukan` is a **data-oriented container library** designed for game engines and simulations.  
It provides SoA containers through type packs and reflection. AoSoA and chunked containers remain planned.

---

## Features

- **Data-Oriented Design Ready** ✅  
  - SoA containers with column-wise access

- **Type Pack Operations & Metaprogramming** ✅  
  - Static type manipulation using type lists (`type_list`) and filters (`_filter`)  
  - Safe and readable code with `if constexpr` and `concepts`

- **Modern C++ Support** ✅  
  - C++23 for type packs; C++26 with reflection for automatic struct schemas
  - Utilizes `std::expected`, `constexpr`, and `concepts`

- **Memory-Efficient Design** ✅  
  - Supports stateless types with EBO (Empty Base Optimization)  
  - Choose plain SoA or an ID/event-enabled buffer

- **Performance & Optimization**  
  - Contiguous column access; automatic SIMD depends on the compiler and the loop

- **Header-Only** ✅  
  - Single-file inclusion  
  - Easy to include and use in your project

---

## Installation

Since it’s header-only, just copy `include/saitekikuukan.hpp` into your project!

CMake users can use `add_subdirectory` or install and call `find_package(saitekikuukan CONFIG REQUIRED)`.
Link `saitekikuukan::saitekikuukan` for C++23 or `saitekikuukan::reflection` for struct reflection.
The library does not modify the parent project's standard or optimization flags.

## Generic SoA without IDs

`basic_soa<Layout, Types...>` owns columns and tracks the row count independently of IDs.
The existing `buffer` adds IDs and events to the same column management.

```cpp
// C++23
kk::soa<float, float, std::string> values;
values.push_back(1.0f, 2.0f, "first");
auto [x, y, name] = values.columns();
x[0] += 1.0f;
for (auto [rx, ry, label] : values)
    ry += rx;

kk::fixed_soa<1024, float, float> fixed;
fixed.resize(10);
```

```cpp
// C++26 with reflection
struct vertex {
    float x = 0, y = 0;
    std::array<float, 3> normal{0, 0, 1};
    std::string name = "default";
};
kk::record_soa<vertex> vertices;
vertices.resize(10);                  // Uses vertex{}, including member defaults
auto storage = vertices.columns();   // Aggregate of column views, returned by value
storage.x[0] = 1.0f;
vertices.begin()->x += 1.0f;
vertices.column<^^vertex::y>()[0] = 2.0f;
vertex value = vertices.read(0);
vertices.write(1, value);
using fixed_vertices = kk::record_soa<vertex, kk::storage::fixed_layout<1024>>;
```

`column<N>()` / `get<N>()` expose only active rows. Ordinary contiguous columns return `std::span<T>`;
const containers return `std::span<const T>`. `vector<bool>` and noncontiguous columns return iterator views.
Column views cannot resize the owned columns. `operator[]` is unchecked; `at`, `read`, `write`, and
`erase_unordered` check bounds. `erase_unordered` moves the last row into the erased position.
Iterators provide random access to reference proxies; use `read` / `write` for owning row values.
Empty schemas retain their row counts. Plain record SoA has no reserved member names.

Layouts provide `column<T>`, `resizable`, and `max_size`. The predefined layouts use vectors or arrays;
`dynamic_layout<N>` also supports a capacity limit. Custom random access columns need `size`, and resizable
columns need `push_back`, `pop_back`, and `clear`. Optional `reserve`, `capacity`, and `max_size` are used when present.
Fixed columns construct their entire capacity upfront. Resizable element types must be copy constructible
or nothrow move constructible.

Insertion stages all arguments before allocating, including references to existing columns.
Failed insertion/growth preserves row counts and column lengths. Standard dynamic columns use copy-and-swap
for copy assignment. Throwing element assignment/swapping may partially change values during `write`,
`erase_unordered`, or fixed-column assignment. Reacquire views/references after invalidating operations;
even `reserve` or failed growth may reallocate some columns.
Obtaining column views from temporary SoA containers is rejected.
If a custom column's move constructor throws, the source may be cleared to keep column lengths consistent.

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
In `record_buffer`, member names `id`, `get`, `parent`, and `index` are reserved for the row API.
`columns()` / `column<N>()` also provide views over active rows. Legacy `get<N>()` and `storage.entries`
remain available; callers must not change owning column lengths directly.
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
The implementation is contained in the single `saitekikuukan.hpp` header.

```sh
cmake -S . -B build/reflection -DCMAKE_CXX_COMPILER=g++-16 \
  -DSAITEKIKUUKAN_BUILD_REFLECTION_TESTS=ON
cmake --build build/reflection
ctest --test-dir build/reflection --output-on-failure
```

---

## Future Features (Checklist)

- [ ] SIMD Optimization  
- [ ] GPU Parallel Processing
---
