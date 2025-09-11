#include "saitekikuukan.hpp"
#include <print>
#include <iostream>

int main() {
    using namespace kk;
    constexpr size_t Size = 1024 * 10;

    buffer<generator::iota, eventsystem::dummy, storage::static_storage, Size, int, int, int, int, int, int> ss;
    buffer<generator::iota, eventsystem::dummy, storage::dynamic_storage, Size, int, int, int, int, int, int> ds;
    ss.resize(Size);
    ds.resize(Size);

    auto& [sa, sb, sc, sd, sx, sy] = ss.storage.entries;
    auto& [da, db, dc, dd, dx, dy] = ds.storage.entries;
    auto [psx, psy] = std::make_tuple(sx.data(), sy.data());
    auto [pdx, pdy] = std::make_tuple(dx.data(), dy.data());

    size_t loop;
    std::cin >> loop;

    for (size_t i = 0; i < 3; i++) {
        {
            auto start = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < loop; i++) {
                for (size_t j = 0; j < Size; j++) {
                    dx[j] += j * 2;
                    dy[j] += Size - j;
                }
            }
            auto end = std::chrono::high_resolution_clock::now();
            std::println("{}µs", std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
        }

        {
            auto start = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < loop; i++) {
                for (size_t j = 0; j < Size; j++) {
                    sx[j] += j * 2;
                    sy[j] += Size - j;
                }
            }
            auto end = std::chrono::high_resolution_clock::now();
            std::println("{}µs", std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
        }
    }
    std::println("{}, {}", ss.get<4>()[Size - 1], ss.get<5>()[Size - 1]);
    std::println("{}, {}", ds.get<4>()[Size - 1], ds.get<5>()[Size - 1]);

    return 0;
}
