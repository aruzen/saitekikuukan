#include "saitekikuukan.hpp"
#include "checks.hpp"

#include <memory>
#include <stdexcept>

struct two_ids {
    using generate_type = std::size_t;
    std::size_t next_id = 0;

    std::optional<std::size_t> next() {
        if (next_id == 2)
            return std::nullopt;
        return next_id++;
    }
};

struct component {
    int x;
};

struct throwing_value {
    inline static bool fail_copy = false;
    inline static bool fail_default = false;
    int value = 7;

    throwing_value() {
        if (fail_default)
            throw std::runtime_error("default construction");
    }

    throwing_value(const throwing_value& other) : value(other.value) {
        if (fail_copy)
            throw std::runtime_error("copy construction");
    }
};

template<template<std::size_t> typename Layout>
void check_strategy() {
    using namespace kk;
    using values =
        kk::buffer<kk::type_list<int, int, std::string>, kk::strategy::structure_of_array,
            Layout<4>, kk::identity::generated<generator::iota>, eventsystem::dummy>;
    buffer_test::check_buffer<values>();
    using events_buffer =
        kk::buffer<kk::type_list<int, int, std::string>, kk::strategy::structure_of_array,
            Layout<2>, kk::identity::generated<generator::iota>, buffer_test::insert_events>;
    buffer_test::check_events<events_buffer>();
    using registry = eventsystem::registration<buffer_test::before_insert>;
    using single = eventsystem::one_event_each_only<buffer_test::before_insert>;
    buffer_test::check_registered_events<
        kk::buffer<kk::type_list<int, int, std::string>, kk::strategy::structure_of_array,
            Layout<4>, kk::identity::generated<generator::iota>, registry>>();
    buffer_test::check_registered_events<
        kk::buffer<kk::type_list<int, int, std::string>, kk::strategy::structure_of_array,
            Layout<4>, kk::identity::generated<generator::iota>, single>>();

    using limited = kk::buffer<kk::type_list<int>, kk::strategy::structure_of_array, Layout<4>,
        kk::identity::generated<two_ids>, eventsystem::dummy>;
    limited ids{};
    buffer_test::check(ids.push_back(1).has_value());
    buffer_test::check(ids.push_back(2).has_value());
    buffer_test::check(ids.push_back(3).error() == error_code::generate_id_failed);
    buffer_test::check(ids.size() == 2);
    limited resized{};
    buffer_test::check(resized.resize(3).error() == error_code::generate_id_failed);
    buffer_test::check(resized.empty());

    kk::buffer<kk::type_list<std::unique_ptr<int>>, kk::strategy::structure_of_array, Layout<4>,
        kk::identity::generated<generator::iota>, eventsystem::dummy>
        movable{};
    buffer_test::check(movable.push_back(std::make_unique<int>(42)).has_value());
    buffer_test::check(*movable.begin()->template get<0>() == 42);
    buffer_test::check(movable.erase(0).has_value());
}

int main() {
    using namespace kk;
    // 条件式の評価と、空・同型を含む型リストの処理を確認する。
    using list = type_list<int, float, long, int>;
    static_assert(std::same_as<list::filter<std::is_integral>, type_list<int, long, int>>);
    static_assert(list::filtered_index<std::is_integral>() == std::array<std::size_t, 3>{0, 2, 3});
    static_assert(type_list<>::filter<std::is_integral>::size == 0);
    static_assert(type_list<>::filtered_index<std::is_integral>().empty());
    static_assert(type_list<float>::filter<std::is_integral>::size == 0);
    static_assert(
        std::same_as<_meta::helper::concat_t<type_list<int>, type_list<float>, type_list<long>>,
            type_list<int, float, long>>);
    check_strategy<storage::fixed_layout>();
    check_strategy<storage::dynamic_layout>();

    kk::buffer<kk::type_list<component>, kk::strategy::structure_of_array,
        kk::storage::fixed_layout<4>, kk::identity::generated<generator::iota>, eventsystem::dummy>
        single{};
    static_assert(std::is_same_v<decltype(single.get<0>()), std::span<component>>);
    buffer_test::check(single.push_back(component{7}).has_value());
    buffer_test::check(single.begin()->get<0>().x == 7);

    kk::buffer<kk::type_list<bool>, kk::strategy::structure_of_array,
        kk::storage::dynamic_layout<4>, kk::identity::generated<generator::iota>,
        eventsystem::dummy>
        bits{};
    buffer_test::check(bits.push_back(true).has_value());
    bits.get<0>(0).value() = false;
    buffer_test::check(!bits.begin()->get<0>());
    buffer_test::check(!std::as_const(bits).get<0>(0).value());

    kk::buffer<kk::type_list<int, throwing_value>, kk::strategy::structure_of_array,
        kk::storage::dynamic_layout<4>, kk::identity::generated<>>
        raw;
    throwing_value value;
    throwing_value::fail_copy = true;
    try {
        (void)raw.push_back(1, value);
        std::abort();
    } catch (const std::runtime_error&) {
    }
    throwing_value::fail_copy = false;
    buffer_test::check(raw.empty() && raw.get<0>().empty() && raw.get<1>().empty());
    buffer_test::check(raw.push_back(1, value).has_value());
    throwing_value::fail_default = true;
    try {
        (void)raw.resize(3);
        std::abort();
    } catch (const std::runtime_error&) {
    }
    throwing_value::fail_default = false;
    buffer_test::check(raw.size() == 1 && raw.get<0>().size() == 1 && raw.get<1>().size() == 1);
    buffer_test::check(raw.index(raw.begin()->id()).value() == 0 && raw.get<1>()[0].value == 7);
}
