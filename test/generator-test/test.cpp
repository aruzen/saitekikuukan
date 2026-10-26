#include "saitekikuukan.hpp"
#include "../buffer-test/checks.hpp"

using buffer_test::check;
using sequence = kk::generator::coroutine_generator<std::size_t>;

sequence numbers() {
    co_yield 0;
    co_yield 1;
}

sequence empty() { co_return; }

sequence failing(bool before_first) {
    if (!before_first)
        co_yield 7;
    throw std::runtime_error("generator");
}

kk::generator::coroutine_generator<std::string> strings() {
    co_yield "first";
    co_yield "second";
}

kk::generator::coroutine_generator<std::unique_ptr<int>> pointers() {
    co_yield std::make_unique<int>(42);
}

struct lifetime {
    int& alive;
    explicit lifetime(int& alive) : alive(alive) { ++alive; }
    ~lifetime() { --alive; }
};

sequence counted(int& alive) {
    lifetime guard(alive);
    co_yield 0;
    co_yield 1;
}

struct throwing_move {
    throwing_move() = default;
    throwing_move(throwing_move&&) { throw std::runtime_error("yield"); }
};

kk::generator::coroutine_generator<throwing_move> failing_yield() { co_yield throwing_move{}; }

template<typename Function>
void check_throws(Function&& function) {
    try {
        function();
        std::abort();
    } catch (const std::runtime_error&) {
    }
}

int main() {
    static_assert(kk::generator::is_generator<sequence>);
    static_assert(!std::is_copy_constructible_v<sequence>);
    static_assert(!std::is_copy_assignable_v<sequence>);
    static_assert(std::is_nothrow_move_constructible_v<sequence>);
    static_assert(std::is_nothrow_move_assignable_v<sequence>);
    static_assert(std::same_as<decltype(strings())::generate_type, std::string>);
    sequence unbound;
    check(!unbound.next());
    auto ids = numbers();
    check(ids.next() == 0 && ids.next() == 1);
    check(!ids.next() && !ids.next());
    check(!empty().next());
    for (bool before_first : {false, true}) {
        auto failed = failing(before_first);
        if (!before_first)
            check(failed.next() == 7);
        check_throws([&] { (void)failed.next(); });
        check(!failed.next());
    }
    auto bad_yield = failing_yield();
    check_throws([&] { (void)bad_yield.next(); });
    check(!bad_yield.next());
    auto names = strings();
    check(names.next() == "first" && names.next() == "second" && !names.next());
    auto owned = pointers();
    auto pointer = owned.next();
    check(pointer && **pointer == 42 && !owned.next());
    int alive = 0;
    {
        auto first = counted(alive);
        check(first.next() == 0 && alive == 1);
        auto second = std::move(first);
        check(!first.next() && second.next() == 1 && alive == 1);
        auto target = counted(alive);
        check(target.next() == 0 && alive == 2);
        target = std::move(second);
        check(alive == 1 && !second.next());
        auto& same = target;
        target = std::move(same);
        check(alive == 1 && !target.next() && alive == 0);
    }
    check(alive == 0);

    using values_type = kk::buffer<kk::type_list<int>, kk::strategy::structure_of_array,
        kk::storage::dynamic_layout<>, kk::identity::generated<sequence>>;
    values_type values{numbers()};
    values.push_back(10).value();
    values.push_back(20).value();
    check(values.find(0)->get<0>() == 10 && values.find(1)->get<0>() == 20);
    check(values.push_back(30).error() == kk::error_code::generate_id_failed);
    check(values.size() == 2);
    values_type growing{numbers()};
    growing.resize(2).value();
    check(growing.resize(3).error() == kk::error_code::generate_id_failed);
    check(growing.size() == 2);
}
