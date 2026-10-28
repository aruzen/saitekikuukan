#include "saitekikuukan.hpp"
#include "../buffer-test/checks.hpp"

using buffer_test::check;
using row = std::tuple<int, std::string>;

template<typename Values>
auto snapshot_ids(const Values& values) {
    if constexpr (Values::has_identity) {
        std::vector<std::pair<typename Values::id_type, row>> result;
        for (std::size_t i = 0; i < values.size(); ++i)
            result.emplace_back(values[i].id(), values.read(i));
        return result;
    } else {
        return std::tuple{};
    }
}

template<typename Layout, typename Identity, typename Structure = kk::strategy::structure_of_array>
void check_algorithms() {
    kk::buffer<kk::type_list<int, std::string>, Structure,
        Layout, Identity> values;
    const std::array<row, 4> rows{{{3, "three"}, {1, "first"}, {2, "two"}, {1, "other"}}};
    check(values.append_range(rows).value() == 4);
    const auto original = snapshot_ids(values);
    auto key = [](const auto& value) {
        if constexpr (requires { value.template get<0>(); })
            return value.template get<0>();
        else
            return std::get<0>(value);
    };
    values.sort(std::ranges::less{}, key).value();
    for (std::size_t i = 1; i < values.size(); ++i)
        check(values.template column<0>()[i - 1] <= values.template column<0>()[i]);
    if constexpr (decltype(values)::has_identity)
        for (const auto& [id, expected] : original)
            check(values.find(id)->template get<0>() == std::get<0>(expected) &&
                values.find(id)->template get<1>() == std::get<1>(expected));
    const std::vector<row> sorted{values.read(0), values.read(1), values.read(2), values.read(3)};
    check(values.append_range(values).value() == 4);
    for (std::size_t i = 0; i < 4; ++i)
        check(values.read(i) == sorted[i] && values.read(i + 4) == sorted[i]);
    check(values.erase_if([&](auto value) { return key(value) == 1; }).value() == 4);
    if constexpr (decltype(values)::has_identity)
        for (const auto& [id, expected] : original)
            check(values.find(id).has_value() == (std::get<0>(expected) != 1));
    check(values.size() == 4);
    check(values.erase_if([](auto) { return true; }).value() == 4);
    check(values.empty());
    values.sort(std::ranges::less{}, key).value();
    check(values.erase_if([](auto) { return true; }).value() == 0);
}

template<typename Function>
void check_throws(Function&& function) {
    try {
        function();
        std::abort();
    } catch (const std::runtime_error&) {
    }
}

void check_aliasing_and_exceptions() {
    kk::soa<int> values;
    const std::array input{3, 1, 2};
    values.append_range(input).value();
    values.append_range(values.column<0>()).value();
    check(values.size() == 6 && values.column<0>()[3] == 3 && values.column<0>()[5] == 2);
    const auto before = std::vector<int>(values.column<0>().begin(), values.column<0>().end());
    check_throws([&] {
        values.sort([](auto, auto) -> bool { throw std::runtime_error("compare"); }).value();
    });
    check(std::ranges::equal(values.column<0>(), before));
    auto throwing_input = input | std::views::transform([](int value) {
        if (value == 1)
            throw std::runtime_error("input");
        return value;
    });
    check_throws([&] { values.append_range(throwing_input).value(); });
    check(std::ranges::equal(values.column<0>(), before));
    kk::fixed_soa<4, int> limited;
    limited.append_range(input).value();
    check(limited.append_range(input).error() == kk::error_code::buffer_over);
    check(limited.size() == 3);
    auto unsized = input | std::views::filter([](int) { return true; });
    check(limited.append_range(unsized).error() == kk::error_code::buffer_over);
    check(limited.size() == 3);
    check_throws([&] {
        values.erase_if([](auto) -> bool { throw std::runtime_error("predicate"); }).value();
    });
    check(std::ranges::equal(values.column<0>(), before));
}

void check_move_only() {
    using pointer = std::unique_ptr<int>;
    std::vector<std::tuple<pointer>> input;
    input.emplace_back(std::make_unique<int>(3));
    input.emplace_back(std::make_unique<int>(1));
    input.emplace_back(std::make_unique<int>(2));
    kk::chunked_soa<2, pointer> values;
    auto moving = std::ranges::subrange(std::make_move_iterator(input.begin()),
        std::make_move_iterator(input.end()));
    check(values.append_range(moving).value() == 3);
    values.sort(std::ranges::less{}, [](auto value) { return *std::get<0>(value); }).value();
    check(*values.column<0>()[0] == 1 && *values.column<0>()[2] == 3);
    check(values.erase_if([](auto value) { return *std::get<0>(value) == 2; }).value() == 1);
    check(values.size() == 2);
}

