#include "saitekikuukan.hpp"
#include "../buffer-test/checks.hpp"

#include <deque>
#include <string>

using buffer_test::check;

struct select_second {
    template<typename Parent>
    static constexpr decltype(auto) get(Parent& parent, std::size_t i) { return (parent[i].second); }
};

void check_projected_iterator() {
    std::array<std::pair<int, std::string>, 3> values{{{0, "ccc"}, {1, "a"}, {2, "bb"}}};
    using iterator = kk::_detail::indexed_iterator<decltype(values), select_second>;
    using const_iterator = kk::_detail::indexed_iterator<const decltype(values), select_second>;
    static_assert(std::random_access_iterator<iterator> && std::random_access_iterator<const_iterator>);
    static_assert(std::same_as<std::iter_value_t<iterator>, std::string>);
    static_assert(std::same_as<std::iter_reference_t<const_iterator>, const std::string&>);
    iterator first{&values, 0}, last{&values, 3};
    const_iterator constant = first;
    check(first->size() == 3 && constant == first);
    std::ranges::sort(std::ranges::subrange(first, last));
    check(values[0] == std::pair{0, std::string("a")} && first[2] == "ccc");
}

template<typename T>
concept temporary_columns = requires(T value) { std::move(value).columns(); };
static_assert(!temporary_columns<kk::soa<int>>);

struct deque_layout {
    static constexpr bool resizable = true;
    static constexpr std::size_t max_size = 64;
    template<typename T> using column = std::deque<T>;
};

template<typename Values>
void check_rows() {
    Values values;
    static_assert(std::random_access_iterator<typename Values::iterator>);
    static_assert(std::random_access_iterator<typename Values::const_iterator>);
    static_assert(std::ranges::random_access_range<Values>);
    static_assert(std::same_as<decltype(std::as_const(values).template column<0>()), std::span<const int>>);
    values.reserve(4);
    values.push_back(1, std::string("first"));
    values.push_back(typename Values::value_type{2, "second"});
    values.resize(3);
    check(values.size() == 3 && values.capacity() >= 4);
    check(values.read(0) == std::tuple{1, std::string("first")});
    check(values.read(2) == std::tuple{0, std::string{}});
    auto [x, name] = values.columns();
    x[0] = 10;
    name[0] = "changed";
    auto [row_x, row_name] = values[1];
    row_x = 20;
    row_name = "row";
    check(std::get<0>(*values.begin()) == 10);
    typename Values::const_iterator converted = values.begin();
    check(std::get<0>(*converted) == 10);
    check(values.end() - values.begin() == 3);
    values.write(2, {30, "written"});
    values.erase_unordered(0);
    check(values.size() == 2 && std::get<0>(values.read(0)) == 30);
    auto copied = values;
    copied = std::as_const(copied);
    copied.template get<0>()[0] = 99;
    check(values.template get<0>()[0] == 30);
    auto moved = std::move(copied);
    check(copied.empty() && copied.template get<0>().empty());
    copied.push_back(42, "reused");
    check(copied.size() == 1 && moved.template get<0>()[0] == 99);
    try { (void) values.at(2); std::abort(); } catch (const std::out_of_range&) {}
    values.clear();
    check(values.empty() && values.template get<1>().empty());
}

void check_aliasing() {
    kk::soa<std::string, std::string> values;
    const std::string a(256, 'a'), b(256, 'b');
    values.push_back(a, b);
    while (values.size() < values.capacity()) values.push_back(a, b);
    values.push_back(values.template get<1>()[0], values.template get<0>()[0]);
    check(values.read(values.size() - 1) == std::tuple{b, a});
    values.write(0, values.read(values.size() - 1));
    check(values.read(0) == std::tuple{b, a});


}

void check_limits() {
    kk::fixed_soa<2, int> fixed;
    fixed.resize(2);
    try { fixed.push_back(3); std::abort(); } catch (const std::length_error&) {}
    try { fixed.resize(3); std::abort(); } catch (const std::length_error&) {}
    check(fixed.size() == 2);
}

#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
struct vertex {
    float x = 1;
    float y = 2;
    std::array<float, 3> normal{0, 0, 1};
    std::string name = "default";
};
struct empty_record {};
struct bit_record { bool active = true; };
// ID なしの行では、buffer 用のメタデータ名も通常の列名として使える。
struct metadata_names { int id = 1; int get = 2; int parent = 3; int index = 4; };
struct xy { float x = 0; float y = 0; };
using xy_values = kk::record_soa<xy, kk::storage::fixed_layout<1024>>;

