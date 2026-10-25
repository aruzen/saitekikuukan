#include "saitekikuukan.hpp"
#include "../buffer-test/checks.hpp"

using buffer_test::check;
template<typename T> concept can_resize = requires(T value) { value.resize(1); };
template<typename T> concept temporary_columns = requires(T value) { std::move(value).template column<0>(); };

template<std::size_t Size>
void check_chunks() {
    kk::chunked_soa<Size, int, std::string> values;
    static_assert(std::ranges::random_access_range<decltype(values)>);
    static_assert(std::ranges::random_access_range<decltype(values.chunks())>);
    static_assert(std::ranges::random_access_range<decltype(values.template column<0>())>);
    static_assert(!std::ranges::contiguous_range<decltype(values.template column<0>())>);
    static_assert(!can_resize<decltype(values.chunk(0))>);
    static_assert(!temporary_columns<decltype(values)>);
    check(values.empty() && values.chunk_count() == 0 && values.chunks().empty());
    values.pop_back();
    values.push_back(1, std::string("first")).value();
    int* first = &values.template column<0>()[0];
    auto first_chunk = values.chunk(0);
    values.reserve(Size * 4 + 1);
    check(values.size() == 1 && values.chunk_count() == 1 && values.capacity() >= Size * 4 + 1);
    for (std::size_t i = 1; i < Size * 3 + 1; ++i) values.push_back(static_cast<int>(i + 1), std::to_string(i)).value();
    check(first == &values.template column<0>()[0] && std::get<0>(first_chunk[0]) == 1);
    check(values.chunk_count() == 4 && values.chunk(3).size() == 1);
    std::size_t visited = 0;
    for (auto chunk : values.chunks()) {
        auto [x, name] = chunk.columns();
        static_assert(std::same_as<decltype(x), std::span<int>>);
        check(x.size() == chunk.size() && name.size() == chunk.size());
        for (auto row : chunk) { ++std::get<0>(row); ++visited; }
    }
    check(visited == values.size() && values.template column<0>()[Size] == static_cast<int>(Size + 2));
    auto col = values.template column<0>();
    std::ranges::sort(col, std::greater<>{});
    check(col[0] == static_cast<int>(values.size() + 1));
    values.write(0, {99, "written"});
    check(values.read(0) == std::tuple{99, std::string("written")});
    auto tail = values.read(values.size() - 1);
    values.erase_unordered(0);
    check(values.read(0) == tail);
    auto old_size = values.size();
    values.pop_back();
    check(values.size() == old_size - 1 && values.end() - values.begin() == static_cast<std::ptrdiff_t>(values.size()));

    const auto& constant = values;
    static_assert(std::same_as<decltype(constant.template column<0>()[0]), const int&>);
    static_assert(std::same_as<decltype(std::get<0>(constant[0])), const int&>);
    static_assert(std::same_as<decltype(constant.chunk(0).template column<0>()), std::span<const int>>);
    typename decltype(values)::const_iterator converted = values.begin();
    check(converted == constant.begin() && std::get<0>(*converted) == std::get<0>(values[0]));
    check(values.template column<0>().begin() == values.template get<0>().begin());
    check(constant.template column<0>().begin() == values.template column<0>().begin());
    check(values.chunks().begin()->size() == values.chunk(0).size());

    auto copy = values;
    copy.template column<0>()[0] = -1;
    check(values.template column<0>()[0] != -1);
    auto moved = std::move(copy);
    check(copy.empty() && copy.capacity() == 0 && moved.template column<0>()[0] == -1);
    copy.push_back(7, "reused").value();
    check(copy.size() == 1);
    moved = values;
    moved = std::move(copy);
    check(moved.read(0) == std::tuple{7, std::string("reused")} && copy.empty());
    values.resize(Size + 1).value();
    values.resize(Size * 3 + 2).value();
    check(values.read(values.size() - 1) == std::tuple{0, std::string{}});
    values.shrink_to_fit();
    check(values.capacity() == values.chunk_count() * Size && &values.template column<0>()[0] == first);
    try { (void)values.at(values.size()); std::abort(); } catch (const std::out_of_range&) {}
    try { (void)values.chunk(values.chunk_count()); std::abort(); } catch (const std::out_of_range&) {}
    try { values.erase_unordered(values.size()); std::abort(); } catch (const std::out_of_range&) {}
    try { values.reserve(values.max_size() + 1); std::abort(); } catch (const std::length_error&) {}
    values.clear();
    check(values.empty() && values.chunks().empty() && values.template column<0>().empty());
    values.shrink_to_fit();
    check(values.capacity() == 0);
}

struct fragile {
    inline static bool fail_default = false, fail_copy = false;
    int value = 7;
    fragile() { if (fail_default) throw std::runtime_error("default"); }
    fragile(const fragile& other) : value(other.value) { if (fail_copy) throw std::runtime_error("copy"); }
    fragile(fragile&&) noexcept = default;
    fragile& operator=(const fragile&) = default;
    fragile& operator=(fragile&&) noexcept = default;
};

