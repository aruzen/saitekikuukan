#include "saitekikuukan.hpp"
#include "check.hpp"

#include <tuple>

using particles = kk::soa<float, float, float, float>;

void step(particles& values, float dt) {
    // 列ビューを構造化束縛で受け取り、位置と速度をまとめて走査する。
    auto [x, y, vx, vy] = values.columns();
    for (std::size_t i = 0; i < values.size(); ++i) {
        x[i] += vx[i] * dt;
        y[i] += vy[i] * dt;
    }
}

int main() {
    particles values;
    values.reserve(8);
    sample::check(values.push_back(0.0f, 10.0f, 2.0f, -4.0f).has_value());
    sample::check(values.push_back(5.0f, 20.0f, 4.0f, -2.0f).has_value());
    step(values, 0.5f);

    sample::check(values.read(0) == std::tuple{1.0f, 8.0f, 2.0f, -4.0f});
    sample::check(values.read(1) == std::tuple{7.0f, 19.0f, 4.0f, -2.0f});

    // 行は参照プロキシ。値の保存・書き戻しには read/write を使う。
    for (auto [x, y, vx, vy] : values) {
        x += 1;
        y += 2;
        sample::check(vx > 0 && vy < 0);
    }

    auto saved = values.read(0);
    std::get<0>(saved) = 100;
    sample::check(values.column<0>()[0] == 2);
    values.write(0, saved).value();
    sample::check(values.column<0>()[0] == 100);

    std::cout << "particles: x=" << values.column<0>()[0] << ", y=" << values.column<1>()[0]
              << '\n';
}
