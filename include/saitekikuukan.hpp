//
// Created by morimoto_hibiki on 2025/08/16.
//
#pragma once

#ifndef SAITEKIKUUKAN_HPP
#define SAITEKIKUUKAN_HPP

#ifndef SAITEKIKUUKAN_NAMESPACE
#define SAITEKIKUUKAN_NAMESPACE kk
#endif

#ifdef __has_include
#  if __has_include(<boost/uuid/uuid.hpp>) || __has_include(<boost/uuid.hpp>)
#    define HAS_BOOST_UUID
#  endif
#endif

// TODO: 最後にalphabet順に並び替えようかな今回は
#include <array>
#include <concepts>
#include <coroutine>
#include <exception>
#include <expected>
#include <string>
#include <source_location>
#include <ranges>
#include <vector>
#include <tuple>
#include <type_traits>

namespace SAITEKIKUUKAN_NAMESPACE {
    namespace _meta {
        template<typename T>
        concept is_type_list = T::is_type_list_flag;

        namespace helper {
            template<typename T>
            concept has_bool_structure_type = std::same_as<std::true_type, typename T::type> ||
                                              std::same_as<std::false_type, typename T::type>;

            template<typename T>
            concept has_bool_value_type = requires {
                { T::value } -> std::convertible_to<bool>;
            };

            template<typename L1, typename L2, typename ...Types>
            auto _concat() {
                if constexpr (sizeof...(Types) == 0)
                    return L1::template append<L2>;
                return _concat<L1::template append<L2>, Types...>;
            }

            template<typename ...Types>
            using concat_t = decltype(_concat<Types...>());

            template<template<typename...> typename Filter, typename T>
            constexpr bool _apply_filter() {
                if constexpr (std::same_as<Filter<T>, std::true_type>) {
                    return true;
                } else if constexpr (has_bool_structure_type<Filter<T>>) {
                    if constexpr (std::same_as<typename Filter<T>::type, std::true_type>)
                        return true;
                } else if (has_bool_value_type<Filter<T>>) {
                    if constexpr (Filter<T>::value)
                        return true;
                } else {
                    static_assert(
                            "Invalid Filter<T> type: must provide either '::value', '::type' or same_as<std::true_type>");
                }
                return false;
            }

            template<template<typename...> typename Filter, typename N, typename T, typename ...Types>
            auto _filter() {
                using added = N::template add<T>;
                if constexpr (sizeof...(Types) == 0) {
                    if constexpr (_apply_filter<Filter, T>)
                        return added();
                    else
                        return N();
                } else {
                    if constexpr (_apply_filter<Filter, T>)
                        return _filter<Filter, added, Types...>();
                    else
                        return _filter<Filter, N, Types...>();
                }
            }

            template<size_t Elem, size_t Size>
            constexpr auto _array_push_back(std::array<size_t, Size> arr) {
                std::array < size_t, Size + 1 > added;
                for (size_t i = 0; i < Size; i++)
                    added[i] = arr[i];
                added[Size] = Elem;
                return added;
            }

            template<size_t Size>
            constexpr auto _array_pop_back(std::array<size_t, Size> arr) {
                std::array < size_t, Size - 1 > poped;
                for (size_t i = 0; i < Size - 1; i++)
                    poped[i] = arr[i];
                return poped;
            }

            template<template<typename...> typename Filter, size_t Current, size_t Size, std::array<size_t, Size> Arr, typename T, typename ...Types>
            constexpr auto _filtered_index() {
                if constexpr (sizeof...(Types) == 0) {
                    if constexpr (_apply_filter<Filter, T>)
                        return _array_push_back<Current, Size>(Arr);
                    else
                        return Arr;
                } else {
                    if constexpr (_apply_filter<Filter, T>)
                        return _filtered_index<Filter,
                                Current + 1, Size + 1, _array_push_back<Current, Size>(Arr), Types...>();
                    else
                        return _filtered_index<Filter, Current + 1, Size, Arr, Types...>();
                }
            }
        }

        template<typename ...Types>
        struct type_list {
            static constexpr bool is_type_list_flag = true;
            static constexpr size_t size = sizeof...(Types);

            template<size_t N>
            using get = typename std::tuple_element<N, std::tuple<Types...>>::type;

            template<typename T>
            static constexpr bool contains = std::disjunction_v<std::is_same<T, Types>...>;

