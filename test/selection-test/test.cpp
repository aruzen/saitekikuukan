#include "saitekikuukan.hpp"
#include "../buffer-test/checks.hpp"

#include <deque>

using buffer_test::check;

template<typename T>
concept temporary_selection = requires(T value) { std::move(value).template select<0>(); };

template<typename T>
concept accepts_wrong_callback = requires(T view) { view.for_each([](std::string&) {}); };

template<typename Layout, typename Identity, typename Structure = kk::strategy::structure_of_array>
void check_selection() {
    using values_type =
        kk::buffer<kk::type_list<int, float, std::string>, Structure, Layout, Identity>;
    static_assert(!temporary_selection<values_type>);
    static_assert(!temporary_selection<const values_type>);
    values_type values;
    for (int i = 0; i < 7; ++i)
        values.push_back(i, static_cast<float>(i), "untouched").value();
    auto selected = values.template select<1, 0>();
    using view_type = decltype(selected);
    static_assert(std::ranges::view<view_type>);
    static_assert(std::ranges::borrowed_range<view_type>);
    static_assert(std::ranges::random_access_range<view_type>);
    static_assert(std::ranges::sized_range<view_type>);
    static_assert(std::ranges::common_range<view_type>);
    static_assert(!std::ranges::contiguous_range<view_type>);
    static_assert(!accepts_wrong_callback<view_type>);
    static_assert(std::same_as<decltype(std::get<0>(selected[0])), float&>);
    static_assert(std::same_as<decltype(std::get<1>(selected[0])), int&>);
    check(selected.size() == 7 && selected.end() - selected.begin() == 7);
    check(&std::get<0>(selected[2]) == &values.template column<1>()[2]);
    auto last = selected.end();
    --last;
    check(std::get<1>(*last) == 6 && last - selected.begin() == 6);
    check(std::get<1>(selected.begin()[3]) == 3);
    check(selected.begin() + 3 == 3 + selected.begin());
    check(selected.begin() < selected.end());

    // 同じ callable を lvalue として呼び、move-only な状態もコピーしない。
    auto update = [count = std::make_unique<int>(0)](float& y, int& x) mutable {
        check(x == *count);
        ++*count;
        x += 10;
        y += 20;
    };
    selected.for_each(update);
    const auto shallow = selected;
    shallow.for_each([](float& y, int& x) { y += x; });
    for (auto&& [y, x] : selected)
        ++x;
    for (int i = 0; i < 7; ++i) {
        check(values.template column<0>()[i] == i + 11);
        check(values.template column<1>()[i] == i * 2 + 30);
        check(values.template column<2>()[i] == "untouched");
        if constexpr (values_type::has_identity) {
            auto id = values[i].id();
            check(values.index(id).value() == static_cast<std::size_t>(i));
            check(values.find(id)->template get<0>() == i + 11);
        }
    }

    auto readonly = std::as_const(values).template select<0, 1>();
    static_assert(std::same_as<decltype(std::get<0>(readonly[0])), const int&>);
    static_assert(std::same_as<decltype(std::get<1>(readonly[0])), const float&>);
    int visits = 0;
    readonly.for_each([&](const int& x, const float& y) {
        check(x == visits + 11 && y == visits * 2 + 30);
        ++visits;
    });
    check(visits == 7);

    auto middle = selected | std::views::drop(2) | std::views::take(3);
    for (auto&& [y, x] : middle)
        x += 100;
    check(values.template column<0>()[1] == 12 && values.template column<0>()[2] == 113);
    auto result =
        std::ranges::for_each(values.template select<0>(), [](auto row) { ++std::get<0>(row); });
    static_assert(!std::same_as<decltype(result.in), std::ranges::dangling>);
    check(result.in == values.template select<0>().end());
    auto borrowed = std::ranges::begin(values.template select<0>());
    check(&std::get<0>(*borrowed) == &values.template column<0>()[0]);
    auto filtered = selected | std::views::filter([](auto row) { return std::get<1>(row) > 100; });
    check(std::ranges::distance(filtered) == 3);

    // 列の重複選択は同じ要素への参照を返す。
    values.template select<0, 0>().for_each([](int& first, int& second) {
        check(&first == &second);
        ++first;
    });
    visits = 0;
    values.template select<>().for_each([&] { ++visits; });
    check(visits == 7 && values.template select<>().size() == 7);

    view_type empty;
    check(empty.empty() && empty.begin() == empty.end());
    empty.for_each([](float&, int&) { std::abort(); });
    values.clear().value();
    values.template select<0, 1>().for_each([](int&, float&) { std::abort(); });
}

