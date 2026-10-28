#include "saitekikuukan.hpp"
#include "../buffer-test/checks.hpp"

#include <deque>

using buffer_test::check;
using aos = kk::strategy::array_of_structure;

template<typename T>
concept temporary_columns = requires(T value) { std::move(value).columns(); };

template<typename Layout>
void check_rows() {
    using values_type = kk::buffer<kk::type_list<int, std::string>, aos, Layout>;
    using row = typename values_type::value_type;
    static_assert(std::ranges::contiguous_range<values_type>);
    static_assert(std::same_as<decltype(std::declval<values_type&>()[0]), row&>);
    static_assert(std::same_as<decltype(*std::declval<const values_type&>().begin()), const row&>);
    static_assert(!temporary_columns<values_type>);
    values_type values;
    values.reserve(4);
    values.push_back(1, "first").value();
    values.push_back(row{2, "second"}).value();
    values.resize(3).value();
    check(values.read(2) == row{0, ""});
    auto [numbers, names] = values.columns();
    static_assert(std::ranges::random_access_range<decltype(numbers)>);
    static_assert(!std::ranges::contiguous_range<decltype(numbers)>);
    static_assert(std::same_as<decltype(numbers[0]), int&>);
    check(&numbers[0] == &std::get<0>(values[0]));
    check(&values[0] + 1 == &values[1]);
    check(std::ranges::data(values) == &values[0]);
    numbers[1] = 20;
    names[1] = "changed";
    for (auto&& [number, name] : values)
        ++number;
    check(values.read(1) == row{21, "changed"});
    const auto& constant = values;
    const auto columns = constant.columns();
    static_assert(std::same_as<decltype(std::get<0>(columns)[0]), const int&>);
    typename values_type::const_iterator converted = values.begin();
    check(converted == constant.begin());
    check(constant.template column<0>().begin() == values.template column<0>().begin());
    values.write(0, {9, "written"}).value();
    auto copy = values;
    copy[0] = {99, "copy"};
    check(std::get<0>(values[0]) == 9);
    copy = values;
    check(copy.read(0) == values.read(0));
    auto moved = std::move(copy);
    check(copy.empty() && moved.read(0) == row{9, "written"});
    copy.push_back(7, "reused").value();
    check(copy.size() == 1);
    try {
        (void)values.at(values.size());
        std::abort();
    } catch (const std::out_of_range&) {
    }
    values.erase_unordered(0).value();
    check(values.read(0) == row{1, ""});
    values.clear().value();
    check(values.empty() && values.template column<0>().empty());
}

struct deque_layout {
    static constexpr bool resizable = true;
    static constexpr std::size_t max_size = 8;
    template<typename T>
    using column = std::deque<T>;
};

struct fragile {
    inline static bool fail_default = false, fail_copy = false;
    int value = 7;

    fragile() {
        if (fail_default)
            throw std::runtime_error("default");
    }

    fragile(const fragile& other) : value(other.value) {
        if (fail_copy)
            throw std::runtime_error("copy");
    }

    fragile(fragile&&) noexcept = default;
    fragile& operator=(const fragile&) = default;
    fragile& operator=(fragile&&) noexcept = default;
};

template<typename Function>
void check_throws(Function&& function) {
    try {
        function();
        std::abort();
    } catch (const std::runtime_error&) {
    }
}

void check_lifetimes() {
    kk::buffer<kk::type_list<std::string, std::string>, aos> alias;
    std::string first(512, 'a'), second(512, 'b');
    alias.push_back(first, second).value();
    while (alias.size() < alias.capacity())
        alias.push_back(first, second).value();
    alias.push_back(alias.column<1>()[0], alias.column<0>()[0]).value();
    check(alias.read(alias.size() - 1) == std::tuple{second, first});

    using pointers = kk::buffer<kk::type_list<std::unique_ptr<int>>, aos>;
    static_assert(!std::is_copy_constructible_v<pointers>);
    pointers owned;
    owned.push_back(std::make_unique<int>(3)).value();
    owned.push_back(std::make_unique<int>(1)).value();
    owned.sort(std::ranges::less{}, [](const auto& row) { return *std::get<0>(row); }).value();
    check(*owned.column<0>()[0] == 1 && *owned.column<0>()[1] == 3);
    owned.erase_unordered(0).value();
    check(*owned.column<0>()[0] == 3);

    kk::buffer<kk::type_list<int, fragile>, aos> values;
    values.push_back(1, fragile{}).value();
    fragile::fail_default = true;
    check_throws([&] { values.resize(4).value(); });
    fragile::fail_default = false;
    check(values.size() == 1 && values.template column<0>()[0] == 1);
    fragile item;
    fragile::fail_copy = true;
    check_throws([&] { values.push_back(2, item).value(); });
    fragile::fail_copy = false;
    check(values.size() == 1);
    decltype(values) target;
    target.push_back(99, fragile{}).value();
    fragile::fail_copy = true;
    check_throws([&] { target = values; });
    fragile::fail_copy = false;
    check(target.size() == 1 && target.column<0>()[0] == 99);
}