            template<template<typename...> typename Filter>
            using filter = decltype(helper::_filter<Filter, type_list<>, Types...>());

            template<template<typename...> typename Filter>
            constexpr auto
            filtered_index() { return helper::_filtered_index<Filter, 0, 0, std::array<size_t, 0>, Types...>; }

            template<typename ...Adds>
            using add = type_list<Types..., Adds...>;

            template<typename ...Adds>
            using add_front = type_list<Adds..., Types...>;

            template<is_type_list List>
            using append = List::template add_front<Types...>;

            template<template<typename...> typename T>
            using wrap = T<Types...>;

            template<template<typename...> typename T>
            using wrap_with_reference = T<Types &...>;

            template<template<typename...> typename T>
            using wrap_with_pointer = T<Types *...>;
        };
    }
    using _meta::type_list;

// ==========================================================

    enum class error_code {
        out_of_range,
        not_found_id,
        buffer_over,
        generate_id_failed
    };

// ==========================================================

    namespace generator {
        template<typename T>
        concept is_generator = requires(T &t) {
            { t.next() } -> std::convertible_to<typename T::generate_type>;
        };

        template<typename T>
        struct coroutine_generator {
            using generate_type = size_t;

            struct promise_type {
                std::optional<T> current_value;

                coroutine_generator get_return_object() {
                    return coroutine_generator{
                            std::coroutine_handle<promise_type>::from_promise(*this)
                    };
                }

                std::suspend_always initial_suspend() noexcept { return {}; }

                std::suspend_always final_suspend() noexcept { return {}; }

                std::suspend_always yield_value(T value) noexcept {
                    current_value = std::move(value);
                    return {};
                }

                void return_void() {}

                void unhandled_exception() { throw; }
            };

            std::coroutine_handle<promise_type> handle;

            explicit coroutine_generator(std::coroutine_handle<promise_type> h) : handle(h) {}

            ~coroutine_generator() {
                if (handle) handle.destroy();
            }

            std::optional<T> next() {
                if (!handle.done()) {
                    handle.resume();
                    return handle.promise().current_value;
                }
                return std::nullopt;
            }
        };

        struct iota {
            using generate_type = size_t;
        private:
            size_t m_next_id = 0;
        public:
            inline size_t next() {
                return m_next_id++;
            }
        };

#if defined(HAS_BOOST_UUID)

        struct uuid {
            using generate_type = boost::uuids::uuid;

            inline static boost::uuids::time_generator_v7 m_uuid_generator;

            inline static boost::uuids::uuid next() {
                return m_uuid_generator();
            }
        };

#endif
    }

// ==========================================================

    namespace eventsystem {
        namespace event {
            template<typename IDType, typename ...Types>
            struct will_insert_event {
            private:
                bool m_canceled = false;

            public:
                IDType &id;
                std::tuple<Types &...> data;

                bool canceled() {
                    return m_canceled;
                }

                void cancel() {
                    m_canceled = true;
                }
            };

            template<typename IDType, typename ...Types>
            struct did_insert_event {
                const IDType id;
                const std::tuple<Types &...> data;
            };

            template<typename IDType, typename ...Types>
            struct will_delete_event {
            private:
                bool m_canceled = false;

            public:
                IDType &id;
                std::tuple<Types &...> data;

                bool canceled() {
                    return m_canceled;
                }

                void cancel() {
                    m_canceled = true;
                }
            };

            template<typename IDType, typename ...Types>
            struct did_delete_event {
                const IDType id;
                const std::tuple<Types &...> data;
            };
        }

        template<typename Event>
        struct listener_tag {
            using listen_event = Event;

            void on(Event &) {};
        };

        template<typename Event>
        struct listener_interface : public listener_tag<Event> {
            virtual void on(Event &) = 0;
        };

        template<typename T>
        concept is_eventsystem = requires(T &t) {
            { t.on(std::declval<event::did_delete_event<size_t, int, int> &>()) };
        };

        namespace helper {
            template<typename T>
            using not_empty = std::negation<std::is_empty<T>>;

            template<typename Event>
            struct event_targeting {
                template<typename T>
                struct is_subscribe {
                    static constexpr bool value = std::same_as<typename T::listen_event, Event>;
                };
            };
        }

        template<typename ...Events>
        struct registration {
        private:
            std::tuple<std::vector<listener_interface<Events> *>...> listeners;
        public:
            template<typename Event>
            listener_interface<Event> *add(listener_interface<Event> *listener) {
                get<Event>(listeners).push_back(listener);
                return listener;
            }

