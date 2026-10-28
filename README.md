# SaitekiKuukan (for Data Oriented Design)
> High-performance Data-Oriented Design (DOD) Library for C++

`saitekikuukan` is a **data-oriented container library** designed for game engines and simulations.  
It provides SoA and chunked AoSoA containers through type packs and reflection.

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

## Buffer strategies

`buffer<Schema, Structure, Layout, Identity, Events>` is the common container entry point.
Each argument selects an independent compile-time policy.

| Argument | Default | Choices |
| --- | --- | --- |
| Schema | `type_list<>` | `type_list<Types...>` or a struct with C++26 reflection |
| Structure | `strategy::structure_of_array` | `strategy::array_of_structure` or a custom policy |
| Layout | `storage::dynamic_layout<>` | `fixed_layout<N>`, `dynamic_layout<N>`, `chunked_layout<N>` |
| Identity | `identity::none` | `identity::generated<Generator, Lookup>` |
| Events | `eventsystem::dummy` | Registered listeners or a custom event system |

`soa`, `fixed_soa`, `record_soa`, and `chunked_soa` are aliases for configured buffers.
Identity is independent of data layout; both flat SoA and chunks support IDs.
Without identity, the buffer stores no ID column, generator, or lookup index.

`push_back` and `resize` return `expected<void, error_code>`, with `buffer_over` for capacity limits.
Allocation and element exceptions propagate.

## Array-of-structures strategy

`strategy::array_of_structure` stores complete row objects using fixed or dynamic layouts.
ID policies, events, sorting, and batch operations use the same buffer APIs as SoA.
Type-list schemas store tuples in C++23; reflected struct schemas store the original record type in C++26.

```cpp
kk::buffer<particle, kk::strategy::array_of_structure> particles;
particles.resize(3).value();
particles.begin()->x = 10;
auto columns = particles.columns();
columns.y[0] = 20;
```

Without IDs, standard layouts provide contiguous rows and actual row references.
Columns are non-contiguous random-access projection views; const containers expose const elements.
Identified rows keep the reference proxy that provides IDs and named fields.
Use `auto&&` for mutable row iteration and `const auto&` for comparisons/projections across layouts.
A by-value `auto row` or structured row binding copies an ordinary AoS row without IDs.
Structured binding of `columns()` copies views and still updates the underlying data.

AoS reuses the existing column manager for ownership and failure handling.
Fixed layouts construct all row slots up front and retain inactive slots after shrinking or clearing.
Dynamic rows require copy construction or noexcept move construction. Struct schema restrictions match SoA.

## Column views and rows

```cpp
// C++23
kk::buffer<kk::type_list<float, float, std::string>,
    kk::strategy::structure_of_array> values;
values.push_back(1.0f, 2.0f, "first").value();
auto [x, y, name] = values.columns();
x[0] += 1.0f;
for (auto [rx, ry, label] : values)
    ry += rx;

kk::fixed_soa<1024, float, float> fixed;
fixed.resize(10).value();
```

```cpp
// C++26 with reflection
struct vertex {
    float x = 0, y = 0;
    std::array<float, 3> normal{0, 0, 1};
    std::string name = "default";
};
kk::buffer<vertex> vertices;
vertices.resize(10).value();                  // Uses vertex{}, including member defaults
auto storage = vertices.columns();   // Aggregate of column views, returned by value
storage.x[0] = 1.0f;
vertices.begin()->x += 1.0f;
vertices.column<^^vertex::y>()[0] = 2.0f;
vertex value = vertices.read(0);
vertices.write(1, value).value();
using fixed_vertices = kk::record_soa<vertex, kk::storage::fixed_layout<1024>>;
```

`column<N>()` / `get<N>()` expose only active rows. Ordinary contiguous columns return `std::span<T>`;
const containers return `std::span<const T>`. `vector<bool>` and noncontiguous columns return iterator views.
Column views cannot resize the owned columns. `operator[]` is unchecked; `at`, `read`, `write`, and
`erase_unordered` check bounds. `erase_unordered` moves the last row into the erased position.
Iterators provide random access to reference proxies; use `read` / `write` for owning row values.
Empty schemas retain their row counts. Plain record SoA has no reserved member names.

