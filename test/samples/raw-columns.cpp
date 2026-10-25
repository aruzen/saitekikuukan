#include "saitekikuukan.hpp"
#include "check.hpp"

#include <type_traits>

struct point {
    float x;
    float y;
};

int main() {
    // 低水準 API は所有列を直接公開する。列長は利用側で管理する。
    kk::storage::dynamic_soa<point> columns;
    columns.x = {1, 2, 3};
    columns.y = {10, 20};
    std::size_t visited_columns = 0;
    kk::storage::for_each_column(columns, [&](auto& column) {
        column.reserve(8);
        ++visited_columns;
    });
    sample::check(visited_columns == 2);

    // 行ビューの長さは最短の列に合わせる。この例では2行を走査する。
    auto rows = kk::storage::rows(columns);
    sample::check(rows.size() == 2);
    for (auto row : rows)
        row.x += row.y;

    // 一時的な行ビューからの iterator も、元の列が生存していれば使える。
    auto first = kk::storage::rows(columns).begin();
    first->x += 1;
    sample::check(columns.x[0] == 12 && columns.x[1] == 22 && columns.x[2] == 3);

    const auto& constant = columns;
    auto readonly = kk::storage::rows(constant);
    static_assert(std::same_as<decltype(readonly.begin()->x), const float&>);
    sample::check(readonly.begin()->y == 10);
    std::cout << "raw columns: " << visited_columns << " columns, " << rows.size() << " rows\n";
}
