#include "saitekikuukan.hpp"
#include "check.hpp"

#include <string>

struct entity {
    int health = 100;
    std::string name = "anonymous";
};

using entities = kk::record_buffer<entity, kk::generator::iota, kk::eventsystem::dummy,
                                  kk::storage::dynamic_storage, 4>;

int main() {
    entities actors;
    sample::check(actors.resize(2).has_value());
    const auto player = actors.begin()->id();
    const auto removed = (actors.begin() + 1)->id();
    sample::check(actors.begin()->health == 100 && actors.columns().name[1] == "anonymous");

    // ID で取得した行にも構造体と同じ名前でアクセスする。
    {
        auto player_row = actors[player].value();
        player_row.name = "player";
        player_row.health -= 10;
    }
    actors.get<0>(player).value().get() -= 5;
    sample::check(actors[player].value().health == 85);

    sample::check(actors.push_back(entity{50, "enemy"}).has_value());
    const auto enemy = (actors.end() - 1)->id();
    sample::check(actors.erase(removed).has_value());
    sample::check(actors.index(enemy).value() == 1);
    sample::check(actors[enemy].value().name == "enemy");
    auto missing = actors[removed];
    sample::check(!missing && missing.error() == kk::error_code::not_found_id);

    const auto& constant = actors;
    static_assert(std::same_as<decltype(constant.begin()->health), const int&>);
    std::cout << "reflected buffer: " << constant[player].value().name
              << " has " << constant[player].value().health << " health\n";
}
