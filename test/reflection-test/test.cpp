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

struct flag {
    bool active;
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

extern "C" void update_rows(positions& storage, float dx, float dy) {
    for (auto row : kk::storage::rows(storage)) {
        row.x += dx;
        row.y += dy;
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

    auto rows = kk::storage::rows(storage);
    using row_iterator = decltype(rows.begin());
    static_assert(std::random_access_iterator<row_iterator>);
    static_assert(std::ranges::random_access_range<decltype(rows)>);
    static_assert(std::ranges::sized_range<decltype(rows)>);
    static_assert(std::ranges::borrowed_range<decltype(rows)>);
    static_assert(std::is_same_v<decltype((*rows.begin()).x), float&>);
    static_assert(std::is_same_v<decltype((*rows.begin()).name), std::string&>);
    check(rows.size() == 4 && rows.end() - rows.begin() == 4);
    check(std::addressof((*rows.begin()).x) == std::addressof(storage.x[0]));
    auto first = rows.begin();
    first->name = "row";
    first[1].x = 8.0f;
    check(storage.name[0] == "row" && storage.x[1] == 8.0f);
    check((first + 2) - first == 2);
    check(first < first + 1 && first + 1 > first);
    auto last = rows.end();
    --last;
    last->y = 9.0f;
    check(storage.y[3] == 9.0f);
    auto previous = first++;
    check(previous == rows.begin() && first == rows.begin() + 1);
    first -= 1;
    check(first == rows.begin());

    visits = 0;
    for (auto row : rows) {
        row.x += 1.0f;
        ++visits;
    }
    check(visits == 4 && storage.x[0] == 2.0f && storage.x[1] == 9.0f);
    auto [row_x, row_y, row_name] = *rows.begin();
    row_y = 10.0f;
    check(storage.y[0] == 10.0f);

    auto constant_rows = kk::storage::rows(constant);
    using const_row_iterator = decltype(constant_rows.begin());
    static_assert(std::random_access_iterator<const_row_iterator>);
    static_assert(std::is_same_v<decltype((*constant_rows.begin()).x), const float&>);
    static_assert(std::is_same_v<decltype((*constant_rows.begin()).name), const std::string&>);
    static_assert(std::is_convertible_v<row_iterator, const_row_iterator>);
    static_assert(!std::is_convertible_v<const_row_iterator, row_iterator>);
    const_row_iterator converted = rows.begin();
    check(converted == constant_rows.begin());
    check(rows.begin() == constant_rows.begin());
    check(constant_rows.end() - rows.begin() == 4);
    check(converted->name == "row");
    check(kk::storage::rows(copy).begin() != rows.begin());
    auto borrowed = std::ranges::find_if(kk::storage::rows(storage),
        [](auto row) { return row.name == "row"; });
    check(borrowed == rows.begin());
    check(std::ranges::distance(rows | std::views::take(2)) == 2);
    auto reverse = rows | std::views::reverse;
    check(std::addressof((*reverse.begin()).x) == std::addressof(storage.x[3]));

    dynamic_columns dynamic;
    kk::storage::for_each_column(dynamic, [](auto& column) { column.resize(3); });
    dynamic.x[0] = 4.0f;
    dynamic.y[0] = 5.0f;
    dynamic.name[0] = "dynamic";
    auto moved = std::move(dynamic);
    check(moved.x.size() == 3 && moved.x[0] == 4.0f);
    check(moved.y.size() == 3 && moved.y[0] == 5.0f);
    check(moved.name.size() == 3 && moved.name[0] == "dynamic");
    // Unequal column lengths follow zip semantics and stop at the shortest.
    moved.x.resize(2);
    check(kk::storage::rows(moved).size() == 2);
    for (auto row : kk::storage::rows(moved))
        row.name = "visited";
    check(moved.name[1] == "visited" && moved.name[2].empty());
    kk::storage::for_each_column(moved, [](auto& column) { column.clear(); });
    check(moved.x.empty() && moved.y.empty() && moved.name.empty());
    check(kk::storage::rows(moved).empty());

    kk::storage::dynamic_soa<flag> flags;
    flags.active.resize(2);
    for (auto row : kk::storage::rows(flags))
        row.active = true; // vector<bool> element proxies also write through.
    check(flags.active[0] && flags.active[1]);
    auto const_flags = kk::storage::rows(std::as_const(flags));
    static_assert(std::is_same_v<decltype((*const_flags.begin()).active), bool>);
    check(const_flags.begin()->active);

    kk::storage::soa<nested, small_column> custom{};
    static_assert(std::is_same_v<decltype(custom.point), std::array<position, 2>>);
    auto& [points, values] = custom;
    points[0] = {6.0f, 7.0f};
    check(custom.point[0].x == 6.0f && custom.point[0].y == 7.0f);
    check(values[0] == 0); // The original record's initializer is not copied.

    kk::storage::static_soa<empty, 4> no_columns;
    kk::storage::for_each_column(no_columns, [](auto&) { std::abort(); });
    check(kk::storage::rows(no_columns).empty());

    positions a{}, b{}, c{};
    update_named(a, 2.0f, 3.0f);
    update_bound(b, 2.0f, 3.0f);
    update_rows(c, 2.0f, 3.0f);
    for (std::size_t i = 0; i < a.x.size(); ++i) {
        check(a.x[i] == 2.0f && a.x[i] == b.x[i]);
        check(a.y[i] == 3.0f && a.y[i] == b.y[i]);
        check(a.x[i] == c.x[i] && a.y[i] == c.y[i]);
    }
}
