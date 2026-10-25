#include "saitekikuukan.hpp"
#include "../buffer-test/checks.hpp"

#include <optional>
#include <string>

using buffer_test::check;

template<typename T>
concept has_find = requires(T& value) { value.find(0); };
template<typename T>
concept exposes_storage = requires(T& value) { value.storage; };

template<typename Layout, typename Lookup>
void check_ids() {
    using values_type =
        kk::buffer<kk::type_list<int, std::string>, kk::strategy::structure_of_array, Layout,
            kk::identity::generated<kk::generator::iota, Lookup>>;
    values_type values;
    check(values.push_back(10, "first").has_value());
    const auto first = values.begin()->id();
    check(values.push_back(20, "second").has_value());
    const auto second = (values.end() - 1)->id();
    check(values.push_back(30, "third").has_value());
    const auto third = (values.end() - 1)->id();
    check(values.push_back(40, "fourth").has_value());
    const auto fourth = (values.end() - 1)->id();
    static_assert(!exposes_storage<values_type>);

    check(values[0].template get<0>() == 10 && values.find(first)->template get<1>() == "first");
    check(values.erase(second).has_value());
    check(!values.find(second));
    check(values.index(fourth).value() == 1 && values[1].id() == fourth);
    values.swap_rows(0, 2);
    check(values.find(first)->template get<0>() == 10 && values.index(third).value() == 0);
    values.find(first)->template get<0>() = 11;
    check(values.template get<0>(first).value().get() == 11);
    values.write(1, {44, "changed"});
    check(values.find(fourth)->template get<1>() == "changed");

    const auto& constant = values;
    static_assert(std::same_as<decltype(constant.begin()->template get<0>()), const int&>);
    check(constant.find(first)->template get<0>() == 11);
    auto copy = values;
    copy.find(first)->template get<0>() = 99;
    check(values.find(first)->template get<0>() == 11);
    values_type assigned;
    assigned = copy;
    auto moved = std::move(assigned);
    check(assigned.empty() && moved.find(first)->template get<0>() == 99);
    check(assigned.push_back(7, "reused").has_value());
    moved = std::move(copy);
    check(copy.empty() && moved.find(first)->template get<0>() == 99);

    check(values.resize(7).has_value());
    for (std::size_t i = 0; i < values.size(); ++i)
        check(values.index((values.begin() + i)->id()).value() == i);
    const auto tail = (values.end() - 1)->id();
    values.pop_back();
    check(!values.find(tail));
    check(values.resize(1).has_value());
    check(values.find(third)->template get<0>() == 30 && !values.find(first));
    values.clear();
    check(values.empty() && !values.find(third));
    check(values.push_back(8, "new").has_value());
    check(values.index(values.begin()->id()).value() == 0);
}

struct repeated_ids {
    using generate_type = std::size_t;

    std::optional<std::size_t> next() { return 0; }
};

template<typename Lookup>
void check_duplicates() {
    kk::buffer<kk::type_list<int>, kk::strategy::structure_of_array, kk::storage::dynamic_layout<>,
        kk::identity::generated<repeated_ids, Lookup>>
        values;
    check(values.push_back(10).has_value());
    check(values.push_back(20).error() == kk::error_code::duplicate_id);
    check(values.size() == 1 && values.find(0)->template get<0>() == 10);
    check(values.resize(3).error() == kk::error_code::duplicate_id);
    check(values.size() == 1 && values.index(0).value() == 0);
}

// ハッシュ索引のキー構築だけを失敗させ、行数と索引のロールバックを確認する。
struct checked_id {
    std::size_t value;
    inline static int copies_left = -1;
    inline static std::size_t comparisons = 0;

    checked_id(std::size_t value) noexcept : value(value) {}

    checked_id(const checked_id& other) : value(other.value) {
        if (copies_left == 0)
            throw std::runtime_error("ID copy");
        if (copies_left > 0)
            --copies_left;
    }

    checked_id(checked_id&&) noexcept = default;
    checked_id& operator=(const checked_id&) = default;
    checked_id& operator=(checked_id&&) noexcept = default;

