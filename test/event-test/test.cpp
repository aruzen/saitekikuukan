#include "saitekikuukan.hpp"

#include <cstdio>
#include <source_location>

void check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "%s:%u: check failed\n", location.file_name(), location.line());
        std::abort();
    }
}

namespace event = kk::eventsystem::event;

struct row_types {
    using inserting = event::will_insert_row_event<int, std::string>;
    using inserted = event::did_insert_row_event<int, std::string>;
    using deleting = event::will_delete_row_event<int, std::string>;
    using deleted = event::did_delete_row_event<int, std::string>;
    using swapping = event::will_swap_row_event<int, std::string>;
    using swapped = event::did_swap_row_event<int, std::string>;
    using updating = event::will_update_row_event<int, std::string>;
    using updated = event::did_update_row_event<int, std::string>;
};

struct id_types {
    using inserting = event::will_insert_event<std::size_t, int, std::string>;
    using inserted = event::did_insert_event<std::size_t, int, std::string>;
    using deleting = event::will_delete_event<std::size_t, int, std::string>;
    using deleted = event::did_delete_event<std::size_t, int, std::string>;
    using swapping = event::will_swap_event<std::size_t, int, std::string>;
    using swapped = event::did_swap_event<std::size_t, int, std::string>;
    using updating = event::will_update_event<std::size_t, int, std::string>;
    using updated = event::did_update_event<std::size_t, int, std::string>;
};

enum class operation { insert, erase, swap, update };

struct notification {
    operation kind;
    bool before;
    std::size_t index = 0, other_index = 0;
    std::size_t id = 0, other_id = 0;
    int value = 0, other_value = 0;
};

template<typename Types>
struct recorder : Types {
    using event_types = kk::type_list<typename Types::inserting, typename Types::inserted,
        typename Types::deleting, typename Types::deleted, typename Types::swapping,
        typename Types::swapped, typename Types::updating, typename Types::updated>;
    std::vector<notification> log;
    std::optional<operation> cancel, throw_before, throw_after;
    int insert_budget = -1, delete_budget = -1;
    bool modify_update = false;

    template<typename Event>
    void on(Event& event) {
        constexpr bool before = requires { event.cancel(); };
        constexpr operation kind = [] {
            if constexpr (std::same_as<Event, typename Types::inserting> ||
                          std::same_as<Event, typename Types::inserted>)
                return operation::insert;
            else if constexpr (std::same_as<Event, typename Types::deleting> ||
                               std::same_as<Event, typename Types::deleted>)
                return operation::erase;
            else if constexpr (std::same_as<Event, typename Types::swapping> ||
                               std::same_as<Event, typename Types::swapped>)
                return operation::swap;
            else
                return operation::update;
        }();
        notification note{kind, before};
        if constexpr (requires { event.index; })
            note.index = event.index;
        if constexpr (requires { event.id; })
            note.id = event.id;
        if constexpr (requires { event.first_index; }) {
            note.index = event.first_index;
            note.other_index = event.second_index;
            note.value = std::get<0>(event.first);
            note.other_value = std::get<0>(event.second);
        }
        if constexpr (requires { event.first_id; }) {
            note.id = event.first_id;
            note.other_id = event.second_id;
        }
        if constexpr (requires { event.data; })
            note.value = std::get<0>(event.data);
        log.push_back(note);
        if constexpr (before) {
            if (cancel == kind)
                event.cancel();
            auto budget = [&](int& remaining) {
                if (remaining == 0)
                    event.cancel();
                else if (remaining > 0)
                    --remaining;
            };
            if constexpr (kind == operation::insert)
                budget(insert_budget);
            if constexpr (kind == operation::erase) {
                static_assert(std::same_as<decltype(std::get<0>(event.data)), const int&>);
                budget(delete_budget);
            }
            if (throw_before == kind)
                throw std::runtime_error("before");
        } else if (throw_after == kind) {
            throw std::runtime_error("after");
        }
        if constexpr (kind == operation::update) {
            if (modify_update)
                std::get<0>(event.data) += before ? 10 : 1;
        }
    }
};

template<typename Function>
void check_throws(Function&& function) {
    try {
        function();
        check(false);
    } catch (const std::runtime_error&) {
    }
}

