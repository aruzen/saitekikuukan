#include "types.hpp"

namespace {
template<typename Row>
void update_row(Row& row, float x, float y) {
    if constexpr (requires { row.x; }) { row.x += x; row.y += y; }
    else { std::get<0>(row) += x; std::get<1>(row) += y; }
}
template<typename Values>
void update_columns(Values& values, float dx, float dy) {
    auto x = values.template column<0>();
    auto y = values.template column<1>();
    for (std::size_t i = 0; i < values.size(); ++i) { x[i] += dx; y[i] += dy; }
}
template<typename Values>
void update_rows(Values& values, float dx, float dy) {
    for (auto row : values) update_row(row, dx, dy);
}
template<typename Values>
void update_values(Values& values, float dx, float dy) {
    for (std::size_t i = 0; i < values.size(); ++i) {
        auto row = values.read(i);
        update_row(row, dx, dy);
        (void)values.write(i, row);
    }
}
template<typename Values>
void push_rows(Values& values, std::size_t count) {
    for (std::size_t i = 0; i < count; ++i) (void)values.push_back(static_cast<float>(i), static_cast<float>(i + 1));
}
}

extern "C" {
void vectors_resize(performance::vectors& values, std::size_t count) { values.resize(count); }
void vectors_push(performance::vectors& values, std::size_t count) {
    for (std::size_t i = 0; i < count; ++i) {
        values.x.push_back(static_cast<float>(i));
        values.y.push_back(static_cast<float>(i + 1));
        ++values.count;
    }
}
void vectors_columns(performance::vectors& values, float dx, float dy) {
    for (std::size_t i = 0; i < values.count; ++i) { values.x[i] += dx; values.y[i] += dy; }
}
#define DEFINE_SOA_KERNELS(name, type) \
    void name##_resize(type& v, std::size_t n) { (void)v.resize(n); } \
    void name##_push(type& v, std::size_t n) { push_rows(v, n); } \
    void name##_columns(type& v, float x, float y) { update_columns(v, x, y); } \
    void name##_rows(type& v, float x, float y) { update_rows(v, x, y); } \
    void name##_values(type& v, float x, float y) { update_values(v, x, y); }
DEFINE_SOA_KERNELS(tuple, performance::tuple)
#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
DEFINE_SOA_KERNELS(record, performance::record)
DEFINE_SOA_KERNELS(initialized, performance::initialized)
#endif
#undef DEFINE_SOA_KERNELS
}