void check_special_columns() {
    kk::buffer<kk::type_list<bool>, aos> bits;
    bits.push_back(true).value();
    static_assert(std::same_as<decltype(bits.column<0>()[0]), bool&>);
    bits.column<0>()[0] = false;
    check(!std::get<0>(bits[0]));
    kk::buffer<kk::type_list<>, aos> empty;
    empty.resize(3).value();
    empty.push_back().value();
    check(empty.size() == 4 && std::tuple_size_v<decltype(empty.columns())> == 0);
    kk::buffer<kk::type_list<int>, aos, kk::storage::fixed_layout<0>> zero;
    check(zero.push_back(1).error() == kk::error_code::buffer_over);
    kk::buffer<kk::type_list<int>, aos, kk::storage::fixed_layout<2>> limited;
    limited.resize(2).value();
    check(limited.push_back(3).error() == kk::error_code::buffer_over);
    check(limited.resize(3).error() == kk::error_code::buffer_over);
    kk::buffer<kk::type_list<int>, aos, deque_layout> custom;
    custom.resize(3).value();
    custom.column<0>()[1] = 8;
    check(std::get<0>(custom[1]) == 8);
    static_assert(std::ranges::random_access_range<decltype(custom)>);
    for (auto&& row : custom)
        ++std::get<0>(row);
    check(std::get<0>(custom[1]) == 9);
}

#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
struct particle {
    float x = 1, y = 2;
    std::array<int, 2> normal{3, 4};
    std::string name = "default";
};

struct empty_record {};

struct metadata_names {
    int id = 1, get = 2, parent = 3, index = 4;
};

template<typename Layout>
void check_named() {
    kk::buffer<particle, aos, Layout> values;
    static_assert(std::same_as<decltype(*values.begin()), particle&>);
    values.resize(3).value();
    auto storage = values.columns();
    storage.x[0] = 10;
    auto [x, y, normal, name] = storage;
    y[1] = 20;
    values.template column<^^particle::name>()[2] = "last";
    check(values.begin()->x == 10 && values[1].y == 20 && values[2].name == "last");
    check(values[0].normal[1] == 4 && normal[0][0] == 3);
    check(&storage.x[1] == &values[1].x && &values[0] + 1 == &values[1]);
    auto constant = std::as_const(values).columns();
    static_assert(std::same_as<decltype(constant.x[0]), const float&>);
    static_assert(std::same_as<decltype(*std::as_const(values).begin()), const particle&>);
    values.sort(std::ranges::less{}, [](const auto& row) { return row.y; }).value();
    values.append_range(values).value();
    check(values.size() == 6);

    using ids = kk::identity::recycled<kk::strategy::sparse_id_lookup<2>>;
    kk::buffer<particle, aos, Layout, ids> entities;
    entities.push_back(particle{3, 4, {5, 6}, "first"}).value();
    auto id = entities.begin()->id();
    entities.push_back(particle{7, 8, {9, 10}, "second"}).value();
    entities.swap_rows(0, 1).value();
    entities.find(id)->x = 30;
    check(entities.index(id).value() == 1 && entities.read(1).x == 30);
    static_assert(std::same_as<decltype(std::as_const(entities).find(id)->x), const float&>);
    auto copy = entities;
    check(copy.find(id)->name == "first");
    entities.erase(id).value();
    entities.push_back(particle{}).value();
    check(!entities.find(id));
}
#endif

int main() {
    check_rows<kk::storage::dynamic_layout<8>>();
    check_rows<kk::storage::fixed_layout<8>>();
    check_lifetimes();
    check_special_columns();
#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
    check_named<kk::storage::dynamic_layout<8>>();
    check_named<kk::storage::fixed_layout<8>>();
    kk::buffer<empty_record, aos> empty;
    empty.resize(3).value();
    check(empty.size() == 3);
    auto columns = empty.columns();
    (void)columns;
    kk::buffer<metadata_names, aos> names;
    names.resize(1).value();
    check(names.begin()->id == 1 && names.columns().parent[0] == 3);
#endif
}
