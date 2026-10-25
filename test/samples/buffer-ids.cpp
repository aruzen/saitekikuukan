#include "saitekikuukan.hpp"
#include "check.hpp"

#include <string>

using entities = kk::buffer<kk::type_list<int, std::string>, kk::strategy::structure_of_array,
    kk::storage::dynamic_layout<3>, kk::identity::generated<>>;

int main() {
    entities values;
    sample::check(values.push_back(100, "player").has_value());
    const auto player = values.begin()->id();
    sample::check(values.push_back(50, "enemy").has_value());
    const auto enemy = (values.end() - 1)->id();
    sample::check(values.push_back(25, "item").has_value());
    const auto item = (values.end() - 1)->id();

    // 物理インデックスが変わっても、生存している行は同じ ID で検索する。
    sample::check(values.erase(enemy).has_value());
    sample::check(values.index(item).value() == 1);
    sample::check(values.find(item).value().get<1>() == "item");
    auto missing = values.find(enemy);
    sample::check(!missing && missing.error() == kk::error_code::not_found_id);

    values.get<0>(player).value().get() -= 10;
    const auto& constant = values;
    sample::check(constant.get<0>(player).value().get() == 90);

    // buffer は失敗理由を expected で返す。
    sample::check(values.push_back(10, "npc").has_value());
    auto overflow = values.push_back(1, "overflow");
    sample::check(!overflow && overflow.error() == kk::error_code::buffer_over);
    sample::check(values.size() == 3);
    std::cout << "buffer: item ID=" << item << ", current index=" << values.index(item).value()
              << '\n';
}