struct deque_layout {
    static constexpr bool resizable = true;
    static constexpr std::size_t max_size = 16;
    template<typename T>
    using column = std::deque<T>;
};

template<typename Layout>
void check_exceptions() {
    kk::basic_soa<Layout, int> values;
    values.resize(7).value();
    int visited = 0;
    try {
        values.template select<0>().for_each([&](int& value) {
            if (visited == 4)
                throw std::runtime_error("callback");
            value = ++visited;
        });
        std::abort();
    } catch (const std::runtime_error&) {
    }
    check(visited == 4 && values.size() == 7);
    for (int i = 0; i < 7; ++i)
        check(values.template column<0>()[i] == (i < 4 ? i + 1 : 0));
}

struct lvalue_function {
    int calls = 0;

    lvalue_function() = default;
    lvalue_function(const lvalue_function&) = delete;

    void operator()(int& value) & { value = ++calls; }

    void operator()(int&) && = delete;
};

void check_special_columns() {
    kk::soa<bool> bits;
    bits.resize(7).value();
    bits.select<0>().for_each([](auto bit) { bit = true; });
    for (auto&& [bit] : bits.select<0>())
        bit = !bit;
    std::as_const(bits).select<0>().for_each([](bool bit) { check(!bit); });

    kk::chunked_soa<3, std::unique_ptr<int>> pointers;
    for (int i = 0; i < 7; ++i)
        pointers.push_back(std::make_unique<int>(i)).value();
    pointers.select<0>().for_each([](std::unique_ptr<int>& pointer) { ++*pointer; });
    check(*pointers.column<0>()[6] == 7);

    kk::soa<> no_columns;
    no_columns.resize(3).value();
    int visited = 0;
    no_columns.select<>().for_each([&] { ++visited; });
    check(visited == 3);
    kk::soa<int> values;
    values.resize(3).value();
    values.select<0>().for_each(lvalue_function{});
    check(values.column<0>()[2] == 3);
}

struct update_events {
    using before = kk::eventsystem::event::will_update_row_event<int>;
    using after = kk::eventsystem::event::did_update_row_event<int>;
    using event_types = kk::type_list<before, after>;
    int calls = 0;

    void on(before&) { ++calls; }

    void on(after&) { ++calls; }
};

void check_events() {
    kk::buffer<kk::type_list<int>, kk::strategy::structure_of_array, kk::storage::dynamic_layout<>,
        kk::identity::none, update_events>
        values;
    values.resize(3).value();
    values.select<0>().for_each([](int& value) { ++value; });
    for (auto&& [value] : values.select<0>())
        ++value;
    check(values.column<0>()[0] == 2 && values.events().calls == 0);
}

#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
struct particle {
    int x = 1;
    float y = 2;
    std::string name = "untouched";
};

template<typename Layout, typename Identity, typename Structure = kk::strategy::structure_of_array>
void check_named() {
    kk::buffer<particle, Structure, Layout, Identity> values;
    values.resize(7).value();
    auto selected = values.template select<^^particle::y, ^^particle::x>();
    selected.for_each([](float& y, int& x) { y += x++; });
    for (auto&& [y, x] : selected)
        x += 10;
    check(values.read(6).x == 12 && values.read(6).y == 3);
    check(values.read(0).name == "untouched");
    auto readonly = std::as_const(values).template select<^^particle::x>();
    static_assert(std::same_as<decltype(std::get<0>(readonly[0])), const int&>);
    readonly.for_each([](const int& x) { check(x == 12); });
    check(std::get<0>(selected[0]) == std::get<0>(values.template select<1, 0>()[0]));
}
#endif

int main() {
    check_selection<kk::storage::dynamic_layout<16>, kk::identity::none>();
    check_selection<kk::storage::fixed_layout<16>, kk::identity::none>();
    check_selection<kk::storage::chunked_layout<3>, kk::identity::none>();
    check_selection<deque_layout, kk::identity::none>();
    check_selection<kk::storage::dynamic_layout<16>, kk::identity::generated<>>();
    using recycled = kk::identity::recycled<kk::strategy::sparse_id_lookup<2>>;
    check_selection<kk::storage::chunked_layout<3>, recycled>();
    check_exceptions<kk::storage::dynamic_layout<16>>();
    check_exceptions<kk::storage::chunked_layout<3>>();
    check_special_columns();
    check_events();
#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
    check_named<kk::storage::dynamic_layout<16>, kk::identity::none>();
    check_named<kk::storage::fixed_layout<16>, kk::identity::generated<>>();
    check_named<kk::storage::chunked_layout<3>, recycled>();
#endif
}
