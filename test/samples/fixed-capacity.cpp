#include "saitekikuukan.hpp"
#include "check.hpp"

#include <stdexcept>
#include <string>
#include <tuple>

int main() {
    kk::fixed_soa<3, int, std::string> inventory;
    inventory.push_back(10, "sword");
    inventory.push_back(20, "shield");
    inventory.push_back(30, "potion");
    sample::check(inventory.size() == inventory.capacity());

    bool full = false;
    try {
        inventory.push_back(40, "helmet");
    } catch (const std::length_error&) {
        full = true;
    }
    sample::check(full && inventory.size() == 3);

    // 順序を保たない削除では、全列の末尾行が削除位置へ移る。
    inventory.erase_unordered(0);
    sample::check(inventory.read(0) == std::tuple{30, std::string("potion")});
    inventory.push_back(40, "helmet");
    sample::check(inventory.read(2) == std::tuple{40, std::string("helmet")});

    const auto& items = inventory;
    static_assert(std::same_as<decltype(items.column<1>()), std::span<const std::string>>);
    sample::check(items.column<1>()[1] == "shield");
    std::cout << "inventory: " << inventory.size() << " / " << inventory.capacity() << '\n';
}
