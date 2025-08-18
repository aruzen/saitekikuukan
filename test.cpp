#include <iostream>

#include <boost/uuid.hpp>

#include <saitekikuukan.hpp>
#include <print>

int main() {
    using namespace kk;

    struct A {
        [[no_unique_address]] generator::uuid i;
        [[no_unique_address]] eventsystem::dummy d;
        char a;
    };

    std::println("{}", sizeof(A));

    static_buffer<generator::uuid, eventsystem::dummy, 4, int, int> buf;
    std::println("sizeof(static_buffer<generator::uuid, eventsystem::dummy, 4, int, int>) : {}", sizeof(buf));
    std::println("sizeof(size_t) + sizeof(boost::uuids::uuid)*4 + sizeof(int)*(4+4) : {}", sizeof(size_t) + sizeof(boost::uuids::uuid)*4 + sizeof(int)*(4+4));

    std::tuple<std::array<int, 4>, std::array<int, 4>> d;

    buf.push_back(1, 2);
    buf.push_back(3, 4);
    buf.push_back(5, 2);

    using casted = int;
    auto *view = reinterpret_cast<casted *>(&buf);
    // for (;;) {
    for (size_t i = 0; i < sizeof(buf)/sizeof(casted); i++)
            std::print("{} ,", (int) view[i]);
    std::println("");
    for (auto& id : buf.ids)
        std::cout << id << ", " << std::endl;
    // }

    return 0;
}
