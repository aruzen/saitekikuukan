#include "saitekikuukan.hpp"
#include "check.hpp"

#include <span>
#include <string>

struct particle {
    float x = 0;
    float y = 10;
    float vx = 2;
    float vy = -4;
    std::string name = "unnamed";
};

int main() {
    kk::buffer<particle> values;
    sample::check(values.resize(2).has_value());
    sample::check(values[0].y == 10 && values[1].name == "unnamed");

    // 構造体を指定するだけで、メンバ名に対応する列ビューを取得できる。
    auto storage = values.columns();
    for (std::size_t i = 0; i < values.size(); ++i) {
        storage.x[i] += storage.vx[i] * 0.5f;
        storage.y[i] += storage.vy[i] * 0.5f;
    }
    sample::check(values.begin()->x == 1 && values.begin()->y == 8);

    values.begin()->name = "first";
    values.column<^^particle::y>()[1] = 30;
    // read は元の構造体の値を返す。変更を反映するには write する。
    particle saved = values.read(1);
    saved.name = "copied";
    sample::check(values[1].name == "unnamed");
    values.write(0, saved).value();
    sample::check(values[0].y == 30 && values[0].name == "copied");

    const auto& constant = values;
    static_assert(std::same_as<decltype(constant.columns().x), std::span<const float>>);
    static_assert(std::same_as<decltype(constant.begin()->name), const std::string&>);
    std::cout << "reflection: " << constant.begin()->name << " at (" << constant.begin()->x << ", "
              << constant.begin()->y << ")\n";
}
