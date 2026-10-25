#include "saitekikuukan.hpp"
#include "check.hpp"

#include <algorithm>
#include <string>
#include <tuple>

using before_insert = kk::eventsystem::event::will_insert_event<std::size_t, int, std::string>;
using after_insert = kk::eventsystem::event::did_insert_event<std::size_t, int, std::string>;

struct validate_score : kk::eventsystem::listener_interface<before_insert> {
    void on(before_insert& event) override {
        auto& [score, name] = event.data;
        if (score < 0) {
            event.cancel();
            return;
        }
        score = std::min(score, 100);
        if (name.empty()) name = "anonymous";
    }
};

struct count_insertions : kk::eventsystem::listener_interface<after_insert> {
    std::size_t count = 0;
    std::size_t last_id = 0;

    void on(after_insert& event) override {
        ++count;
        last_id = event.id;
    }
};

using scores = kk::buffer<kk::generator::iota,
    kk::eventsystem::registration<before_insert, after_insert>,
    kk::storage::dynamic_storage, 4, int, std::string>;

int main() {
    // 登録先はリスナを所有しない。登録中はリスナを生存させる。
    validate_score validate;
    count_insertions inserted;
    scores values;
    values.events().add<before_insert>(&validate);
    values.events().add<after_insert>(&inserted);

    sample::check(values.push_back(30, "first").has_value());
    auto rejected = values.push_back(-1, "rejected");
    sample::check(!rejected && rejected.error() == kk::error_code::operation_canceled);
    sample::check(values.push_back(200, "").has_value());
    sample::check(values.size() == 2 && inserted.count == 2);
    sample::check(values.get<0>()[1] == 100 && values.get<1>()[1] == "anonymous");

    // キャンセルされた挿入にも ID を生成するため、ID は連番とは限らない。
    sample::check(inserted.last_id == 2);
    values.events().remove<before_insert>(&validate);
    values.events().remove<after_insert>(&inserted);
    sample::check(values.push_back(-5, "unfiltered").has_value());
    sample::check(values.get<0>()[2] == -5 && inserted.count == 2);
    std::cout << "events: " << inserted.count << " accepted notifications\n";
}
