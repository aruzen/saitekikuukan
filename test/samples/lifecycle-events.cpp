#include "saitekikuukan.hpp"
#include "check.hpp"

using deleting = kk::eventsystem::event::will_delete_row_event<int>;
using deleted = kk::eventsystem::event::did_delete_row_event<int>;
using updating = kk::eventsystem::event::will_update_row_event<int>;

struct protect_score : kk::eventsystem::listener_tag<deleting> {
    void on(deleting& event) {
        if (std::get<0>(event.data) == 42)
            event.cancel();
    }
};

struct count_deleted : kk::eventsystem::listener_tag<deleted> {
    std::size_t count = 0;

    void on(deleted&) { ++count; }
};

struct limit_score : kk::eventsystem::listener_tag<updating> {
    void on(updating& event) {
        std::get<0>(event.data) = std::clamp(std::get<0>(event.data), 0, 100);
    }
};

int main() {
    // 空のリスナは保持せず、削除件数を数えるリスナだけを保持する。
    using events = kk::eventsystem::minimum_cost<protect_score, count_deleted, limit_score>;
    kk::buffer<kk::type_list<int>, kk::strategy::structure_of_array, kk::storage::chunked_layout<2>,
        kk::identity::none, events>
        scores;
    scores.push_back(42).value();
    scores.push_back(7).value();
    sample::check(scores.erase_unordered(0).error() == kk::error_code::operation_canceled);
    scores.write(0, {200}).value();
    sample::check(scores.column<0>()[0] == 100);
    scores.swap_rows(0, 1).value();
    scores.pop_back().value();
    scores.clear().value();
    const auto count = std::get<count_deleted>(scores.events().listeners).count;
    sample::check(count == 2 && scores.empty());
    std::cout << "lifecycle events: " << count << " rows deleted\n";
}
