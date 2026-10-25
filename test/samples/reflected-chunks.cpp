#include "saitekikuukan.hpp"
#include "check.hpp"

#include <span>
#include <string>

struct vertex {
    float x = 1;
    float y = 2;
    std::string group = "default";
};

int main() {
    kk::record_chunked_soa<vertex, 4> vertices;
    vertices.resize(10);
    sample::check(vertices.chunk_count() == 3 && vertices.chunk(2).size() == 2);
    float* first_x = &vertices[0].x;

    for (auto chunk : vertices.chunks()) {
        auto storage = chunk.columns();
        for (std::size_t i = 0; i < chunk.size(); ++i) {
            storage.x[i] += 5;
            storage.y[i] -= 1;
        }
    }

    // 行の iterator とメンバ指定の列は、チャンク境界をまたいで使える。
    sample::check((vertices.begin() + 4)->x == 6);
    vertices.column<^^vertex::x>()[5] = 42;
    sample::check(vertices[5].x == 42 && vertices[5].group == "default");

    vertices.push_back(vertex{100, 200, "appended"});
    sample::check(first_x == &vertices[0].x && vertices.chunk(2).size() == 3);
    vertices.erase_unordered(1);
    sample::check(vertices[1].x == 100 && vertices[1].group == "appended");

    const auto& constant = vertices;
    static_assert(std::same_as<decltype(constant.chunk(0).columns().x), std::span<const float>>);
    sample::check(constant.size() == 10 && constant.chunk(2).size() == 2);
    std::cout << "reflected chunks: " << constant.chunk_count()
              << " chunks, moved row=" << constant[1].group << '\n';
}