Flat layouts provide `column<T>`, `resizable`, and `max_size`. The predefined layouts use vectors or arrays;
`dynamic_layout<N>` also supports a capacity limit. Custom random access columns need `size`, and resizable
columns need `push_back`, `pop_back`, and `clear`. Optional `reserve`, `capacity`, and `max_size` are used when present.
Fixed columns construct their entire capacity upfront. Resizable element types must be copy constructible
or nothrow move constructible.

Insertion stages all arguments before allocating, including references to existing columns.
Failed insertion/growth keeps row counts and column lengths consistent. Standard dynamic columns use copy-and-swap
for copy assignment. Throwing element assignment/swapping may partially change values during `write`,
`erase_unordered`, or fixed-column assignment. Reacquire views/references after invalidating operations;
even `reserve` or failed growth may reallocate some columns.
Obtaining column views from temporary SoA containers is rejected.
If a custom column's move constructor throws, the source may be cleared to keep column lengths consistent.

## Chunked SoA (AoSoA)

`storage::chunked_layout<ChunkSize>` groups fixed-capacity SoA containers into independently
owned blocks. `chunked_soa<ChunkSize, Types...>` is the alias without identity. Appending or reserving blocks preserves references to existing rows.
Chunk sizes are positive compile-time constants; powers of two are optional.

```cpp
// C++23
kk::chunked_soa<128, float, float> points;
points.resize(300).value(); // Active block lengths: 128, 128, 44
for (auto chunk : points.chunks()) {
    auto [x, y] = chunk.columns();
    for (std::size_t i = 0; i < chunk.size(); ++i) {
        x[i] += 1.0f;
        y[i] += 2.0f;
    }
}
points.column<0>()[129] = 10.0f;
points.erase_unordered(0).value();
```

```cpp
// C++26 with the existing reflection schema
struct particle { float x = 0, y = 0; std::string name = "default"; };
kk::record_chunked_soa<particle, 128> particles;
particles.resize(300).value();
for (auto chunk : particles.chunks()) {
    auto storage = chunk.columns();
    for (std::size_t i = 0; i < chunk.size(); ++i)
        storage.x[i] += storage.y[i];
}
particles.begin()->name = "first";
particles.column<^^particle::x>()[129] = 2.0f;
```

Whole-container row iteration, indexing, and `read/write` cross block boundaries.
Whole-column `column<N>()` / `get<N>()` views provide random access without promising contiguous storage.
The views returned by `chunks()` / `chunk(i)` expose contiguous active columns as spans;
use these for vectorizable inner loops. They permit element updates while the owner manages row counts.
Const owners expose const elements through every view.

`reserve(n)` constructs blocks, `capacity()` reports allocated row capacity, and `chunk_count()` counts active blocks.
Without matching notifications, `resize(n)` preserves record defaults and restores row counts on failure.
Like fixed SoA, blocks construct all column slots upfront, so column elements must be default constructible.
Unused slot objects remain alive after pop, clear, or shrink; `shrink_to_fit()` releases unused blocks.
Copyable columns support copying; move-only columns are also supported.
Throwing element assignment/swapping may partially update values during `write` / `erase_unordered`.
Views capture their row ranges; reacquire them after changing sizes.

## Identity generation and lookup

```cpp
struct particle { float x = 0, y = 0; std::string name; };
using particles_type = kk::buffer<particle,
    kk::strategy::structure_of_array,
    kk::storage::dynamic_layout<1024>,
    kk::identity::generated<>>;

particles_type particles;
particles.push_back(particle{1, 2, "first"}).value();
auto id = particles.begin()->id();
particles.find(id).value().x = 5;
particles[0].y = 10; // Always a physical row index
particles.erase(id).value();
```

`identity::generated<Generator, Lookup>` independently selects generation and lookup.
Defaults are `generator::iota` and `strategy::hash_id_lookup`.
A generator provides `generate_type` and `next()` returning `optional<generate_type>`.

| Lookup | Behavior |
| --- | --- |
| `strategy::hash_id_lookup` | Additional hash index; average O(1) lookup and registration, updated after row moves |
| `strategy::linear_id_lookup` | No lookup index; O(n) search/duplicate checks, O(n²) duplicate checks during bulk growth |

Hash lookup requires nonthrowing `std::hash<ID>` and equality. IDs require copy construction and
nonthrowing move/swap. Fixed-capacity ID columns also require default construction.
Both lookup policies reject duplicate IDs with `duplicate_id`.
Generation failure returns `generate_id_failed`; failed/canceled operations may consume IDs.