void check_failures() {
    kk::chunked_soa<2, int, fragile> values;
    fragile item;
    values.push_back(1, item).value();
    values.push_back(2, item).value();
    fragile::fail_default = true;
    try { values.push_back(3, item).value(); std::abort(); } catch (const std::runtime_error&) {}
    fragile::fail_default = false;
    check(values.size() == 2 && values.chunk_count() == 1 && values.column<0>()[1] == 2);
    values.push_back(3, item).value();
    kk::chunked_soa<2, int, fragile> target;
    target.push_back(99, item).value();
    fragile::fail_copy = true;
    try { target = values; std::abort(); } catch (const std::runtime_error&) {}
    fragile::fail_copy = false;
    check(target.size() == 1 && target.column<0>()[0] == 99);

    kk::chunked_soa<2, std::string, std::string> aliased;
    std::string a(256, 'a'), b(256, 'b');
    aliased.push_back(a, b).value();
    aliased.push_back(a, b).value();
    aliased.push_back(aliased.column<1>()[0], aliased.column<0>()[0]).value();
    check(aliased.read(2) == std::tuple{b, a});

    using pointers = kk::chunked_soa<2, std::unique_ptr<int>>;
    static_assert(!std::is_copy_constructible_v<pointers> && std::is_nothrow_move_constructible_v<pointers>);
    pointers movable;
    for (int i = 0; i < 5; ++i) movable.push_back(std::make_unique<int>(i)).value();
    movable.erase_unordered(1);
    check(movable.size() == 4 && *movable.column<0>()[1] == 4);
}

using positions = kk::chunked_soa<128, float, float>;
extern "C" void update_chunk_columns(positions& values, float dx, float dy) {
    for (auto chunk : values.chunks()) {
        auto [x, y] = chunk.columns();
        for (std::size_t i = 0; i < chunk.size(); ++i) { x[i] += dx; y[i] += dy; }
    }
}

#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
struct particle { float x = 1, y = 2; std::string name = "default"; };
using named_positions = kk::record_chunked_soa<particle, 128>;
extern "C" void update_named_chunk_columns(named_positions& values, float dx, float dy) {
    for (auto chunk : values.chunks()) {
        auto storage = chunk.columns();
        for (std::size_t i = 0; i < chunk.size(); ++i) { storage.x[i] += dx; storage.y[i] += dy; }
    }
}
struct sequenced {
    inline static int sequence = 0, throw_at = 0;
    static int next() { if (++sequence == throw_at) throw std::runtime_error("row"); return sequence; }
    int number = next();
};
void check_records() {
    kk::record_chunked_soa<particle, 3> values;
    values.resize(8).value();
    for (auto row : values) check(row.x == 1 && row.y == 2 && row.name == "default");
    auto storage = values.chunk(0).columns();
    storage.x[0] = 10;
    check(values.begin()->x == 10 && (values.begin() + 3)->name == "default");
    values.column<^^particle::y>()[4] = 20;
    check(values[4].y == 20);
    const auto& constant = values;
    static_assert(std::same_as<decltype(constant.begin()->x), const float&>);
    static_assert(std::same_as<decltype(constant.chunk(0).columns().name), std::span<const std::string>>);
    static_assert(std::same_as<decltype(constant.column<^^particle::x>()[0]), const float&>);
    auto saved = values.read(4);
    values.write(0, saved);
    check(values[0].y == 20);
    values.push_back(particle{3, 4, "last"}).value();
    values.erase_unordered(0);
    check(values[0].name == "last" && values.size() == 8);

    kk::record_chunked_soa<sequenced, 2> ordered;
    ordered.resize(1).value();
    sequenced::throw_at = 4;
    try { ordered.resize(7).value(); std::abort(); } catch (const std::runtime_error&) {}
    check(ordered.size() == 1 && ordered.chunk_count() == 1 && ordered[0].number == 1);
    sequenced::throw_at = 0;
    ordered.resize(3).value();
    check(ordered[1].number == 5 && ordered[2].number == 6);

    named_positions points;
    points.resize(259).value();
    update_named_chunk_columns(points, 2, 3);
    for (auto row : points) check(row.x == 3 && row.y == 5 && row.name == "default");
}
#endif

int main() {
    check_chunks<1>();
    check_chunks<3>();
    check_chunks<128>();
    check_failures();
    kk::chunked_soa<3> empty;
    empty.resize(7).value();
    empty.push_back().value();
    empty.erase_unordered(0);
    check(empty.size() == 7 && empty.chunk_count() == 3 && empty.chunk(2).size() == 1);
    positions points;
    points.resize(259).value();
    update_chunk_columns(points, 2, 3);
    for (auto [x, y] : points) check(x == 2 && y == 3);
#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
    check_records();
#endif
}