template<typename Layout, typename Identity, typename Structure = kk::strategy::structure_of_array>
void check_lifecycle() {
    constexpr bool identified = !std::same_as<Identity, kk::identity::none>;
    using events = recorder<std::conditional_t<identified, id_types, row_types>>;
    using buffer = kk::buffer<kk::type_list<int, std::string>, Structure,
        Layout, Identity, events>;
    buffer values;
    auto& observer = values.events();
    values.push_back(10, "first").value();
    values.push_back(20, "second").value();
    values.push_back(30, "third").value();
    observer.log.clear();

    observer.cancel = operation::erase;
    check(values.erase_unordered(1).error() == kk::error_code::operation_canceled);
    check(values.size() == 3 && values.read(1) == std::tuple{20, std::string("second")});
    check(observer.log.size() == 1 && observer.log[0].value == 20);
    observer.cancel.reset();
    observer.log.clear();
    values.erase_unordered(1).value();
    check(observer.log.size() == 2 && observer.log[0].before && !observer.log[1].before);
    check(observer.log[0].kind == operation::erase && observer.log[1].kind == operation::erase);
    check(observer.log[1].index == 1 && values.template column<0>()[1] == 30);
    if constexpr (identified) {
        check(observer.log[1].id == 1 && !values.find(1));
        check(values.index(2).value() == 1);
    }

    observer.log.clear();
    values.swap_rows(0, 1).value();
    check(observer.log.size() == 2 && observer.log[0].value == 10 && observer.log[1].value == 30);
    check(observer.log[0].other_value == 30 && observer.log[1].other_value == 10);
    check(values.read(0) == std::tuple{30, std::string("third")});
    if constexpr (identified) {
        check(observer.log[0].id == 0 && observer.log[0].other_id == 2);
        check(observer.log[1].id == 2 && observer.log[1].other_id == 0);
        check(values.index(2).value() == 0 && values.index(0).value() == 1);
    }
    observer.cancel = operation::swap;
    observer.log.clear();
    check(values.swap_rows(0, 1).error() == kk::error_code::operation_canceled);
    check(values.template column<0>()[0] == 30 && observer.log.size() == 1);
    observer.log.clear();
    values.swap_rows(0, 0).value();
    check(observer.log.empty());

    observer.cancel = operation::update;
    check(values.write(0, {99, "replacement"}).error() == kk::error_code::operation_canceled);
    check(values.template column<0>()[0] == 30 && observer.log.back().value == 99);
    observer.cancel.reset();
    observer.modify_update = true;
    observer.log.clear();
    values.write(0, {99, "replacement"}).value();
    check(values.template column<0>()[0] == 110 && observer.log.size() == 2);
    check(observer.log[0].value == 99 && observer.log[1].value == 109);
    observer.modify_update = false;
    observer.log.clear();
    values.template column<0>()[0] = 11;
    check(observer.log.empty());

    values.resize(1).value();
    check(values.size() == 1 && observer.log.size() == 2 && observer.log[0].index == 1);
    observer.log.clear();
    values.resize(3).value();
    check(observer.log.size() == 4 && values.template column<0>()[2] == 0);
    values.clear().value();
    observer.log.clear();
    values.clear().value();
    values.pop_back().value();
    check(observer.log.empty());

    observer.insert_budget = 2;
    check(values.resize(5).error() == kk::error_code::operation_canceled);
    check(values.size() == 2 && observer.log.size() == 5);
    observer.insert_budget = -1;
    observer.delete_budget = 1;
    observer.log.clear();
    check(values.clear().error() == kk::error_code::operation_canceled);
    check(values.size() == 1 && observer.log.size() == 3);
    observer.delete_budget = -1;
    values.clear().value();

    for (auto kind : {operation::erase, operation::swap, operation::update}) {
        values.clear().value();
        values.push_back(1, "one").value();
        values.push_back(2, "two").value();
        auto change = [&] {
            if (kind == operation::erase)
                values.erase_unordered(0).value();
            else if (kind == operation::swap)
                values.swap_rows(0, 1).value();
            else
                values.write(0, {3, "three"}).value();
        };
        observer.throw_before = kind;
        check_throws(change);
        check(values.size() == 2 && values.template column<0>()[0] == 1);
        observer.throw_before.reset();
        observer.throw_after = kind;
        check_throws(change);
        check(values.size() == (kind == operation::erase ? 1 : 2));
        check(values.template column<0>()[0] == (kind == operation::update ? 3 : 2));
        observer.throw_after.reset();
        if constexpr (identified)
            for (auto row : values)
                check(values.index(row.id()).value() == row.index);
    }
}

inline std::vector<int> listener_order;
using before_int = event::will_insert_row_event<int>;

struct first_listener : kk::eventsystem::listener_tag<before_int> {
    void on(before_int& event) {
        listener_order.push_back(1);
        ++std::get<0>(event.data);
    }
};

