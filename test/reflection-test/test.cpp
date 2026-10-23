#include "saitekikuukan.hpp"
#include "../buffer-test/checks.hpp"

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

struct item {
    int x, y;
    std::string name;
};

struct movable_item {
    std::unique_ptr<int> value;
};

struct initialized_item {
    std::array<float, 3> normal{0, 0, 1};
    std::string name = "default";
};

struct sequenced_item {
    inline static int sequence = 0;
    int number = ++sequence;
    float value = 42;
};
struct destroyed_item {
    inline static int destroyed = 0;
    int value;
    ~destroyed_item() { ++destroyed; }
};

void check_record_resize() {
    kk::record_soa<position> plain;
    plain.resize(8);
    for (auto row : plain) buffer_test::check(row.x == 0 && row.y == 0);
    plain[0].x = 7;
    plain.resize(1);
    plain.resize(4);
    buffer_test::check(plain[0].x == 7 && plain[1].x == 0);

    sequenced_item::sequence = 0;
    kk::record_soa<sequenced_item> sequenced;
    sequenced.resize(3);
    sequenced.resize(5);
    buffer_test::check(sequenced_item::sequence == 5);
    for (std::size_t i = 0; i < sequenced.size(); ++i)
        buffer_test::check(sequenced[i].number == static_cast<int>(i + 1) && sequenced[i].value == 42);

    destroyed_item::destroyed = 0;
    kk::record_soa<destroyed_item> destroyed;
    destroyed.resize(3);
    buffer_test::check(destroyed_item::destroyed == 3);
}

template<template<typename, std::size_t, typename...> typename Storage>
void check_integrated_buffer() {
    kk::record_buffer<initialized_item, kk::generator::iota, kk::eventsystem::dummy, Storage, 4> initialized;
    buffer_test::check(initialized.resize(2).has_value());
    buffer_test::check(initialized.begin()->normal[2] == 1 && initialized.begin()->name == "default");
    initialized.clear();
    buffer_test::check(initialized.resize(1).has_value());
    buffer_test::check(initialized.begin()->normal[2] == 1);

    using values_type = kk::record_buffer<item, kk::generator::iota, kk::eventsystem::dummy, Storage, 4>;
    static_assert(std::is_same_v<values_type,
        kk::buffer<kk::generator::iota, kk::eventsystem::dummy, Storage, 4, kk::record<item>>>);
    buffer_test::check_buffer<values_type>();
    using events_type = kk::record_buffer<item, kk::generator::iota, buffer_test::insert_events, Storage, 2>;
    buffer_test::check_events<events_type>();
    using registry = kk::eventsystem::registration<buffer_test::before_insert>;
    using single = kk::eventsystem::one_event_each_only<buffer_test::before_insert>;
    buffer_test::check_registered_events<kk::record_buffer<item, kk::generator::iota, registry, Storage, 4>>();
    buffer_test::check_registered_events<kk::record_buffer<item, kk::generator::iota, single, Storage, 4>>();

    values_type values{};
    item original{1, 2, "first"};
    buffer_test::check(values.push_back(original).has_value());
    buffer_test::check(original.name == "first");
    buffer_test::check(values.push_back(item{3, 4, "second"}).has_value());
    auto& columns = values.storage.entries;
    buffer_test::check(columns.x[0] == 1 && columns.y[1] == 4 && columns.name[1] == "second");
    auto& [x, y, name] = columns;
    buffer_test::check(std::addressof(x) == std::addressof(values.template get<0>()));
    values.begin()->x = 5;
    values.begin()->name = "updated";
    buffer_test::check(x[0] == 5 && name[0] == "updated");
    buffer_test::check(values.begin()->id() == 0);
    buffer_test::check(values[0].value().x == 5);
    for (auto row : values)
        row.y += 10;
    buffer_test::check(y[0] == 12 && y[1] == 14 && values.size() == 2);
    const auto& constant = values;
    static_assert(std::is_same_v<decltype(constant.begin()->x), const int&>);
    static_assert(std::is_same_v<decltype(constant[0].value().name), const std::string&>);
    buffer_test::check(constant.begin()->name == "updated");
    buffer_test::check(values.erase(0).has_value());
    buffer_test::check(values.begin()->id() == 1 && values.begin()->name == "second");

    kk::record_buffer<movable_item, kk::generator::iota, kk::eventsystem::dummy, Storage, 4> movable{};
    buffer_test::check(movable.push_back(movable_item{std::make_unique<int>(42)}).has_value());
    buffer_test::check(*movable.begin()->value == 42);

    kk::record_buffer<empty, kk::generator::iota, kk::eventsystem::dummy, Storage, 4> no_fields{};
    buffer_test::check(no_fields.push_back(empty{}).has_value());
    buffer_test::check(no_fields.push_back(empty{}).has_value());
    buffer_test::check(no_fields.end() - no_fields.begin() == 2);
    buffer_test::check(no_fields.begin()->id() == 0);
}

template<typename T>
using small_column = std::array<T, 2>;

void check(bool condition) {
    if (!condition)
        std::abort();
}

using positions = kk::storage::static_soa<position, 10240>;

// 同じ生成ストレージへの名前付きアクセスと構造化束縛を、別関数として比較する。
// 関数を外部に公開して、ベクトル化診断と生成コードを確認できるようにする。
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

using position_buffer = kk::record_buffer<position, kk::generator::iota,
    kk::eventsystem::dummy, kk::storage::static_storage, 10240>;

extern "C" void update_buffer_direct(position_buffer& values, float dx, float dy) {
    auto& columns = values.storage.entries;
    for (std::size_t i = 0; i < values.size(); ++i) {
        columns.x[i] += dx;
        columns.y[i] += dy;
    }
}

extern "C" void update_buffer_iterator(position_buffer& values, float dx, float dy) {
    for (auto it = values.begin(); it != values.end(); ++it) {
        it->x += dx;
        it->y += dy;
    }
}

int main() {
    check_record_resize();
    check_integrated_buffer<kk::storage::static_storage>();
    check_integrated_buffer<kk::storage::dynamic_storage>();
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
    // 列長が異なる場合は zip と同じく最短の列長まで走査する。
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
        row.active = true; // vector<bool> のプロキシ経由でも元の列に書き込む。
    check(flags.active[0] && flags.active[1]);
    auto const_flags = kk::storage::rows(std::as_const(flags));
    static_assert(std::is_same_v<decltype((*const_flags.begin()).active), bool>);
    check(const_flags.begin()->active);

    kk::storage::soa<nested, small_column> custom{};
    static_assert(std::is_same_v<decltype(custom.point), std::array<position, 2>>);
    auto& [points, values] = custom;
    points[0] = {6.0f, 7.0f};
    check(custom.point[0].x == 6.0f && custom.point[0].y == 7.0f);
    check(values[0] == 0); // 元の構造体のメンバ初期化子は引き継がない。

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

    position_buffer direct{}, iterated{};
    check(direct.resize(128).has_value() && iterated.resize(128).has_value());
    update_buffer_direct(direct, 2.0f, 3.0f);
    update_buffer_iterator(iterated, 2.0f, 3.0f);
    for (std::size_t i = 0; i < direct.size(); ++i) {
        check(direct.storage.entries.x[i] == iterated.storage.entries.x[i]);
        check(direct.storage.entries.y[i] == iterated.storage.entries.y[i]);
    }
    check(iterated.storage.entries.x[0] == 2.0f && iterated.storage.entries.y[0] == 3.0f);
    check(iterated.storage.entries.x[128] == 0.0f && iterated.storage.entries.y[128] == 0.0f);
}
