#include "saitekikuukan.hpp"
#include "check.hpp"

int main() {
    kk::chunked_soa<3, float, float, std::string> particles;
    particles.resize(7).value();
    auto xy = particles.select<0, 1>();
    // for_each はチャンクごとの連続列を走査し、選択順に引数を渡す。
    xy.for_each([](float& x, float& y) {
        x += 1;
        y += 2;
    });
    for (auto&& [x, y] : xy)
        x += y;
    sample::check(particles.column<0>()[6] == 3 && particles.column<2>()[6].empty());

    auto readonly = std::as_const(particles).select<1, 0>();
    float sum = 0;
    readonly.for_each([&](const float& y, const float& x) { sum += x + y; });
    sample::check(sum == 35);
    std::cout << "selected columns: chunk traversal and shared references\n";
}
