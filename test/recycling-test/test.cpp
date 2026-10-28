#include "saitekikuukan.hpp"
#include "../buffer-test/checks.hpp"

using buffer_test::check;

template<typename Layout, typename Lookup, typename Structure = kk::strategy::structure_of_array>
void check_reuse() {
    using identity = kk::identity::recycled<Lookup>;
    using values_type = kk::buffer<kk::type_list<int, std::string>,
        Structure, Layout, identity>;
    values_type values;
    values.push_back(10, "first").value();
    const auto first = values.begin()->id();
    values.push_back(20, "second").value();
    const auto second = (values.end() - 1)->id();
    values.erase(first).value();
    values.push_back(30, "replacement").value();
    const auto replaced = (values.end() - 1)->id();
    check(first.slot() == replaced.slot() && first.generation() + 1 == replaced.generation());
    check(!values.find(first) && values.find(replaced)->template get<0>() == 30);
    check(values.find(second)->template get<0>() == 20);
    auto copy = values;
    copy.erase(replaced).value();
    check(values.find(replaced)->template get<0>() == 30 && !copy.find(replaced));
    values.swap_rows(0, 1).value();
    check(values.index(replaced).value() == 0 && values.index(second).value() == 1);
    values.resize(1).value();
    check(!values.find(second));
    values.resize(4).value();
    for (auto row : values)
        check(values.index(row.id()).value() == row.index);
    auto moved = std::move(values);
    check(values.empty() && moved.find(replaced)->template get<0>() == 30);
    check(values.push_back(1, "unbound").error() == kk::error_code::generate_id_failed);
    moved.clear().value();
    moved.push_back(7, "again").value();
    check(!moved.find(replaced));
}

void check_generation_limits() {
    kk::generator::recycling<unsigned char, unsigned char> ids;
    auto first = ids.next().value();
    auto current = first;
    for (unsigned i = 0; i < 255; ++i) {
        check(ids.release(current));
        current = ids.next().value();
        check(current.slot() == first.slot() && current.generation() == i + 1);
        check(!ids.valid(first));
    }
    check(ids.release(current) && !ids.release(current));
    const auto fresh = ids.next().value();
    check(fresh.slot() != first.slot() && fresh.generation() == 0);
    using identity = kk::identity::recycled<kk::strategy::hash_id_lookup, unsigned char>;
    kk::buffer<kk::type_list<int>, kk::strategy::structure_of_array,
        kk::storage::dynamic_layout<>, identity> values;
    check(values.resize(256).error() == kk::error_code::generate_id_failed);
    check(values.empty());
    values.push_back(1).value();
    check(values.size() == 1);
}

using handle = kk::identity::generational_id<>;
using before = kk::eventsystem::event::will_insert_event<handle, int>;
using after = kk::eventsystem::event::did_insert_event<handle, int>;
using deleting = kk::eventsystem::event::will_delete_event<handle, int>;

struct events {
    using event_types = kk::type_list<before, after, deleting>;
    bool cancel_insert = false, cancel_delete = false, throw_after = false, replace_id = false;
    bool throw_before = false;
    void on(before& event) {
        if (throw_before)
            throw std::runtime_error("before");
        if (cancel_insert)
            event.cancel();
        if (replace_id)
            event.id = handle{99, 99};
    }
    void on(after&) {
        if (throw_after)
            throw std::runtime_error("after");
    }
    void on(deleting& event) {
        if (cancel_delete)
            event.cancel();
    }
};

void check_events() {
    kk::buffer<kk::type_list<int>, kk::strategy::structure_of_array,
        kk::storage::dynamic_layout<>, kk::identity::recycled<>, events> values;
    values.events().cancel_insert = true;
    check(values.push_back(1).error() == kk::error_code::operation_canceled);
    values.events().cancel_insert = false;
    values.push_back(2).value();
    const auto id = values.begin()->id();
    check(id.slot() == 0 && id.generation() == 1);
    values.events().cancel_delete = true;
    check(values.erase(id).error() == kk::error_code::operation_canceled);
    check(values.find(id)->get<0>() == 2);
    values.events().cancel_delete = false;
    values.erase(id).value();
    values.events().throw_before = true;
    try {
        values.push_back(3).value();
        std::abort();
    } catch (const std::runtime_error&) {
    }
    values.events().throw_before = false;
    values.events().replace_id = true;
    check(values.push_back(3).error() == kk::error_code::invalid_id);
    check(values.empty());
    values.events().replace_id = false;
    values.events().throw_after = true;
    try {
        values.push_back(4).value();
        std::abort();
    } catch (const std::runtime_error&) {
    }
    check(values.size() == 1 && values.begin()->id().slot() == 0);
    values.events().throw_after = false;
    values.push_back(5).value();
    check((values.end() - 1)->id().slot() == 1);
    values.clear().value();
}

