#include "saitekikuukan.hpp"
#include "check.hpp"

int main() {
    using rows_type = kk::buffer<kk::type_list<int, std::string>, kk::strategy::array_of_structure>;
    rows_type rows;
    rows.push_back(3, "three").value();
    rows.push_back(1, "one").value();
    // AoS の行は実体への参照。auto&& は SoA の参照プロキシにも使える。
    for (auto&& [number, name] : rows)
        ++number;
    auto [numbers, names] = rows.columns();
    numbers[0] = 5;
    sample::check(std::get<0>(rows[0]) == 5 && names[1] == "one");
    rows.sort(std::ranges::less{}, [](const auto& row) { return std::get<0>(row); }).value();
    sample::check(std::get<0>(rows[0]) == 2);
    rows_type copy;
    copy.append_range(rows).value();
    sample::check(copy.read(1) == rows.read(1));
    std::cout << "AoS: " << rows.size() << " contiguous rows with projected columns\n";
}