struct last_listener : kk::eventsystem::listener_tag<before_int> {
    void on(before_int& event) {
        listener_order.push_back(3);
        std::get<0>(event.data) *= 2;
    }
};

struct counting_listener : kk::eventsystem::listener_tag<before_int> {
    int calls = 0;

    void on(before_int&) {
        listener_order.push_back(2);
        ++calls;
    }
};

void check_static_listeners() {
    using stateless = kk::eventsystem::zero_cost<first_listener, last_listener>;
    using minimum = kk::eventsystem::minimum_cost<first_listener, counting_listener, last_listener,
        counting_listener>;
    using plain = kk::buffer<kk::type_list<int>>;
    using zero_buffer = kk::buffer<kk::type_list<int>, kk::strategy::structure_of_array,
        kk::storage::dynamic_layout<>, kk::identity::none, stateless>;
    using minimum_buffer = kk::buffer<kk::type_list<int>, kk::strategy::structure_of_array,
        kk::storage::dynamic_layout<>, kk::identity::none, minimum>;
    static_assert(kk::eventsystem::is_eventsystem<stateless>);
    static_assert(kk::eventsystem::is_eventsystem<minimum>);
    static_assert(kk::eventsystem::is_eventsystem<kk::eventsystem::zero_cost<>>);
    static_assert(kk::eventsystem::is_eventsystem<kk::eventsystem::minimum_cost<>>);
    static_assert(sizeof(zero_buffer) == sizeof(plain));
    zero_buffer zero;
    listener_order.clear();
    zero.push_back(1).value();
    check(zero.column<0>()[0] == 4 && listener_order == std::vector{1, 3});
    minimum_buffer stored;
    listener_order.clear();
    stored.push_back(1).value();
    check(stored.column<0>()[0] == 4 && listener_order == std::vector{1, 2, 3, 2});
    stored.push_back(1).value();
    check(std::get<0>(stored.events().listeners).calls == 2);
    check(std::get<1>(stored.events().listeners).calls == 2);
    kk::eventsystem::minimum_cost<> empty;
    int value = 1;
    before_int event{0, std::tie(value)};
    empty.on(event);
    kk::eventsystem::zero_cost<>{}.on(event);
    check(value == 1);

    using empty_only = kk::eventsystem::minimum_cost<first_listener, last_listener>;
    using empty_buffer = kk::buffer<kk::type_list<int>, kk::strategy::structure_of_array,
        kk::storage::dynamic_layout<>, kk::identity::none, empty_only>;
    static_assert(sizeof(empty_buffer) == sizeof(plain));
    empty_buffer all_empty;
    all_empty.push_back(1).value();
    check(all_empty.column<0>()[0] == 4);
    using state_only = kk::eventsystem::minimum_cost<counting_listener>;
    state_only all_stateful;
    all_stateful.on(event);
    check(std::get<0>(all_stateful.listeners).calls == 1);
    const auto calls = listener_order.size();
    event::did_delete_row_event<int> unrelated{0};
    all_stateful.on(unrelated);
    check(listener_order.size() == calls);
}

struct checked_id {
    inline static bool fail_copy = false;
    std::size_t value;

    explicit checked_id(std::size_t value) noexcept : value(value) {}

    checked_id(const checked_id& other) : value(other.value) {
        if (fail_copy)
            throw std::runtime_error("ID copy");
    }

    checked_id(checked_id&&) noexcept = default;
    checked_id& operator=(const checked_id&) = default;
    checked_id& operator=(checked_id&&) noexcept = default;
    friend bool operator==(const checked_id&, const checked_id&) = default;
};

struct checked_ids {
    using generate_type = checked_id;
    std::size_t next_id = 0;

    std::optional<checked_id> next() { return checked_id{next_id++}; }
};

void check_delete_metadata() {
    using deleted = event::did_delete_event<checked_id, int>;

    struct events {
        using event_types = kk::type_list<deleted>;
        std::function<void(const checked_id&)> check_removed;

        void on(deleted& event) {
            check(event.index == 0);
            check_removed(event.id);
        }
    };

    using identity = kk::identity::generated<checked_ids, kk::strategy::linear_id_lookup>;
    kk::buffer<kk::type_list<int>, kk::strategy::structure_of_array, kk::storage::dynamic_layout<>,
        identity, events>
        values;
    values.push_back(10).value();
    values.push_back(20).value();
    values.events().check_removed = [&](const checked_id& id) {
        check(id.value == 0 && !values.find(id));
        check(values.find(checked_id{1})->get<0>() == 20);
        check(values.index(checked_id{1}).value() == 0);
    };
    checked_id::fail_copy = true;
    check_throws([&] { values.erase_unordered(0).value(); });
    checked_id::fail_copy = false;
    check(values.size() == 2 && values.find(checked_id{0})->get<0>() == 10);
    values.erase_unordered(0).value();
    check(values.size() == 1);
}