    friend bool operator==(const checked_id& a, const checked_id& b) noexcept {
        ++comparisons;
        return a.value == b.value;
    }
};

template<>
struct std::hash<checked_id> {
    std::size_t operator()(const checked_id& id) const noexcept { return id.value; }
};

struct checked_ids {
    using generate_type = checked_id;
    std::size_t next_id = 0;

    std::optional<checked_id> next() { return checked_id{next_id++}; }
};

void check_index_rollback() {
    kk::buffer<kk::type_list<int>, kk::strategy::structure_of_array, kk::storage::chunked_layout<2>,
        kk::identity::generated<checked_ids>>
        values;
    check(values.push_back(10).has_value());
    checked_id::copies_left = 0;
    try {
        values.push_back(20).value();
        std::abort();
    } catch (const std::runtime_error&) {
    }
    checked_id::copies_left = -1;
    check(values.size() == 1 && values.find(checked_id{0})->template get<0>() == 10);
    check(!values.find(checked_id{1}));

    checked_id::copies_left = 1;
    try {
        values.resize(4).value();
        std::abort();
    } catch (const std::runtime_error&) {
    }
    checked_id::copies_left = -1;
    check(values.size() == 1 && values.index(checked_id{0}).value() == 0);
    for (std::size_t i = 1; i < 5; ++i)
        check(!values.find(checked_id{i}));
    check(values.push_back(50).has_value());
    check(values.find(checked_id{5})->template get<0>() == 50);
}

template<typename Lookup>
std::size_t lookup_comparisons() {
    kk::buffer<kk::type_list<int>, kk::strategy::structure_of_array, kk::storage::dynamic_layout<>,
        kk::identity::generated<checked_ids, Lookup>>
        values;
    for (int i = 0; i < 128; ++i)
        check(values.push_back(i).has_value());
    checked_id::comparisons = 0;
    for (int i = 0; i < 128; ++i)
        check(values.find(checked_id{127})->template get<0>() == 127);
    return checked_id::comparisons;
}

struct string_ids {
    using generate_type = std::string;
    std::size_t next_id = 0;

    std::optional<std::string> next() { return "entity-" + std::to_string(next_id++); }
};

void check_string_ids() {
    kk::buffer<kk::type_list<int>, kk::strategy::structure_of_array, kk::storage::dynamic_layout<>,
        kk::identity::generated<string_ids>>
        values;
    check(values.push_back(10).has_value());
    check(values.push_back(20).has_value());
    check(values.erase("entity-0").has_value());
    check(values.index("entity-1").value() == 0);
    check(values.find("entity-1")->get<0>() == 20);
}

struct plain_id {
    std::size_t value;
    friend bool operator==(const plain_id&, const plain_id&) = default;
};

struct plain_ids {
    using generate_type = plain_id;
    std::size_t next_id = 0;

    std::optional<plain_id> next() { return plain_id{next_id++}; }
};

void check_unhashed_ids() {
    kk::buffer<kk::type_list<int>, kk::strategy::structure_of_array, kk::storage::dynamic_layout<>,
        kk::identity::generated<plain_ids, kk::strategy::linear_id_lookup>>
        values;
    check(values.push_back(12).has_value());
    check(values.find(plain_id{0})->get<0>() == 12);
}

template<typename Before, typename After>
struct bit_events {
    using event_types = kk::type_list<Before, After>;
    int notifications = 0;

    void on(Before& event) { std::get<0>(event.data) = true; }

    void on(After& event) {
        check(std::get<0>(event.data));
        std::get<0>(event.data) = false;
        ++notifications;
    }
};

void check_proxy_events() {
    using before = kk::eventsystem::event::will_insert_row_event<bool>;
    using after = kk::eventsystem::event::did_insert_row_event<bool>;
    kk::buffer<kk::type_list<bool>, kk::strategy::structure_of_array, kk::storage::dynamic_layout<>,
        kk::identity::none, bit_events<before, after>>
        plain;
    check(plain.push_back(false).has_value());
    check(!plain.column<0>()[0] && plain.events().notifications == 1);

    using id_before = kk::eventsystem::event::will_insert_event<std::size_t, bool>;
    using id_after = kk::eventsystem::event::did_insert_event<std::size_t, bool>;
    kk::buffer<kk::type_list<bool>, kk::strategy::structure_of_array, kk::storage::dynamic_layout<>,
        kk::identity::generated<>, bit_events<id_before, id_after>>
        identified;
    check(identified.push_back(false).has_value());
    check(!identified.column<0>()[0] && identified.events().notifications == 1);
}