            template<typename Event>
            void remove(listener_interface<Event> *listener) {
                remove(get<Event>(listeners).begin(), get<Event>(listeners).end(), listener);
            }

            template<typename Event>
            void on(Event &e) {
                for (auto &listener: get<Event>(listeners))
                    listener->on(e);
            };
        };

        template<typename ...Events>
        struct one_event_each_only {
        private:
            std::tuple<listener_interface<Events> *...> listeners;
        public:
            template<typename Event>
            listener_interface<Event> *add(listener_interface<Event> *listener) {
                get<Event>(listeners) = listener;
                return listener;
            }

            template<typename Event>
            void remove() {
                get<Event>(listeners) = nullptr;
            }

            template<typename Event>
            void on(Event &e) {
                get<Event>(listeners)->on(e);
            };
        };

        /*
         * 実体を保持しない(EOB用)
         */
        template<_meta::is_type_list ListenerList>
        struct zero_cost {
        private:
            template<typename Event, size_t Idx>
            void apply(Event &e) {
                using Listener = ListenerList::template get<Idx>;
                if constexpr (std::same_as<typename Listener::listen_event, Event>)
                    Listener().on(e);
                if constexpr (Idx < ListenerList::size)
                    apply_class<Event, Idx + 1>();
            }

        public:
            template<typename Event>
            void on(Event &e) {
                apply<Event, 0>(e);
            };
        };

        /*
         * 空クラスは実体を保持しない
         */
        template<_meta::is_type_list ListenerList>
        struct minimum_cost {
            using empty_types = ListenerList::template filter<std::is_empty>;
            using not_empty_types = ListenerList::template filter<helper::not_empty>;
            not_empty_types::template wrap<std::tuple> listeners;

        private:
            template<typename Event, size_t Size, size_t Idx, std::array<size_t, Size> Rem>
            void apply_entities(Event &e) {
                get<Rem[Idx]>(listeners).on(e);
                if constexpr (Idx < Size)
                    apply_entities<Rem[Size], Size, Idx + 1, Rem>();
            }

            template<typename Event, size_t Idx>
            void apply_class(Event &e) {
                using Listener = empty_types::template get<Idx>;
                if constexpr (std::same_as<typename Listener::listen_event, Event>)
                    Listener().on(e);
                if constexpr (Idx < empty_types::size)
                    apply_class<Event, Idx + 1>();
            }

        public:
            template<typename Event>
            void on(Event &e) {
                constexpr auto not_empty_indices = not_empty_types::template filtered_index<helper::event_targeting<Event>::is_subscribe>();
                apply_class<Event, 0>(e);
                apply_entities<Event, not_empty_indices.size(), 0, not_empty_types>(e);
            };
        };

        struct dummy {
            template<typename Event>
            void on(Event &e) {};
        };
    }

// ==========================================================

    template<typename IDType = size_t, typename ...Types>
    struct dynamic_buffer {
        using id_type = IDType;
        using element_types = _meta::type_list<Types...>;

    public:
        std::vector<id_type> ids;

        /*
         * 急な話ですがReflectionが早く欲しいものですentriesでなくそれぞれに名前をつけてあげたい❤️
         */
        std::tuple<std::vector<Types>...> entries;

        template<std::size_t N>
        [[nodiscard]] auto &get() {
            static_assert(N <= element_types::size);
            return std::get<N>(entries);
        }

        template<std::size_t N>
        [[nodiscard]] const auto &get() const {
            static_assert(N <= element_types::size);
            return std::get<N>(entries);
        }

        [[nodiscard]] std::size_t size() const {
            return std::get<0>(entries).size();
        }

        [[nodiscard]] bool empty() const {
            return std::get<0>(entries).empty();
        }

        void resize(std::size_t n) {
            ((std::get<std::vector<Types>>(entries).resize(n)), ... );
        }
    };

    template<generator::is_generator IDGenerator = generator::iota, eventsystem::is_eventsystem EventSystem = eventsystem::dummy, size_t MaxSize = 16, typename ...Types>
    struct static_buffer {
        using this_type = static_buffer<IDGenerator, eventsystem::dummy, MaxSize, Types...>;

        static constexpr size_t max_size = MaxSize;
        using id_type = typename IDGenerator::generate_type;
        using element_types = _meta::type_list<Types...>;

        struct entry;
        struct iterator;
        // TODO
        struct const_iterator;

