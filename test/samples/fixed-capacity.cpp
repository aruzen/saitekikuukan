#include "saitekikuukan.hpp"
#include "check.hpp"

#include <stdexcept>
#include <string>
#include <tuple>

int main() {
    kk::fixed_soa<3, int, std::string> inventory;
    sample::check(inventory.push_back(10, "sword").has_value());
    sample::check(inventory.push_back(20, "shield").has_value());
    sample::check(inventory.push_back(30, "potion").has_value());
    sample::check(inventory.size() == inventory.capacity());

    auto full = inventory.push_back(40, "helmet");
    sample::check(!full && full.error() == kk::error_code::buffer_over);
    sample::check(inventory.size() == 3);

    // 順序を保たない削除では、全列の末尾行が削除位置へ移る。
    inventory.erase_unordered(0);
    sample::check(inventory.read(0) == std::tuple{30, std::string("potion")});
    sample::check(inventory.push_back(40, "helmet").has_value());
    sample::check(inventory.read(2) == std::tuple{40, std::string("helmet")});

    const auto& items = inventory;
    static_assert(std::same_as<decltype(items.column<1>()), std::span<const std::string>>);
    sample::check(items.column<1>()[1] == "shield");
    std::cout << "inventory: " << inventory.size() << " / " << inventory.capacity() << '\n';
}