template<typename T>
struct unstable_column : std::vector<T> {
    inline static bool fail_move = false;
    unstable_column() = default;
    unstable_column(const unstable_column&) = default;

    unstable_column(unstable_column&& other) : std::vector<T>(std::move(other)) {
        if (fail_move)
            throw std::runtime_error("column move");
    }

    unstable_column& operator=(const unstable_column&) = default;
    unstable_column& operator=(unstable_column&&) = default;
};

struct unstable_layout {
    static constexpr bool resizable = true;
    static constexpr std::size_t max_size = 8;
    template<typename T>
    using column = unstable_column<T>;
};

void check_failed_move() {
    using pointer = std::unique_ptr<int>;
    kk::buffer<kk::type_list<pointer>, kk::strategy::structure_of_array, unstable_layout,
        kk::identity::generated<>>
        values;
    check(values.push_back(std::make_unique<int>(7)).has_value());
    unstable_column<pointer>::fail_move = true;
    try {
        auto failed = std::move(values);
        (void)failed;
        std::abort();
    } catch (const std::runtime_error&) {
    }
    unstable_column<pointer>::fail_move = false;
    check(values.empty() && !values.find(0));
    check(values.push_back(std::make_unique<int>(9)).has_value());
    check(*values.find(1)->get<0>() == 9);
}

using before_row = kk::eventsystem::event::will_insert_row_event<int>;
using after_row = kk::eventsystem::event::did_insert_row_event<int>;

struct row_events {
    using event_types = kk::type_list<before_row, after_row>;
    std::size_t accepted = 0;

    void on(before_row& event) {
        if (std::get<0>(event.data) < 0)
            event.cancel();
        else
            std::get<0>(event.data) += 10;
    }

    void on(after_row& event) {
        check(event.index == accepted);
        ++accepted;
    }
};

void check_optional_identity() {
    using no_id = kk::buffer<kk::type_list<int>>;
    static_assert(std::same_as<no_id, kk::soa<int>>);
    static_assert(!has_find<no_id> && !no_id::has_identity);
    static_assert(
        sizeof(no_id) == sizeof(kk::storage::basic_columns<kk::storage::dynamic_layout<>, int>));

    kk::buffer<kk::type_list<int>, kk::strategy::structure_of_array, kk::storage::chunked_layout<2>,
        kk::identity::none, row_events>
        values;
    check(values.push_back(1).has_value());
    check(values.push_back(-1).error() == kk::error_code::operation_canceled);
    check(values.push_back(2).has_value());
    check(values.push_back(3).has_value());
    check(values.events().accepted == 3 && std::get<0>(values[2]) == 13);
    check(values.chunk_count() == 2);

    kk::buffer<kk::type_list<int, int, std::string>, kk::strategy::structure_of_array,
        kk::storage::chunked_layout<2>, kk::identity::generated<>, buffer_test::insert_events>
        ids;
    check(ids.push_back(1, 2, "first").has_value());
    ids.events().cancel_next = true;
    check(ids.push_back(3, 4, "cancel").error() == kk::error_code::operation_canceled);
    check(ids.push_back(5, 6, "second").has_value());
    check(ids.push_back(7, 8, "third").has_value());
    check(ids.find(3)->get<0>() == 107 && ids.find(3)->get<1>() == 9);
    check(ids.chunk_count() == 2 && ids.events().after_count == 3);
}

#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
struct particle {
    float x = 1, y = 2;
    std::string name = "default";
};

// 配置方針が実体への参照を返しても、ID 層は列から行参照を組み立てられる。
class aos_test_rows {
    std::vector<particle> m_values;

    template<std::size_t N>
    static auto projection() {
        return [](auto& row) -> decltype(auto) {
            if constexpr (N == 0)
                return (row.x);
            else if constexpr (N == 1)
                return (row.y);
            else
                return (row.name);
        };
    }

public:
    using value_type = particle;
    using element_types = kk::type_list<float, float, std::string>;

