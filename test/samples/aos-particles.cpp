#include "saitekikuukan.hpp"
#include "check.hpp"

struct particle {
    float x = 1, y = 2;
    std::string name = "default";
};

int main() {
    kk::buffer<particle, kk::strategy::array_of_structure> particles;
    particles.resize(3).value();
    particles.begin()->x = 10;
    auto storage = particles.columns();
    storage.y[0] = 20;
    particles.column<^^particle::name>()[0] = "first";
    sample::check(particles[0].x == 10 && particles[0].y == 20 && particles[0].name == "first");

    using ids = kk::identity::recycled<kk::strategy::sparse_id_lookup<>>;
    kk::buffer<particle, kk::strategy::array_of_structure, kk::storage::fixed_layout<8>, ids>
        entities;
    entities.append_range(particles).value();
    auto first = entities.begin()->id();
    entities.find(first)->x = 30;
    sample::check(entities.find(first)->name == "first" && entities.read(0).x == 30);
    entities.erase(first).value();
    entities.push_back(particle{}).value();
    sample::check(!entities.find(first));
    std::cout << "reflected AoS: named columns and generational IDs\n";
}
