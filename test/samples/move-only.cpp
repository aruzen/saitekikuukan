#include "saitekikuukan.hpp"
#include "check.hpp"

#include <memory>
#include <tuple>
#include <utility>

int main() {
    using jobs = kk::soa<int, std::unique_ptr<int>>;
    static_assert(!std::is_copy_constructible_v<jobs>);
    jobs pending;
    sample::check(pending.push_back(7, std::make_unique<int>(70)).has_value());
    sample::check(pending.push_back(8, std::make_unique<int>(80)).has_value());

    auto running = std::move(pending);
    sample::check(pending.empty() && running.size() == 2);
    // 行の分解で受け取る unique_ptr は参照なので、所有権を移さず更新できる。
    for (auto [id, value] : running)
        *value += id;

    running.erase_unordered(0);
    sample::check(running.column<0>()[0] == 8 && *running.column<1>()[0] == 88);
    sample::check(pending.push_back(9, std::make_unique<int>(90)).has_value());
    sample::check(pending.size() == 1);

    running.clear();
    sample::check(running.empty());
    std::cout << "move-only: moved-from container reused, job=" << pending.column<0>()[0] << '\n';
}
