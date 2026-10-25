#include "saitekikuukan.hpp"
#include "check.hpp"

#include <tuple>

using particles = kk::chunked_soa<128, float, float, float, float>;

void step(particles& values, float dt) {
    for (auto chunk : values.chunks()) {
        auto [x, y, vx, vy] = chunk.columns();
        // 最後のチャンクも、有効行だけを処理する。
        for (std::size_t i = 0; i < chunk.size(); ++i) {
            x[i] += vx[i] * dt;
            y[i] += vy[i] * dt;
        }
    }
}

int main() {
    particles values;
    for (int i = 0; i < 300; ++i)
        sample::check(values.push_back(float(i), float(-i), 2.0f, 4.0f).has_value());

    sample::check(values.chunk_count() == 3);
    sample::check(values.chunk(0).size() == 128 && values.chunk(2).size() == 44);
    float* first_x = &values.column<0>()[0];
    values.reserve(1024);
    sample::check(first_x == &values.column<0>()[0]);
    step(values, 0.5f);

    for (std::size_t i = 0; i < values.size(); ++i) {
        sample::check(values.column<0>()[i] == float(i) + 1);
        sample::check(values.column<1>()[i] == -float(i) + 2);
    }

    // 全体の列はランダムアクセス、チャンク内の列は連続範囲。
    static_assert(!std::ranges::contiguous_range<decltype(values.column<0>())>);
    static_assert(std::ranges::contiguous_range<decltype(values.chunk(0).column<0>())>);
    const auto last = values.read(299);
    values.erase_unordered(0);
    sample::check(values.size() == 299 && values.read(0) == last);
    sample::check(values.chunk(2).size() == 43);
    std::cout << "chunks: 128 + 128 + " << values.chunk(2).size() << " rows\n";
}