struct fragile {
    inline static bool fail = false;
    fragile() { if (fail) throw std::runtime_error("default"); }
    fragile(const fragile&) { if (fail) throw std::runtime_error("copy"); }
    fragile(fragile&&) noexcept = default;
    fragile& operator=(const fragile&) = default;
    fragile& operator=(fragile&&) noexcept = default;
};

void check_failed_storage() {
    using buffer = kk::buffer<kk::type_list<fragile>, kk::strategy::structure_of_array,
        kk::storage::dynamic_layout<>, kk::identity::recycled<>>;
    buffer values;
    fragile item;
    fragile::fail = true;
    try {
        values.push_back(item).value();
        std::abort();
    } catch (const std::runtime_error&) {
    }
    fragile::fail = false;
    values.push_back(item).value();
    check(values.begin()->id().slot() == 0 && values.begin()->id().generation() == 1);
    buffer growing;
    fragile::fail = true;
    try {
        growing.resize(3).value();
        std::abort();
    } catch (const std::runtime_error&) {
    }
    fragile::fail = false;
    check(growing.empty());
    growing.push_back(item).value();
    check(growing.begin()->id().slot() < 3 && growing.begin()->id().generation() == 1);
}

struct throwing_copy_events {
    using event_types = kk::type_list<>;
    inline static bool fail = false;
    throwing_copy_events() = default;
    throwing_copy_events(const throwing_copy_events&) {
        if (fail)
            throw std::runtime_error("copy");
    }
    throwing_copy_events(throwing_copy_events&&) noexcept = default;
    throwing_copy_events& operator=(throwing_copy_events&&) noexcept = default;
};

void check_copy_transaction() {
    using buffer = kk::buffer<kk::type_list<int>, kk::strategy::structure_of_array,
        kk::storage::dynamic_layout<>, kk::identity::recycled<>, throwing_copy_events>;
    buffer source, target;
    source.push_back(1).value();
    source.clear().value();
    source.push_back(2).value();
    target.push_back(99).value();
    const auto target_id = target.begin()->id();
    throwing_copy_events::fail = true;
    try {
        target = source;
        std::abort();
    } catch (const std::runtime_error&) {
    }
    throwing_copy_events::fail = false;
    check(target.find(target_id)->get<0>() == 99);
    target.erase(target_id).value();
    target.push_back(100).value();
    check(target.begin()->id().generation() == 1);
}

#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
struct particle { int x = 7; };
void check_named() {
    kk::buffer<particle, kk::strategy::structure_of_array, kk::storage::chunked_layout<2>,
        kk::identity::recycled<>> values;
    values.resize(3).value();
    auto id = values.begin()->id();
    values.erase(id).value();
    values.push_back(particle{9}).value();
    check(!values.find(id) && (values.end() - 1)->x == 9);
}
#endif

int main() {
    check_reuse<kk::storage::dynamic_layout<8>, kk::strategy::hash_id_lookup>();
    check_reuse<kk::storage::fixed_layout<8>, kk::strategy::hash_id_lookup>();
    check_reuse<kk::storage::chunked_layout<2>, kk::strategy::hash_id_lookup>();
    check_reuse<kk::storage::dynamic_layout<8>, kk::strategy::linear_id_lookup>();
    check_reuse<kk::storage::dynamic_layout<8>, kk::strategy::sparse_id_lookup<2>>();
    check_reuse<kk::storage::fixed_layout<8>, kk::strategy::sparse_id_lookup<2>>();
    check_reuse<kk::storage::chunked_layout<2>, kk::strategy::sparse_id_lookup<2>>();
    using aos = kk::strategy::array_of_structure;
    check_reuse<kk::storage::dynamic_layout<8>, kk::strategy::sparse_id_lookup<2>, aos>();
    check_reuse<kk::storage::fixed_layout<8>, kk::strategy::sparse_id_lookup<2>, aos>();
    check_generation_limits();
    check_events();
    check_failed_storage();
    check_copy_transaction();
#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
    check_named();
#endif
}