`find(id)` and `index(id)` return `expected`, with `not_found_id` for missing IDs.
`get<N>(id)` returns `expected<reference_wrapper<T>, error_code>` for ordinary columns.
`operator[]`, `at`, `read/write`, and `erase_unordered` always take physical row indices.
`erase(id)` deletes by ID; moving the last row preserves its identity.
Owning columns and ID tables are private. `columns()`, `column<N>()`, and `get<N>()` expose element views.
`capacity()` reports data-column capacity; ID columns and lookup indexes allocate additional storage.
Identified rows reserve the names `id`, `get`, `parent`, and `index` for the row API.

## Coroutine ID generators

`generator::coroutine_generator<T>` returns yielded values as `optional<T>`.
Empty, exhausted, and moved-from generators return `nullopt`; coroutine exceptions propagate from `next()`.
The generator exclusively owns its frame, is move-only, and also supports move-only values.
Pass a stateful generator to an identified buffer's constructor; an initial event system may be passed as its second argument.

## Sparse ID lookup

`strategy::sparse_id_lookup<PageSize = 256, Key = strategy::sparse_key>` maps unsigned keys directly to row positions using paged arrays.
The default key projection accepts unsigned integer IDs or IDs with an unsigned `slot()` accessor.
Custom projections must be noexcept and return an unsigned integer fitting `size_t`.
Only one ID per projected key can be stored; lookup also checks the complete ID.
Pages are allocated on demand. The outer page table grows with the largest key, so prefer hashing for extremely sparse, large keys.
`reserve(n)` reserves page-table capacity; `clear()` retains pages for reuse. Compare memory use and constant costs for your workload.

## Recycling and generational IDs

`identity::recycled<Lookup = strategy::hash_id_lookup, Index = uint32_t, Generation = uint32_t>` combines a recycling generator and the selected lookup policy.
Its `identity::generational_id<Index, Generation>` handles expose `slot()` and `generation()`.
Deletion, shrinking, and clearing release IDs; a reused slot receives a new generation, so old IDs do not resolve.
Slots are permanently retired at the maximum generation instead of wrapping. The maximum Index value is reserved for invalid handles.
Exhaustion returns `generate_id_failed`. Copies have independent pools; moves transfer the pool and leave the source generator unbound.
Use handles with the corresponding buffer state.

Canceled/failed insertion and failed batch growth release reservations; an exception after successful insertion does not release its live ID.
Changing a recycling ID in a before-insert event returns `invalid_id`.
Custom releasable generators implement `release(const ID&) noexcept` and use IDs with noexcept copy construction.
Generators without release support retain their existing behavior.

## Row algorithms and batch operations

`sort(compare, projection = std::identity{})` compares read-only rows, then permutes every column and its ID together.
It is not stable. Comparisons and projections finish before rows move; sorting uses two index arrays and does not copy field values.
For reflected rows, a projection can return `row.x`.

`append_range(rows)` accepts an input range constructible as the original row value type and returns the appended count.
Buffers with the same value type can also be copied across layouts/identity policies. Appended rows receive new IDs.
Input is staged before storage grows, supporting self-append and aliased column views; use move iterators for move-only values.
Staging needs memory proportional to the input size. Input/staging/capacity failures append no rows.

`erase_if(predicate)` evaluates read-only rows, removes matches without preserving order, and returns the removed count.
Sort returns `expected<void, error_code>`; append and erase return `expected<size_t, error_code>`.
All operations use public row mutation APIs, including events and ID lifetime management.
Cancellation or exceptions during mutation retain previously completed changes; compare sizes to count completed insertions/deletions on failure.
Do not change the target buffer's row count or row order from comparisons, projections, predicates, or input-range evaluation.

## Events

Select `Events` with the last template argument and access it through `events()`.
Insertion, deletion, row swaps, and explicit updates support notifications with and without IDs,
for fixed, dynamic, and chunked layouts.

| Event system | Listener storage and invocation |
| --- | --- |
| `dummy` | No notifications, additional event state, or row snapshots |
| `registration<Events...>` | Register external listeners and invoke them in registration order |
| `one_event_each_only<Events...>` | Register one external listener per event |
| `zero_cost<Listeners...>` | Construct temporary listeners in template argument order |
| `minimum_cost<Listeners...>` | Store only stateful listeners; invoke all listeners in template argument order |

