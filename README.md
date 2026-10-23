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

---

## Future Features (Checklist)

- [ ] SIMD Optimization  
- [ ] GPU Parallel Processing
---
