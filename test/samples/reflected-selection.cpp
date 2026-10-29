#include "saitekikuukan.hpp"
#include "check.hpp"

struct particle {
    float x = 1, y = 2;
    std::string name = "untouched";
};

int main() {
    using ids = kk::identity::recycled<kk::strategy::sparse_id_lookup<>>;
    kk::buffer<particle, kk::strategy::structure_of_array, kk::storage::chunked_layout<3>, ids>
        particles;
    particles.resize(7).value();
    auto id = particles.begin()->id();
    auto xy = particles.select<^^particle::x, ^^particle::y>();
    xy.for_each([](float& x, float& y) { x += y; });
    for (auto&& [x, y] : xy | std::views::take(2))
        y += x;
    sample::check(particles.find(id)->x == 3 && particles.find(id)->y == 5);
    sample::check(particles.read(6).y == 2 && particles.read(0).name == "untouched");
    std::cout << "reflected selection: member selectors and generational IDs\n";
}