    private:
        [[no_unique_address]] IDGenerator m_generator;
        [[no_unique_address]] EventSystem m_eventsytem;
        size_t m_last_index = 0;
    public:
        std::array<id_type, max_size> ids;
        std::tuple<std::array<Types, max_size>...> entries;


        template<std::size_t N>
        [[nodiscard]] auto &get() {
            return std::get<N>(entries);
        }

        template<std::size_t N>
        [[nodiscard]] const auto &get() const {
            return std::get<N>(entries);
        }

        [[nodiscard]] std::size_t size() const {
            return std::get<0>(entries).size();
        }

        [[nodiscard]] bool empty() const {
            return std::get<0>(entries).empty();
        }

        std::expected<size_t, error_code> index(id_type id) const {
            for (size_t i = 0; i < size(); i++) {
                if (ids[i] == id)
                    return i;
            }
            return std::unexpected{error_code::not_found_id};
        }

        std::expected<entry, error_code> operator[](id_type id) {
            auto idx = index(id);
            if (idx.has_value())
                return idx.error();
            return entry{this, id, idx.value()};
        }

        std::expected<const entry, error_code> operator[](id_type id) const {
            auto idx = index(id);
            if (idx.has_value())
                return idx.error();
            return entry{this, id, idx.value()};
        }

        template<size_t N>
        std::expected<typename element_types::template get<N> &, error_code> get(id_type id) {
            auto idx = index(id);
            if (idx.has_value())
                return idx.error();
            return get<N>()[idx.value()];
        }

        template<size_t N>
        std::expected<const typename element_types::template get<N> &, error_code> get(id_type id) const {
            auto idx = index(id);
            if (idx.has_value())
                return idx.error();
            return get<N>()[idx.value()];
        }

        std::expected<void, error_code> push_back(Types&&... args) {
            if (max_size < m_last_index + 1)
                return std::unexpected{error_code::buffer_over};
            std::optional<id_type> id = m_generator.next();
            if (not id)
                return std::unexpected{error_code::generate_id_failed};
            ids[m_last_index] = id.value();
            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                ((std::get<Is>(entries)[m_last_index] = std::forward<Types>(args)), ...);
            }(std::make_index_sequence<sizeof...(Types)>{});
            m_last_index++;
        }

        void pop_back() {
            if (0 < m_last_index)
                m_last_index--;
        }

        void clear() {
            m_last_index = 0;
        }

        std::expected<void, error_code> erase(id_type id) {
            for (size_t i = 0; i < size(); i++) {
                if (ids[i] == id) {
                    ((swap(std::get<std::array<Types, max_size>>(entries)[i],
                           std::get<std::array<Types, max_size>>(entries)[m_last_index])), ... );
                    return {};
                }
            }
            return std::unexpected{error_code::not_found_id};
        }

        iterator begin() {
            return iterator(this);
        }

        iterator end() {

        }

        struct entry {
        private:
            this_type *parent;
            size_t index;
        public:
            id_type id() {
                return parent->ids[index];
            }

            template<size_t N>
            typename element_types::template get<N> &get() {
                return parent->template get<N>()[index];
            }

            template<size_t N>
            const typename element_types::template get<N> &get() const {
                return parent->template get<N>()[index];
            }
        };

        struct iterator {
            using iterator_category = std::random_access_iterator_tag;
            using value_type = entry;
            using difference_type = ptrdiff_t;
            using pointer = entry;
            using reference = entry;
        private:
            this_type *parent;
            size_t index;
        public:
            iterator() = default;

            iterator(const iterator &) = default;

            iterator &operator=(const iterator &) = default;

            reference operator*() const {
                return entry{parent, index};
            }

            pointer operator->() const {
                return entry{parent, index};
            }

            entry operator[](difference_type) const {

            }

            iterator &operator++() {
                index++;
                return *this;
            }

            iterator operator++(int) {
                return {parent, index++};
            }

            iterator &operator--() {
                index--;
                return *this;
            }

            iterator operator--(int) {
                return {parent, index--};
            }

            iterator &operator+=(difference_type diff) {
                index += diff;
                return *this;
            }

            iterator &operator-=(difference_type diff) {
                index -= diff;
                return *this;
            }

            operator bool() {
                return index <= parent->m_last_index;
            }

            bool operator!() {
                return parent->m_last_index < index;
            }

            friend iterator operator+(const iterator &, difference_type);

