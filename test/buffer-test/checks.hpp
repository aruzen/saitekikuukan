#pragma once

#include <cstdlib>
#include <string>
#include <type_traits>

namespace buffer_test {
    inline void check(bool condition) {
        if (!condition)
            std::abort();
    }

    template<typename Buffer>
    void check_buffer() {
        Buffer values{};
        static_assert(std::random_access_iterator<typename Buffer::iterator>);
        static_assert(std::random_access_iterator<typename Buffer::const_iterator>);
        static_assert(std::ranges::random_access_range<Buffer>);
        static_assert(std::is_convertible_v<typename Buffer::iterator, typename Buffer::const_iterator>);
        check(values.empty() && values.begin() == values.end());
        values.pop_back();

        check(values.push_back(10, 20, std::string("one")).has_value());
        check(values.push_back(11, 21, std::string("two")).has_value());
        check(values.push_back(12, 22, std::string("three")).has_value());
        check(values.size() == 3 && values.end() - values.begin() == 3);
        check(values.begin()->id() == 0);
        check(values.index(0).value() == 0);
        check(values[1].value().template get<2>() == "two");
        values.template get<0>(0).value().get() = 100;
        check(values.begin()->template get<0>() == 100);
        values.begin()->template get<1>() = 200;
        check(values.template get<1>()[0] == 200);
        check(values.template get<0>(99).error() == kk::error_code::not_found_id);
        check(values[99].error() == kk::error_code::not_found_id);

        auto it = values.begin();
        check((it + 2)->id() == 2);
        check((it + 2) - it == 2 && it - (it + 2) == -2);
        check(it[1].template get<1>() == 21);
        auto previous = it++;
        check(previous == values.begin() && it == values.begin() + 1);
        check((--it) == values.begin());
        check(values.end() - 1 >= values.begin());
        const auto& constant = values;
        static_assert(std::is_same_v<decltype(constant.begin()->template get<0>()), const int&>);
        check(constant.template get<0>(0).value().get() == 100);
        check(constant[0].value().template get<0>() == 100);
        check(constant.cbegin() == values.begin());
        check(std::ranges::distance(values | std::views::reverse) == 3);

        check(values.erase(1).has_value());
        check(values.size() == 2 && values.index(2).value() == 1);
        check(values[2].value().template get<2>() == "three");
        check(values.erase(1).error() == kk::error_code::not_found_id);
        check(values.erase(2).has_value());
        check(values.erase(0).has_value() && values.empty());

        check(values.resize(3).has_value());
        check(values.size() == 3);
        check(values.storage.ids[0] != values.storage.ids[1]);
        check(values.storage.ids[1] != values.storage.ids[2]);
        for (std::size_t i = 0; i < values.size(); ++i)
            check(values.index(values.storage.ids[i]).value() == i);
        check(values.resize(5).error() == kk::error_code::buffer_over);
        check(values.size() == 3);
        check(values.resize(1).has_value());
        check(values.size() == 1 && values.end() - values.begin() == 1);
        check(values.storage.swap(0, 1).error() == kk::error_code::buffer_over);

        values.clear();
        for (int i = 0; i < 4; ++i)
            check(values.push_back(i, i * 2, std::to_string(i)).has_value());
        check(values.push_back(4, 8, std::string("overflow")).error() == kk::error_code::buffer_over);
        check(values.size() == 4 && values.storage.swap(0, 0).has_value());
        Buffer another{};
        check(values.begin() != another.begin());
    }

    using before_insert = kk::eventsystem::event::will_insert_event<std::size_t, int, int, std::string>;
    using after_insert = kk::eventsystem::event::did_insert_event<std::size_t, int, int, std::string>;

    struct insert_events {
        using event_types = kk::type_list<before_insert, after_insert>;
        int before_count = 0;
        int after_count = 0;
        bool cancel_next = false;

        void on(before_insert& event) {
            ++before_count;
            if (cancel_next) {
                cancel_next = false;
                event.cancel();
            } else {
                std::get<0>(event.data) += 100;
            }
        }

        void on(after_insert& event) {
            ++after_count;
            ++std::get<1>(event.data);
        }
    };

    template<typename Buffer>
    void check_events() {
        Buffer values{};
        check(values.push_back(1, 2, std::string("first")).has_value());
        check(values.begin()->template get<0>() == 101);
        check(values.begin()->template get<1>() == 3);
        check(values.events().before_count == 1 && values.events().after_count == 1);
        values.events().cancel_next = true;
        check(values.push_back(4, 5, std::string("cancel")).error() == kk::error_code::operation_canceled);
        check(values.size() == 1 && values.events().after_count == 1);
        check(values.push_back(6, 7, std::string("second")).has_value());
        check((values.begin() + 1)->id() == 2);
        check((values.begin() + 1)->template get<0>() == 106);
        check(values.push_back(8, 9, std::string("overflow")).error() == kk::error_code::buffer_over);
        check(values.events().before_count == 3 && values.events().after_count == 2);
    }

    struct cancel_listener : kk::eventsystem::listener_interface<before_insert> {
        void on(before_insert& event) override { event.cancel(); }
    };

    template<typename Buffer>
    void check_registered_events() {
        Buffer values{};
        check(values.push_back(1, 2, std::string("no listener")).has_value());
        cancel_listener listener;
        values.events().template add<before_insert>(&listener);
        check(values.push_back(3, 4, std::string("cancel")).error() == kk::error_code::operation_canceled);
        if constexpr (requires { values.events().template remove<before_insert>(&listener); })
            values.events().template remove<before_insert>(&listener);
        else
            values.events().template remove<before_insert>();
        check(values.push_back(5, 6, std::string("removed")).has_value());
        check(values.size() == 2);
    }
}