// 列・行・矢印アクセスの実行結果と、最適化時のベクトル化を比較できるようにする。
extern "C" void update_soa_columns(xy_values& values, float dx, float dy) {
    auto columns = values.columns();
    for (std::size_t i = 0; i < values.size(); ++i) {
        columns.x[i] += dx;
        columns.y[i] += dy;
    }
}
extern "C" void update_soa_rows(xy_values& values, float dx, float dy) {
    for (auto row : values) {
        row.x += dx;
        row.y += dy;
    }
}
extern "C" void update_soa_arrows(xy_values& values, float dx, float dy) {
    for (auto it = values.begin(); it != values.end(); ++it) {
        it->x += dx;
        it->y += dy;
    }
}

template<typename Layout>
void check_records() {
    kk::record_soa<vertex, Layout> values;
    static_assert(std::ranges::random_access_range<decltype(values)>);
    values.resize(3);
    check(values[0].normal[2] == 1 && values[1].name == "default");
    auto columns = values.columns();
    static_assert(std::same_as<decltype(columns.x), std::span<float>>);
    columns.x[0] = 10;
    check(values.begin()->x == 10 && values.begin()->y == 2);
    values.template column<^^vertex::y>()[1] = 20;
    check((values.begin() + 1)->y == 20);
    const auto& const_values = values;
    static_assert(std::same_as<decltype(const_values.columns().x), std::span<const float>>);
    static_assert(std::same_as<decltype(const_values[0].x), const float&>);
    static_assert(std::same_as<decltype(const_values.template column<^^vertex::x>()), std::span<const float>>);
    check(const_values.begin()->x == 10);
    auto row = values.read(1);
    row.name = "updated";
    values.write(0, row);
    check(values[0].y == 20 && values[0].name == "updated");
    values.push_back(vertex{});
    check(values[3].normal[2] == 1);
    values.erase_unordered(0);
    check(values[0].name == "default");

    kk::record_buffer<vertex, kk::generator::iota, kk::eventsystem::dummy,
        kk::storage::dynamic_storage, 8> ids;
    check(ids.resize(2).has_value());
    check(ids.begin()->normal[2] == 1 && ids.begin()->name == "default");
    static_assert(std::same_as<decltype(ids.columns().x), std::span<float>>);
    check(ids.column<0>().size() == 2 && ids.columns().normal[0][2] == 1);
}
#endif

int main() {
    check_projected_iterator();
    check_rows<kk::soa<int, std::string>>();
    check_rows<kk::fixed_soa<8, int, std::string>>();
    check_aliasing();
    check_limits();

    kk::soa<> empty;
    empty.resize(10);
    empty.push_back();
    check(empty.size() == 11 && empty.end() - empty.begin() == 11);
    empty.erase_unordered(0);
    check(empty.size() == 10);
    kk::fixed_soa<4> fixed_empty;
    fixed_empty.resize(4);
    check(fixed_empty.size() == 4 && fixed_empty.capacity() == 4);

    kk::soa<bool, int> bits;
    bits.push_back(true, 7);
    bits.column<0>()[0] = false;
    check(!std::get<0>(bits[0]) && std::get<1>(bits.read(0)) == 7);
    check(!std::as_const(bits).column<0>()[0]);

    static_assert(!std::is_copy_constructible_v<kk::soa<std::unique_ptr<int>>>);
    kk::soa<std::unique_ptr<int>> movable;
    movable.push_back(std::make_unique<int>(42));
    check(*movable.get<0>()[0] == 42);

    kk::basic_soa<deque_layout, int> custom;
    custom.push_back(12);
    custom.resize(4);
    check(custom.size() == 4 && custom.column<0>()[0] == 12);
    kk::basic_soa<deque_layout, std::unique_ptr<int>> custom_movable;
    custom_movable.push_back(std::make_unique<int>(23));
    auto custom_moved = std::move(custom_movable);
    check(custom_movable.empty() && *custom_moved.column<0>()[0] == 23);

#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
    check_records<kk::storage::dynamic_layout<>>();
    check_records<kk::storage::fixed_layout<8>>();
    kk::record_soa<empty_record> records;
    records.resize(7);
    check(records.size() == 7 && records.end() - records.begin() == 7);
    kk::record_soa<metadata_names> named;
    named.resize(1);
    check(named.begin()->id == 1 && named.columns().get[0] == 2);
    kk::record_soa<bit_record> record_bits;
    record_bits.resize(2);
    record_bits.columns().active[0] = false;
    check(!record_bits.begin()->active && record_bits[1].active);
    check(!std::as_const(record_bits).columns().active[0]);
    xy_values positions;
    positions.resize(1024);
    update_soa_columns(positions, 1, 2);
    update_soa_rows(positions, 3, 4);
    update_soa_arrows(positions, 5, 6);
    for (auto row : positions) check(row.x == 9 && row.y == 12);
#endif
}
