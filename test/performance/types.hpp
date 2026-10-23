#pragma once
#include "saitekikuukan.hpp"

namespace performance {
struct vectors {
    std::size_t count = 0;
    std::vector<float> x, y;
    void reserve(std::size_t n) { x.reserve(n); y.reserve(n); }
    void resize(std::size_t n) { x.resize(n); y.resize(n); count = n; }
    void clear() { x.clear(); y.clear(); count = 0; }
    std::size_t size() const { return count; }
    template<std::size_t I> auto column() const {
        if constexpr (I == 0) return std::span<const float>(x);
        else return std::span<const float>(y);
    }
};
using tuple = kk::soa<float, float>;
#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
struct row { float x, y; };
struct initialized_row { float x = 0, y = 0; };
using record = kk::record_soa<row>;
using initialized = kk::record_soa<initialized_row>;
#endif
}

// 別の翻訳単位から呼び、既知の引数によるループ全体の定数畳み込みを避ける。
#define DECLARE_SOA_KERNELS(name, type) \
    void name##_resize(type&, std::size_t); \
    void name##_push(type&, std::size_t); \
    void name##_columns(type&, float, float); \
    void name##_rows(type&, float, float); \
    void name##_values(type&, float, float)
extern "C" {
void vectors_resize(performance::vectors&, std::size_t);
void vectors_push(performance::vectors&, std::size_t);
void vectors_columns(performance::vectors&, float, float);
DECLARE_SOA_KERNELS(tuple, performance::tuple);
#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
DECLARE_SOA_KERNELS(record, performance::record);
DECLARE_SOA_KERNELS(initialized, performance::initialized);
#endif
}
#undef DECLARE_SOA_KERNELS