    std::size_t size() const { return m_values.size(); }

    bool empty() const { return m_values.empty(); }

    std::size_t max_size() const { return m_values.max_size(); }

    std::size_t capacity() const { return m_values.capacity(); }

    void reserve(std::size_t n) { m_values.reserve(n); }

    void resize(std::size_t n) { m_values.resize(n); }

    void pop_back() { m_values.pop_back(); }

    void clear() { m_values.clear(); }

    void swap(aos_test_rows& other) noexcept { m_values.swap(other.m_values); }

    template<typename... Args>
    void push_back(Args&&... args) {
        m_values.push_back(particle{std::forward<Args>(args)...});
    }

    template<std::size_t N>
    auto column() {
        return m_values | std::views::transform(projection<N>());
    }

    template<std::size_t N>
    auto column() const {
        return m_values | std::views::transform(projection<N>());
    }

    particle& operator[](std::size_t n) { return m_values[n]; }

    const particle& operator[](std::size_t n) const { return m_values[n]; }

    particle read(std::size_t n) const { return m_values.at(n); }

    void write(std::size_t n, particle value) { m_values.at(n) = std::move(value); }
};

struct aos_test_strategy {
    template<typename, typename...>
    using container = aos_test_rows;
};

void check_strategy_independence() {
    kk::buffer<particle, aos_test_strategy, kk::storage::dynamic_layout<>,
        kk::identity::generated<>>
        values;
    check(values.push_back(particle{3, 4, "aos"}).has_value());
    const auto id = values.begin()->id();
    values.find(id)->x = 20;
    check(values.read(0).x == 20);
    const auto& constant = values;
    static_assert(std::same_as<decltype(constant.find(id)->x), const float&>);
    check(values.push_back(particle{5, 6, "last"}).has_value());
    const auto last = (values.end() - 1)->id();
    check(values.erase(id).has_value());
    check(values.find(last)->x == 5 && values.find(last)->name == "last");
    check(values.index(last).value() == 0);
}

template<typename Layout>
void check_named() {
    kk::buffer<particle, kk::strategy::structure_of_array, Layout, kk::identity::generated<>>
        values;
    check(values.resize(5).has_value());
    auto id = (values.begin() + 3)->id();
    values.find(id)->x = 30;
    values.template column<^^particle::y>()[3] = 40;
    check(values.read(3).name == "default" && values.find(id)->y == 40);
    const auto& constant = values;
    static_assert(std::same_as<decltype(constant.find(id)->x), const float&>);
    check(values.erase(values.begin()->id()).has_value());
    check(values.find(id)->x == 30);
}
#endif

int main() {
    check_optional_identity();
    check_ids<kk::storage::fixed_layout<8>, kk::strategy::linear_id_lookup>();
    check_ids<kk::storage::fixed_layout<8>, kk::strategy::hash_id_lookup>();
    check_ids<kk::storage::dynamic_layout<8>, kk::strategy::linear_id_lookup>();
    check_ids<kk::storage::dynamic_layout<8>, kk::strategy::hash_id_lookup>();
    check_ids<kk::storage::chunked_layout<2>, kk::strategy::linear_id_lookup>();
    check_ids<kk::storage::chunked_layout<2>, kk::strategy::hash_id_lookup>();
    check_duplicates<kk::strategy::linear_id_lookup>();
    check_duplicates<kk::strategy::hash_id_lookup>();
    check_index_rollback();
    check_string_ids();
    check_unhashed_ids();
    check_proxy_events();
    check_failed_move();
    check(lookup_comparisons<kk::strategy::linear_id_lookup>() == 128 * 128);
    check(lookup_comparisons<kk::strategy::hash_id_lookup>() <= 256);
#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
    static_assert(std::same_as<kk::buffer<particle>, kk::record_soa<particle>>);
    check_named<kk::storage::fixed_layout<8>>();
    check_named<kk::storage::dynamic_layout<8>>();
    check_named<kk::storage::chunked_layout<2>>();
    check_strategy_independence();
#endif
}
