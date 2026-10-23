#include "saitekikuukan.hpp"
#include "../buffer-test/checks.hpp"

#include <deque>
#include <string>

using buffer_test::check;

struct deque_layout {
    static constexpr bool resizable = true;
    static constexpr std::size_t max_size = 64;
    template<typename T> using column = std::deque<T>;
};

template<typename T>
struct throwing_column : std::vector<T> {
    inline static bool fail_move = false;
    throwing_column() = default;
    throwing_column(const throwing_column&) = default;
    throwing_column(throwing_column&& other) : std::vector<T>(std::move(other)) {
        if (fail_move) throw std::runtime_error("column move");
    }
    throwing_column& operator=(const throwing_column&) = default;
    throwing_column& operator=(throwing_column&&) = default;
};
struct throwing_layout : deque_layout {
    template<typename T> using column = throwing_column<T>;
};

struct fragile {
    inline static bool fail = false;
    int value = 7;
    fragile() = default;
    fragile(const fragile& other) : value(other.value) {
        if (fail) throw std::runtime_error("copy");
    }
    fragile(fragile&& other) noexcept(false) : value(other.value) {}
    fragile& operator=(const fragile&) = default;
    fragile& operator=(fragile&&) = default;
};

void check_failures() {
    kk::storage::basic_columns<kk::storage::dynamic_layout<>, int, fragile> values;
    values.reserve(8);
    values.push_back(1, fragile{});
    kk::storage::basic_columns<kk::storage::dynamic_layout<>, int, fragile> target;
    target.push_back(99, fragile{});
    fragile::fail = true;
    try { values.push_back(2, fragile{}); std::abort(); } catch (const std::runtime_error&) {}
    check(values.size() == 1 && values.template get<0>().size() == 1 && values.template get<1>().size() == 1);
    try { target = values; std::abort(); } catch (const std::runtime_error&) {}
    check(target.size() == 1 && target.get<0>()[0] == 99);
    fragile::fail = false;
    values.push_back(2, fragile{});
    check(values.size() == 2);

    using buffer = kk::buffer<kk::generator::iota, kk::eventsystem::dummy,
        kk::storage::dynamic_storage, 8, int, fragile>;
    static_assert(!std::is_nothrow_copy_assignable_v<buffer>);
    buffer source, destination;
    check(source.push_back(1, fragile{}).has_value());
    check(destination.push_back(99, fragile{}).has_value());
    fragile::fail = true;
    try { destination = source; std::abort(); } catch (const std::runtime_error&) {}
    fragile::fail = false;
    check(destination.size() == 1 && destination.get<0>()[0] == 99);

    using pointer = std::unique_ptr<int>;
    kk::storage::basic_columns<throwing_layout, pointer, pointer> moving;
    moving.push_back(std::make_unique<int>(1), std::make_unique<int>(2));
    throwing_column<pointer>::fail_move = true;
    try { auto failed = std::move(moving); (void) failed; std::abort(); }
    catch (const std::runtime_error&) {}
    throwing_column<pointer>::fail_move = false;
    check(moving.empty() && moving.get<0>().empty() && moving.get<1>().empty());
    moving.push_back(std::make_unique<int>(3), std::make_unique<int>(4));
    check(*moving.get<1>()[0] == 4);

    kk::storage::basic_columns<kk::storage::fixed_layout<2>, int> fixed;
    fixed.resize(2);
    try { fixed.push_back(3); std::abort(); } catch (const std::length_error&) {}
    try { fixed.resize(3); std::abort(); } catch (const std::length_error&) {}
    check(fixed.size() == 2);
}

int main() {
    static_assert(kk::buffer<>::max_size == 1024);
    static_assert(kk::buffer<kk::generator::iota, kk::eventsystem::dummy,
        kk::storage::dynamic_storage>::max_size == kk::storage::DynamicSizeUnlimited);
    check_failures();

    const std::string a(256, 'a'), b(256, 'b');
    kk::storage::basic_columns<kk::storage::dynamic_layout<64>, std::string, std::string> values;
    values.push_back(a, b);
    while (values.size() < values.capacity()) values.push_back(a, b);
    values.push_back(values.get<1>()[0], values.get<0>()[0]);
    check(values.get<0>().back() == b && values.get<1>().back() == a);
    kk::buffer<kk::generator::iota, kk::eventsystem::dummy, kk::storage::dynamic_storage,
        64, std::string, std::string> ids;
    check(ids.push_back(a, b).has_value());
    while (ids.size() < ids.get<0>().capacity()) check(ids.push_back(a, b).has_value());
    check(ids.push_back(ids.get<1>()[0], ids.get<0>()[0]).has_value());
    check(ids.get<0>().back() == b && ids.get<1>().back() == a);

    kk::storage::basic_columns<kk::storage::dynamic_layout<>> empty;
    empty.resize(10);
    empty.push_back();
    check(empty.size() == 11);
    kk::storage::basic_columns<kk::storage::fixed_layout<4>> fixed_empty;
    fixed_empty.resize(4);
    check(fixed_empty.size() == 4 && fixed_empty.capacity() == 4);

    kk::storage::basic_columns<deque_layout, int> custom;
    custom.push_back(12);
    custom.resize(4);
    check(custom.size() == 4 && custom.get<0>()[0] == 12);
    kk::storage::basic_columns<deque_layout, std::unique_ptr<int>> custom_movable;
    custom_movable.push_back(std::make_unique<int>(23));
    auto moved = std::move(custom_movable);
    check(custom_movable.empty() && *moved.get<0>()[0] == 23);
}
