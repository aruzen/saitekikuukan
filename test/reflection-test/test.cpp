#include "saitekikuukan_reflection.hpp"

#include <cstdlib>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

struct particle {
    float x, y;
    std::string name;
};

struct position {
    float x, y;
};

struct empty {};

struct nested {
    static constexpr int version = 1;
    position point;
    int value = 42;
};

template<typename T>
using small_column = std::array<T, 2>;

void check(bool condition) {
    if (!condition)
        std::abort();
}

using positions = kk::storage::static_soa<position, 10240>;

// Separate externally visible kernels allow checking vectorization diagnostics
// for named access and structured bindings with the same generated storage.
extern "C" void update_named(positions& storage, float dx, float dy) {
    for (std::size_t i = 0; i < storage.x.size(); ++i) {
        storage.x[i] += dx;
        storage.y[i] += dy;
    }
}

extern "C" void update_bound(positions& storage, float dx, float dy) {
    auto& [x, y] = storage;
    for (std::size_t i = 0; i < x.size(); ++i) {
        x[i] += dx;
        y[i] += dy;
    }
}

int main() {
    using static_columns = kk::storage::static_soa<particle, 4>;
    using dynamic_columns = kk::storage::dynamic_soa<particle>;
    static_assert(std::is_aggregate_v<static_columns>);
    static_assert(std::is_aggregate_v<dynamic_columns>);
    static_assert(std::is_same_v<decltype(static_columns::x), std::array<float, 4>>);
    static_assert(std::is_same_v<decltype(static_columns::name), std::array<std::string, 4>>);
    static_assert(std::is_same_v<decltype(dynamic_columns::x), std::vector<float>>);
    static_assert(std::is_same_v<decltype(dynamic_columns::name), std::vector<std::string>>);
    static_assert(sizeof(positions) == 2 * sizeof(std::array<float, 10240>));
    static_assert(std::is_empty_v<kk::storage::static_soa<empty, 4>>);

    static_columns storage{};
    check(storage.x[0] == 0.0f);
    check(storage.y[0] == 0.0f);
    check(storage.name[0].empty());

    storage.x[0] = 1.0f;
    storage.y[0] = 2.0f;
    storage.name[0] = "first";
    auto& [x, y, name] = storage;
    check(std::addressof(x) == std::addressof(storage.x));
    check(std::addressof(y) == std::addressof(storage.y));
    check(std::addressof(name) == std::addressof(storage.name));
    x[1] = 3.0f;
    check(storage.x[1] == 3.0f);

    const auto& constant = storage;
    static_assert(std::is_same_v<decltype((constant.x)), const std::array<float, 4>&>);
    std::size_t visits = 0;
    kk::storage::for_each_column(constant, [&](const auto& column) {
        check(column.size() == 4);
        ++visits;
    });
    check(visits == 3);

    auto copy = storage;
    copy.name[0] = "copy";
    check(storage.name[0] == "first");
    check(copy.name[0] == "copy");

    dynamic_columns dynamic;
    kk::storage::for_each_column(dynamic, [](auto& column) { column.resize(3); });
    dynamic.x[0] = 4.0f;
    dynamic.y[0] = 5.0f;
    dynamic.name[0] = "dynamic";
    auto moved = std::move(dynamic);
    check(moved.x.size() == 3 && moved.x[0] == 4.0f);
    check(moved.y.size() == 3 && moved.y[0] == 5.0f);
    check(moved.name.size() == 3 && moved.name[0] == "dynamic");
    kk::storage::for_each_column(moved, [](auto& column) { column.clear(); });
    check(moved.x.empty() && moved.y.empty() && moved.name.empty());

    kk::storage::soa<nested, small_column> custom{};
    static_assert(std::is_same_v<decltype(custom.point), std::array<position, 2>>);
    auto& [points, values] = custom;
    points[0] = {6.0f, 7.0f};
    check(custom.point[0].x == 6.0f && custom.point[0].y == 7.0f);
    check(values[0] == 0); // The original record's initializer is not copied.

    kk::storage::static_soa<empty, 4> no_columns;
    kk::storage::for_each_column(no_columns, [](auto&) { std::abort(); });

    positions a{}, b{};
    update_named(a, 2.0f, 3.0f);
    update_bound(b, 2.0f, 3.0f);
    for (std::size_t i = 0; i < a.x.size(); ++i) {
        check(a.x[i] == 2.0f && a.x[i] == b.x[i]);
        check(a.y[i] == 3.0f && a.y[i] == b.y[i]);
    }
}
