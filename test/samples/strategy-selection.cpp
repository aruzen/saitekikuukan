#include "saitekikuukan.hpp"
#include "check.hpp"

#include <string>
#include <tuple>

using schema = kk::type_list<int, std::string>;
using layout = kk::storage::chunked_layout<4>;

int main() {
    // 同じデータ配置・格納方式に、異なる ID 管理を組み合わせる。
    kk::buffer<schema, kk::strategy::structure_of_array, layout> rows;
    sample::check(rows.push_back(10, "plain").has_value());
    sample::check(std::get<0>(rows[0]) == 10);
    static_assert(!decltype(rows)::has_identity);

    using hash_ids = kk::identity::generated<>;
    kk::buffer<schema, kk::strategy::structure_of_array, layout, hash_ids> entities;
    sample::check(entities.push_back(20, "first").has_value());
    const auto first = entities.begin()->id();
    sample::check(entities.push_back(30, "second").has_value());
    const auto second = (entities.end() - 1)->id();
    sample::check(entities.erase(first).has_value());
    sample::check(entities[0].id() == second);
    sample::check(entities.find(second)->get<1>() == "second");

    // ID生成器を変えずに、索引を持たない線形検索へ切り替える。
    using linear_ids = kk::identity::generated<kk::generator::iota, kk::strategy::linear_id_lookup>;
    kk::buffer<schema, kk::strategy::structure_of_array, layout, linear_ids> small;
    sample::check(small.push_back(40, "linear").has_value());
    sample::check(small.find(small.begin()->id())->get<0>() == 40);
    std::cout << "strategy: plain rows / hash IDs / linear IDs\n";
}
