#include "saitekikuukan.hpp"
#include <print>

int main() {
    using namespace kk;

    struct enemy {
        int max_hp, hp, max_mp, mp, x, y;
    };

    constexpr size_t Size = 1024 * 10;

    buffer<generator::iota, eventsystem::dummy, storage::static_storage, Size, int, int, int, int, int, int> enemies_of_array;
    enemies_of_array.resize(Size);
    auto &[max_hp, hp, max_mp, mp, x, y] = enemies_of_array.storage.entries;
    auto& elem_x = enemies_of_array.get<4>();
    auto& elem_y = enemies_of_array.get<5>();
    auto [max_hp_p, hp_p, max_mp_p, mp_p, x_p, y_p] = std::make_tuple(reinterpret_cast<int*>(&max_hp), reinterpret_cast<int*>(&hp), reinterpret_cast<int*>(&max_mp), reinterpret_cast<int*>(&mp), reinterpret_cast<int*>(&x), reinterpret_cast<int*>(&y));


    std::array<enemy, Size> array_of_enemies{};

    {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < Size; i++) {
            for (size_t j = 0; j < Size; j++) {
                x[j] += i;
                y[j] += Size - i;
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        std::println("{}µs", std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
    }

    {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < Size; i++) {
            for (size_t j = 0; j < Size; j++) {
                elem_x[j] += i;
                elem_y[j] += Size - i;
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        std::println("{}µs", std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
    }

    {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < Size; i++) {
            for (size_t j = 0; j < Size; j++) {
                x_p[j] += i;
                y_p[j] += Size - i;
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        std::println("{}µs", std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
    }

    {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < Size; i++) {
            for (auto entry : enemies_of_array) {
                entry.get<4>() += i;
                entry.get<5>() += Size - i;
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        std::println("{}µs", std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
    }

    {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < Size; i++) {
            for (size_t j = 0; j < Size; j++) {
                array_of_enemies[j].x += i;
                array_of_enemies[j].y += Size - i;
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        std::println("{}µs", std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
    }
    return 0;
}
