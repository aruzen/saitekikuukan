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

// Existing custom storage only needs entries and the original storage protocol.
template<typename ID, std::size_t Size, typename... Types>
struct legacy_storage : kk::storage::static_storage<ID, Size, Types...> {
private:
    using kk::storage::static_storage<ID, Size, Types...>::get;
};

template<template<typename, std::size_t, typename...> typename Storage>
void check_strategy() {
    using namespace kk;
    using values = buffer<generator::iota, eventsystem::dummy, Storage, 4, int, int, std::string>;
    buffer_test::check_buffer<values>();
    using events_buffer = buffer<generator::iota, buffer_test::insert_events, Storage, 2, int, int, std::string>;
    buffer_test::check_events<events_buffer>();
    using registry = eventsystem::registration<buffer_test::before_insert>;
    using single = eventsystem::one_event_each_only<buffer_test::before_insert>;
    buffer_test::check_registered_events<buffer<generator::iota, registry, Storage, 4, int, int, std::string>>();
    buffer_test::check_registered_events<buffer<generator::iota, single, Storage, 4, int, int, std::string>>();

    using limited = buffer<two_ids, eventsystem::dummy, Storage, 4, int>;
    limited ids{};
    buffer_test::check(ids.push_back(1).has_value());
    buffer_test::check(ids.push_back(2).has_value());
    buffer_test::check(ids.push_back(3).error() == error_code::generate_id_failed);
    buffer_test::check(ids.size() == 2);
    limited resized{};
    buffer_test::check(resized.resize(3).error() == error_code::generate_id_failed);
    buffer_test::check(resized.empty());

    buffer<generator::iota, eventsystem::dummy, Storage, 4, std::unique_ptr<int>> movable{};
    buffer_test::check(movable.push_back(std::make_unique<int>(42)).has_value());
    buffer_test::check(*movable.begin()->template get<0>() == 42);
    buffer_test::check(movable.erase(0).has_value());
}

int main() {
    using namespace kk;
    check_strategy<storage::static_storage>();
    check_strategy<storage::dynamic_storage>();
    check_strategy<legacy_storage>();

    buffer<generator::iota, eventsystem::dummy, storage::static_storage, 4, component> single{};
    static_assert(std::is_same_v<decltype(single.get<0>()), std::array<component, 4>&>);
    buffer_test::check(single.push_back(component{7}).has_value());
    buffer_test::check(single.begin()->get<0>().x == 7);

    buffer<generator::iota, eventsystem::dummy, storage::dynamic_storage, 4, bool> bits{};
    buffer_test::check(bits.push_back(true).has_value());
    bits.get<0>(0).value() = false;
    buffer_test::check(!bits.begin()->get<0>());
    buffer_test::check(!std::as_const(bits).get<0>(0).value());

    storage::dynamic_storage<std::size_t, 4, int, throwing_value> raw;
    throwing_value value;
    throwing_value::fail_copy = true;
    try {
        (void) raw.push_back(0, 1, value);
        std::abort();
    } catch (const std::runtime_error&) {}
    throwing_value::fail_copy = false;
    buffer_test::check(raw.empty() && raw.get<0>().empty() && raw.get<1>().empty());
    buffer_test::check(raw.push_back(0, 1, value).has_value());
    throwing_value::fail_default = true;
    try {
        (void) raw.resize(3);
        std::abort();
    } catch (const std::runtime_error&) {}
    throwing_value::fail_default = false;
    buffer_test::check(raw.size() == 1 && raw.get<0>().size() == 1 && raw.get<1>().size() == 1);
    buffer_test::check(raw.ids[0] == 0 && raw.get<1>()[0].value == 7);
}