struct bit_events {
    using event_types = kk::type_list<event::will_delete_row_event<bool>,
        event::did_swap_row_event<bool>, event::did_update_row_event<bool>>;

    void on(event::will_delete_row_event<bool>& event) {
        static_assert(std::same_as<decltype(std::get<0>(event.data)), const bool&>);
    }

    void on(event::did_swap_row_event<bool>& event) {
        std::get<0>(event.first) = true;
        std::get<0>(event.second) = false;
    }

    void on(event::did_update_row_event<bool>& event) {
        std::get<0>(event.data) = false;
        throw std::runtime_error("after proxy update");
    }
};

void check_proxy_and_move_only() {
    kk::buffer<kk::type_list<bool>, kk::strategy::structure_of_array, kk::storage::dynamic_layout<>,
        kk::identity::none, bit_events>
        bits;
    bits.push_back(true).value();
    bits.push_back(false).value();
    bits.swap_rows(0, 1).value();
    check(bits.column<0>()[0] && !bits.column<0>()[1]);
    check_throws([&] { bits.write(0, {true}).value(); });
    check(!bits.column<0>()[0]);
    bits.erase_unordered(0).value();

    using pointer = std::unique_ptr<int>;
    using deleting = event::will_delete_event<std::size_t, pointer>;
    using deleted = event::did_delete_event<std::size_t, pointer>;

    struct move_events {
        using event_types = kk::type_list<deleting, deleted>;
        int before = 0, after = 0;

        void on(deleting& event) {
            check(*std::get<0>(event.data) == 7);
            ++before;
        }

        void on(deleted& event) {
            check(event.id == 0 && event.index == 0);
            ++after;
        }
    };

    kk::buffer<kk::type_list<pointer>, kk::strategy::structure_of_array,
        kk::storage::chunked_layout<2>, kk::identity::generated<>, move_events>
        values;
    values.push_back(std::make_unique<int>(7)).value();
    values.erase(0).value();
    check(values.empty() && values.events().before == 1 && values.events().after == 1);
}

#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
struct task {
    int score = 7;
    std::string name = "default";
};

template<typename Layout>
void check_named() {
    kk::buffer<task, kk::strategy::structure_of_array, Layout, kk::identity::generated<>,
        recorder<id_types>>
        values;
    values.resize(3).value();
    check(values.begin()->score == 7 && values.begin()->name == "default");
    values.events().modify_update = true;
    values.write(0, task{10, "changed"}).value();
    check(values.find(0)->score == 21);
    values.swap_rows(0, 2).value();
    check(values.find(0)->score == 21 && values.index(0).value() == 2);
    values.erase(1).value();
    check(!values.find(1) && values.find(0)->name == "changed");
}
#endif

int main() {
    check_static_listeners();
    check_delete_metadata();
    check_proxy_and_move_only();
    check_lifecycle<kk::storage::fixed_layout<8>, kk::identity::none>();
    check_lifecycle<kk::storage::dynamic_layout<8>, kk::identity::none>();
    check_lifecycle<kk::storage::chunked_layout<2>, kk::identity::none>();
    using hash = kk::identity::generated<>;
    using linear = kk::identity::generated<kk::generator::iota, kk::strategy::linear_id_lookup>;
    check_lifecycle<kk::storage::fixed_layout<8>, hash>();
    check_lifecycle<kk::storage::dynamic_layout<8>, hash>();
    check_lifecycle<kk::storage::chunked_layout<2>, hash>();
    check_lifecycle<kk::storage::fixed_layout<8>, linear>();
    check_lifecycle<kk::storage::dynamic_layout<8>, linear>();
    check_lifecycle<kk::storage::chunked_layout<2>, linear>();
    using aos = kk::strategy::array_of_structure;
    check_lifecycle<kk::storage::dynamic_layout<8>, kk::identity::none, aos>();
    check_lifecycle<kk::storage::fixed_layout<8>, kk::identity::none, aos>();
    check_lifecycle<kk::storage::dynamic_layout<8>, hash, aos>();
    check_lifecycle<kk::storage::fixed_layout<8>, hash, aos>();
#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
    check_named<kk::storage::fixed_layout<8>>();
    check_named<kk::storage::dynamic_layout<8>>();
    check_named<kk::storage::chunked_layout<2>>();
#endif
}