using inserting = kk::eventsystem::event::will_insert_row_event<int>;
using deleting = kk::eventsystem::event::will_delete_row_event<int>;
using swapping = kk::eventsystem::event::will_swap_row_event<int>;
struct events {
    using event_types = kk::type_list<inserting, deleting, swapping>;
    int insert_budget = -1, delete_budget = -1, swap_budget = -1;
    template<typename Event>
    void budget(Event& event, int& remaining) {
        if (remaining == 0)
            event.cancel();
        else if (remaining > 0)
            --remaining;
    }
    void on(inserting& event) { budget(event, insert_budget); }
    void on(deleting& event) { budget(event, delete_budget); }
    void on(swapping& event) { budget(event, swap_budget); }
};

void check_cancellation() {
    kk::buffer<kk::type_list<int>, kk::strategy::structure_of_array,
        kk::storage::dynamic_layout<>, kk::identity::none, events> values;
    const std::array input{3, 2, 1};
    values.events().insert_budget = 1;
    check(values.append_range(input).error() == kk::error_code::operation_canceled);
    check(values.size() == 1 && values.column<0>()[0] == 3);
    values.events().insert_budget = -1;
    values.clear().value();
    values.append_range(input).value();
    values.events().swap_budget = 0;
    check(values.sort([](auto a, auto b) { return std::get<0>(a) < std::get<0>(b); }).error() ==
        kk::error_code::operation_canceled);
    check(std::ranges::equal(values.column<0>(), input));
    values.events().swap_budget = -1;
    values.sort([](auto a, auto b) { return std::get<0>(a) < std::get<0>(b); }).value();
    values.events().delete_budget = 1;
    check(values.erase_if([](auto) { return true; }).error() == kk::error_code::operation_canceled);
    check(values.size() == 2);
}

#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
struct particle { int x; std::string name; };
void check_named() {
    using schema = kk::buffer<particle>;
    schema source;
    source.append_range(std::array{particle{3, "three"}, particle{1, "one"}, particle{2, "two"}}).value();
    using identity = kk::identity::recycled<kk::strategy::sparse_id_lookup<2>>;
    kk::buffer<particle, kk::strategy::structure_of_array, kk::storage::chunked_layout<2>,
        identity> values;
    values.append_range(source).value();
    auto id = values.begin()->id();
    values.sort(std::ranges::less{}, [](auto value) {
        static_assert(std::same_as<decltype(value.x), const int&>);
        return value.x;
    }).value();
    check(values.find(id)->name == "three" && values.index(id).value() == 2);
    values.append_range(values).value();
    check(values.erase_if([](auto value) { return value.x == 1; }).value() == 2);
    check(values.size() == 4 && values.find(id)->x == 3);
}
#endif

int main() {
    check_algorithms<kk::storage::dynamic_layout<12>, kk::identity::none>();
    check_algorithms<kk::storage::fixed_layout<12>, kk::identity::none>();
    check_algorithms<kk::storage::chunked_layout<2>, kk::identity::none>();
    check_algorithms<kk::storage::dynamic_layout<12>, kk::identity::generated<>>();
    check_algorithms<kk::storage::fixed_layout<12>, kk::identity::generated<>>();
    check_algorithms<kk::storage::chunked_layout<2>, kk::identity::generated<>>();
    using recycled = kk::identity::recycled<kk::strategy::sparse_id_lookup<2>>;
    check_algorithms<kk::storage::dynamic_layout<12>, recycled>();
    check_algorithms<kk::storage::fixed_layout<12>, recycled>();
    check_algorithms<kk::storage::chunked_layout<2>, recycled>();
    using aos = kk::strategy::array_of_structure;
    check_algorithms<kk::storage::dynamic_layout<12>, kk::identity::none, aos>();
    check_algorithms<kk::storage::fixed_layout<12>, kk::identity::none, aos>();
    check_algorithms<kk::storage::dynamic_layout<12>, recycled, aos>();
    check_algorithms<kk::storage::fixed_layout<12>, recycled, aos>();
    check_aliasing_and_exceptions();
    check_move_only();
    check_cancellation();
#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
    check_named();
#endif
}