Static listeners provide `listen_event` and `on(Event&)`, for example by inheriting
`listener_tag<Event>`. Empty lists and multiple listeners for the same event are supported.
`minimum_cost` stores its stateful listeners in the public `events().listeners` tuple, in declaration order.

| Operation | With IDs | Without IDs |
| --- | --- | --- |
| Insert | `will_insert_event<ID, Types...>` / `did_insert_event<ID, Types...>` | `will_insert_row_event<Types...>` / `did_insert_row_event<Types...>` |
| Delete | `will_delete_event<ID, Types...>` / `did_delete_event<ID, Types...>` | `will_delete_row_event<Types...>` / `did_delete_row_event<Types...>` |
| Swap rows | `will_swap_event<ID, Types...>` / `did_swap_event<ID, Types...>` | `will_swap_row_event<Types...>` / `did_swap_row_event<Types...>` |
| Update | `will_update_event<ID, Types...>` / `did_update_event<ID, Types...>` | `will_update_row_event<Types...>` / `did_update_row_event<Types...>` |

Before events support `cancel()`. Cancellation returns `operation_canceled` and skips the corresponding after event.
Before insert/update events expose mutable references to the proposed values in `data`.
Before delete/swap events expose read-only rows. Generated insertion IDs can be changed;
IDs in delete/swap/update events are read-only.
After insert/update/swap events reference stored elements, including writable `vector<bool>` values.

Before deletion, the payload contains `index`, `data`, and, when enabled, `id`.
After deletion, only the removed row's `index` and optional `id` are provided.
No destroyed elements are referenced, so deletion notifications also support move-only fields.
Swap events expose `first_index`, `second_index`, `first`, and `second`.
Their optional `first_id` and `second_id` describe the IDs at those positions at notification time.

- `erase`, `erase_unordered`, and `pop_back` emit deletion events. Internal relocation during deletion does not emit swap events.
- `swap_rows` emits swap events. Swapping a position with itself emits nothing.
- `write` emits update events. Direct writes through column or row references emit nothing.
- Growing `resize` emits insert events; shrinking `resize` and `clear` emit delete events from the back.
- Empty `pop_back`/`clear` and unchanged `resize` emit nothing.
- `reserve`, whole-container copy/move/swap, and destruction do not emit row events.

`push_back`, `resize`, `write`, `erase`, `erase_unordered`, `pop_back`, `clear`, and `swap_rows`
return `expected<void, error_code>`. Invalid row positions still throw `out_of_range`.
Operations without matching subscriptions use the underlying storage path directly.

Observable `resize`/`clear` operations process one row at a time and stop at the first cancellation or exception.
Previously completed row operations and notifications remain committed.
If an after listener throws, the corresponding row operation has already completed.

External listeners are not owned: keep them alive while registered.
During notification, do not change the row count or row order of the same buffer, or change its listener lists.

## Generating column storage alone

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

## Executable samples

[test/samples](test/samples/README.md) contains standalone examples for type packs, fixed capacity,
IDs and events, chunks, custom columns, move-only types, and reflected records.
Each example validates its results and can be built individually or run through CTest.

## Performance reports

Run `python3 tools/performance_report.py --compiler clang++`, or use
`--compiler g++-16 --reflection` to include reflected records with and without member initializers.
The script compares reserved `clear/resize`, reserved `clear/push_back`, and column/row/read-write
updates against direct vectors. Every result is checked across all rows.

Defaults are 262144 rows, 24 iterations, 7 samples, and both `-O2` and `-O3`.
Use `--rows`, `--iterations`, `--samples`, and `--levels O3` to adjust the run.
`--include-dir` selects another version's headers for comparison with the same benchmark sources.
`--output` defaults to `build/performance` and stores median ns/row CSV files, assembly,
vectorization diagnostics, compiler arguments, and source hashes.

With `SAITEKIKUUKAN_BUILD_BENCHMARKS=ON`, CMake also builds `performance-benchmark`;
enabling reflection tests adds `performance-reflection-benchmark`.
Kernels use separate translation units without LTO to keep the measured work from being folded away.
CI runs sanitizer tests on Clang/C++23 and GCC 16/reflection, then uploads unsanitized performance reports.
It runs automatically when a GitHub Release is published, including pre-releases, and can also be started manually from Actions.
Shared-runner timings have no fixed pass/fail threshold; compare CSV and generated code under matching conditions.

## Future Features (Checklist)

- [ ] SIMD Optimization  
- [ ] GPU Parallel Processing
---