            friend iterator operator+(difference_type, const iterator &);

            friend iterator operator-(const iterator &, difference_type);

            friend difference_type operator-(const iterator &, const iterator &);

            friend bool operator==(const iterator &, const iterator &);

            friend bool operator!=(const iterator &, const iterator &);

            friend bool operator<(const iterator &, const iterator &);

            friend bool operator<=(const iterator &, const iterator &);

            friend bool operator>(const iterator &, const iterator &);

            friend bool operator>=(const iterator &, const iterator &);
        };
    };

    template<generator::is_generator G, eventsystem::is_eventsystem E, size_t M, typename ...Args>
    static_buffer<G, E, M, Args...>::iterator operator+(
            const typename static_buffer<G, E, M, Args...>::iterator &itr,
            typename static_buffer<G, E, M, Args...>::iterator::difference_type diff) {
        return {itr.parent, itr.index + diff};
    }

    template<generator::is_generator G, eventsystem::is_eventsystem E, size_t M, typename ...Args>
    typename static_buffer<G, E, M, Args...>::iterator
    operator+(typename static_buffer<G, E, M, Args...>::iterator::difference_type diff,
              const typename static_buffer<G, E, M, Args...>::iterator &itr) {
        return {itr.parent, itr.index + diff};
    }

    template<generator::is_generator G, eventsystem::is_eventsystem E, size_t M, typename ...Args>
    typename static_buffer<G, E, M, Args...>::iterator
    operator-(const typename static_buffer<G, E, M, Args...>::iterator &itr,
              typename static_buffer<G, E, M, Args...>::iterator::difference_type diff) {
        return {itr.parent, itr.index - diff};
    }

    template<generator::is_generator G, eventsystem::is_eventsystem E, size_t M, typename ...Args>
    typename static_buffer<G, E, M, Args...>::iterator::difference_type
    operator-(const typename static_buffer<G, E, M, Args...>::iterator &l,
              const typename static_buffer<G, E, M, Args...>::iterator &r) {
        return r.index - l.index;
    }

    template<generator::is_generator G, eventsystem::is_eventsystem E, size_t M, typename ...Args>
    bool operator==(const typename static_buffer<G, E, M, Args...>::iterator &l,
                    const typename static_buffer<G, E, M, Args...>::iterator &r) {
        if ((bool) l && (bool) r && l.dex == r.index)
            return true;
        return false;
    }

    template<generator::is_generator G, eventsystem::is_eventsystem E, size_t M, typename ...Args>
    bool operator!=(const typename static_buffer<G, E, M, Args...>::iterator &l,
                    const typename static_buffer<G, E, M, Args...>::iterator &r) {
        if ((bool) l != (bool) r)
            return true;
        if ((!l && !r) || l.index != r.index)
            return true;
        return false;
    }

    template<generator::is_generator G, eventsystem::is_eventsystem E, size_t M, typename ...Args>
    bool operator<(const typename static_buffer<G, E, M, Args...>::iterator &l,
                   const typename static_buffer<G, E, M, Args...>::iterator &r) {
        return l.index < r.index;
    }

    template<generator::is_generator G, eventsystem::is_eventsystem E, size_t M, typename ...Args>
    bool operator<=(const typename static_buffer<G, E, M, Args...>::iterator &l,
                    const typename static_buffer<G, E, M, Args...>::iterator &r) {
        return l.index <= r.index;
    }

    template<generator::is_generator G, eventsystem::is_eventsystem E, size_t M, typename ...Args>
    bool operator>(const typename static_buffer<G, E, M, Args...>::iterator &l,
                   const typename static_buffer<G, E, M, Args...>::iterator &r) {
        return l.index > r.index;
    }

    template<generator::is_generator G, eventsystem::is_eventsystem E, size_t M, typename ...Args>
    bool operator>=(const typename static_buffer<G, E, M, Args...>::iterator &l,
                    const typename static_buffer<G, E, M, Args...>::iterator &r) {
        return l.index >= r.index;
    }

    template<generator::is_generator G, eventsystem::is_eventsystem E, size_t ChunkSize = 16, typename ...Types>
    struct chunked_buffer {
        static constexpr size_t chunk_size = ChunkSize;
        using id_type = typename G::id_type;

        std::tuple<std::vector<static_buffer<G, E, ChunkSize, Types>>...> chunks;
    };
}

#endif //SAITEKIKUUKAN_HPP
