#include "saitekikuukan.hpp"
#include "check.hpp"

#include <deque>
#include <ranges>
#include <string>
#include <tuple>

struct deque_layout {
    static constexpr bool resizable = true;
    static constexpr std::size_t max_size = 8;

    template<typename T>
    using column = std::deque<T>;
};

int main() {
    kk::basic_soa<deque_layout, int, std::string> scores;
    scores.push_back(10, "first");
    scores.push_back(20, "second");
    scores.push_back(30, "third");
    static_assert(std::ranges::random_access_range<decltype(scores.column<0>())>);
    static_assert(!std::ranges::contiguous_range<decltype(scores.column<0>())>);

    // 行のプロキシを ranges に渡し、条件に合う行だけを更新する。
    auto selected = scores | std::views::filter([](auto row) {
        return std::get<0>(row) >= 20;
    });
    std::size_t updated = 0;
    for (auto [score, name] : selected) {
        score += 5;
        name += "!";
        ++updated;
    }

    sample::check(updated == 2);
    sample::check(scores.read(0) == std::tuple{10, std::string("first")});
    sample::check(scores.read(1) == std::tuple{25, std::string("second!")});
    sample::check(scores.read(2) == std::tuple{35, std::string("third!")});
    std::cout << "custom columns: " << updated << " rows updated through filter\n";
}
