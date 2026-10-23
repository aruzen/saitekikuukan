//
// Created by morimoto_hibiki on 2025/08/16.
//
#pragma once

#ifndef SAITEKIKUUKAN_HPP
#define SAITEKIKUUKAN_HPP

#ifndef SAITEKIKUUKAN_NAMESPACE
#define SAITEKIKUUKAN_NAMESPACE kk
#endif

// #define HAS_BOOST_UUID

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
#include <optional>
#include <functional>
#include <cstdint>
#include <algorithm>
#include "saitekikuukan/soa.hpp"

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
                    if constexpr (_apply_filter<Filter, T>())
                        return added();
                    else
                        return N();
                } else {
                    if constexpr (_apply_filter<Filter, T>())
                        return _filter<Filter, added, Types...>();
                    else
                        return _filter<Filter, N, Types...>();
                }
            }

            template<size_t Elem, size_t Size>
            constexpr auto _array_push_back(std::array<size_t, Size> arr) {
                std::array<size_t, Size + 1> added;
                for (size_t i = 0; i < Size; i++)
                    added[i] = arr[i];
                added[Size] = Elem;
                return added;
            }

            template<size_t Size>
            constexpr auto _array_pop_back(std::array<size_t, Size> arr) {
                std::array<size_t, Size - 1> poped;
                for (size_t i = 0; i < Size - 1; i++)
                    poped[i] = arr[i];
                return poped;
            }

            template<template<typename...> typename Filter, size_t Current, size_t Size, std::array<size_t, Size> Arr, typename T, typename ...Types>
            constexpr auto _filtered_index() {
                if constexpr (sizeof...(Types) == 0) {
                    if constexpr (_apply_filter<Filter, T>())
                        return _array_push_back<Current, Size>(Arr);
                    else
                        return Arr;
                } else {
                    if constexpr (_apply_filter<Filter, T>())
                        return _filtered_index<Filter,
                                Current + 1, Size + 1, _array_push_back<Current, Size>(Arr), Types...>();
                    else
                        return _filtered_index<Filter, Current + 1, Size, Arr, Types...>();
                }
            }

            template<size_t MetaID, typename T>
            struct id_appended;

            template<size_t MetaID, typename T> requires std::is_class_v<T>
            struct id_appended<MetaID, T> : public T {
                static constexpr size_t meta_id = MetaID;
                using T::T;
            };

            template<size_t MetaID, typename T> requires (not std::is_class_v<T>)
            struct id_appended<MetaID, T> {
                static constexpr size_t meta_id = MetaID;
                T value;

                operator T &() {
                    return value;
                }

                operator const T &() const {
                    return value;
                }
            };

            template<size_t I, typename List, typename T, typename... Types>
            constexpr auto _append_id() {
                using NewList = List::template add<id_appended<I, T>>;
                if constexpr (sizeof...(Types) == 0) {
                    return NewList();
                } else {
                    return _append_id<I + 1, NewList, Types...>();
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
            using apply = T<Types...>;

            template<template<typename...> typename T>
            using reference = type_list<Types &...>;

            template<template<typename...> typename T>
            using pointer = type_list<Types *...>;

            template<template<typename...> typename T>
            using wrap = type_list<T<Types>...>;

            /*
             * これのStartは価値がないが遅延評価させるtemplateのために必要
             */
            template<size_t Start = 0>
            using wrap_meta_id = decltype(helper::_append_id<Start, type_list<>, Types...>());

            template<size_t Size>
            using wrap_array = type_list<std::array<Types, Size>...>;
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
            typename T::generate_type;
            { t.next() } -> std::convertible_to<std::optional<typename T::generate_type>>;
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
            inline std::optional<size_t> next() {
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
                will_insert_event(IDType &id, std::tuple<Types &...> data) : id(id), data(data) {};

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
                did_insert_event(IDType id, std::tuple<Types &...> data) : id(id), data(data) {};

                const IDType id;
                const std::tuple<Types &...> data;
            };

            template<typename IDType, typename ...Types>
            struct will_delete_event {
            private:
                bool m_canceled = false;
            public:
                will_delete_event(IDType &id, std::tuple<Types &...> data) : id(id), data(data) {};

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
                did_delete_event(IDType id, std::tuple<Types &...> data) : id(id), data(data) {};

                const IDType id;
                const std::tuple<Types &...> data;
            };

            template<typename IDType, typename ...Types>
            struct will_swap_event {
            private:
                bool m_canceled = false;
            public:
                will_swap_event(IDType &id, std::tuple<Types &...> first, std::tuple<Types &...> second) : id(id), first(first), second(second) {};

                IDType &id;
                std::tuple<Types &...> first, second;

                bool canceled() {
                    return m_canceled;
                }

                void cancel() {
                    m_canceled = true;
                }
            };

            template<typename IDType, typename ...Types>
            struct did_swap_event {
                did_swap_event(IDType id, std::tuple<Types &...> first, std::tuple<Types &...> second) : id(id), first(first), second(second) {};

                const IDType id;
                const std::tuple<Types &...> first, second;
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

        template<typename T, typename E>
        concept eventsystem_for = requires(T t, E &e) {
            { t.on(e) };
        };

        template<typename T>
        concept is_eventsystem = requires {
            typename T::event_types;
        } && []<typename... Ev>(type_list<Ev...> *) {
            return (eventsystem_for<T, Ev> && ...);
        }(static_cast<typename T::event_types *>(nullptr));

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
            using event_types = type_list<Events...>;
        private:
            std::tuple<std::vector<listener_interface<Events> *>...> listeners;
        public:
            template<typename Event>
            listener_interface<Event> *add(listener_interface<Event> *listener) {
                std::get<Event>(listeners).push_back(listener);
                return listener;
            }

            template<typename Event>
            void remove(listener_interface<Event> *listener) {
                std::remove(std::get<Event>(listeners).begin(), std::get<Event>(listeners).end(), listener);
            }

            template<typename Event>
            void on(Event &e) {
                for (auto &listener: std::get<Event>(listeners))
                    listener->on(e);
            };
        };

        template<typename ...Events>
        struct one_event_each_only {
            using event_types = type_list<Events...>;
        private:
            std::tuple<listener_interface<Events> *...> listeners;
        public:
            template<typename Event>
            listener_interface<Event> *add(listener_interface<Event> *listener) {
                std::get<Event>(listeners) = listener;
                return listener;
            }

            template<typename Event>
            void remove() {
                std::get<Event>(listeners) = nullptr;
            }

            template<typename Event>
            void on(Event &e) {
                std::get<Event>(listeners)->on(e);
            };
        };

        /*
         * 実体を保持しない(EOB用)
         */
        template<typename... Types>
        struct zero_cost {
            using listener_types = type_list<Types...>;
        private:
            template<typename Event, size_t Idx>
            void apply(Event &e) {
                using Listener = listener_types::template get<Idx>;
                if constexpr (std::same_as<typename Listener::listen_event, Event>)
                    Listener().on(e);
                if constexpr (Idx < listener_types::size)
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
        template<typename... Types>
        struct minimum_cost {
            using listener_types = type_list<Types...>;
            using empty_types = listener_types::template filter<std::is_empty>;
            using not_empty_types = listener_types::template filter<helper::not_empty>;
            not_empty_types::template apply<std::tuple> listeners;

        private:
            template<typename Event, size_t Size, size_t Idx, std::array<size_t, Size> Rem>
            void apply_entities(Event &e) {
                std::get<Rem[Idx]>(listeners).on(e);
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
            using event_types = type_list<>;

            template<typename Event>
            void on(Event &e) {};
        };
    }

// ==========================================================

    namespace storage {
        template<template<typename, size_t, typename...> typename T>
        concept is_storage = requires(T<size_t, 1, int> t, const T<size_t, 1, int> tc) {
            T<size_t, 1, int>::default_max_size;
            T<size_t, 1, int>::max_size;
            typename T<size_t, 1, int>::id_type;
            t.template get<0>();
            tc.id_at(0);

            t.resize(std::declval<size_t>());
            t.push_back(std::declval<size_t>(), std::declval<int>());
            t.pop_back();
            t.swap(std::declval<size_t>(), std::declval<size_t>());
            t.clear();

            tc.size();
            tc.empty();
            tc.index(std::declval<size_t>());
        };

        template<typename ...Types>
        struct dynamic_entries_type {
            using tl = type_list<Types...>;
            using vector = tl::template wrap<std::vector>;
            // using meta_id = vector::template wrap_meta_id<0>;
            using result = vector::template apply<std::tuple>;
        };

        template<size_t Size, typename ...Types>
        struct static_entries_type {
            using tl = type_list<Types...>;
            using array = tl::template wrap_array<Size>;
            // using meta_id = array::template wrap_meta_id<0>;
            using result = array::template apply<std::tuple>;
        };

        constexpr size_t DynamicSizeUnlimited = SIZE_MAX;

        template<typename IDType, size_t MaxSize = DynamicSizeUnlimited, typename... Types>
        class dynamic_storage {
            DynamicColumns<IDType, Types...> _columns;

        public:
            static constexpr size_t default_max_size = DynamicSizeUnlimited;
            static constexpr size_t max_size = MaxSize;
            using id_type = IDType;

            size_t size() const noexcept;
            bool empty() const noexcept;
            void clear() noexcept;
            std::expected<void, error_code> resize(size_t count);
            template<typename... Args>
            std::expected<void, error_code> push_back(id_type id, Args&&... args);
            void pop_back() noexcept;
            std::expected<void, error_code> swap(size_t a_idx, size_t b_idx);
            std::expected<size_t, error_code> index(id_type id) const;
            id_type id_at(size_t index) const;
            template<size_t Index>
            auto get() noexcept;
            template<size_t Index>
            auto get() const noexcept;
        };

        template<typename IDType, size_t MaxSize, typename... Types>
        size_t dynamic_storage<IDType, MaxSize, Types...>::size() const noexcept {
            return _columns.size();
        }

        template<typename IDType, size_t MaxSize, typename... Types>
        bool dynamic_storage<IDType, MaxSize, Types...>::empty() const noexcept {
            return _columns.empty();
        }

        template<typename IDType, size_t MaxSize, typename... Types>
        void dynamic_storage<IDType, MaxSize, Types...>::clear() noexcept {
            _columns.clear();
        }

        template<typename IDType, size_t MaxSize, typename... Types>
        std::expected<void, error_code> dynamic_storage<IDType, MaxSize, Types...>::resize(size_t count) {
            if (count > max_size)
                return std::unexpected{error_code::buffer_over};
            _columns.resize(count);
            return {};
        }

        template<typename IDType, size_t MaxSize, typename... Types>
        template<typename... Args>
        std::expected<void, error_code> dynamic_storage<IDType, MaxSize, Types...>::push_back(id_type id, Args&&... args) {
            if (size() == max_size)
                return std::unexpected{error_code::buffer_over};
            _columns.push_back(std::move(id), std::forward<Args>(args)...);
            return {};
        }

        template<typename IDType, size_t MaxSize, typename... Types>
        void dynamic_storage<IDType, MaxSize, Types...>::pop_back() noexcept {
            _columns.pop_back();
        }

        template<typename IDType, size_t MaxSize, typename... Types>
        std::expected<void, error_code> dynamic_storage<IDType, MaxSize, Types...>::swap(size_t a_idx, size_t b_idx) {
            static_assert(std::is_nothrow_swappable_v<IDType> && (std::is_nothrow_swappable_v<Types> && ...));
            if (a_idx >= size() || b_idx >= size())
                return std::unexpected{error_code::out_of_range};
            [&]<size_t... Indices>(std::index_sequence<Indices...>) {
                (std::swap(_columns.template column<Indices>()[a_idx], _columns.template column<Indices>()[b_idx]), ...);
            }(std::make_index_sequence<sizeof...(Types) + 1>{});
            return {};
        }

        template<typename IDType, size_t MaxSize, typename... Types>
        std::expected<size_t, error_code> dynamic_storage<IDType, MaxSize, Types...>::index(id_type id) const {
            const auto ids = _columns.template column<0>();
            const auto found = std::find(ids.begin(), ids.end(), id);
            if (found == ids.end())
                return std::unexpected{error_code::not_found_id};
            return static_cast<size_t>(found - ids.begin());
        }

        template<typename IDType, size_t MaxSize, typename... Types>
        IDType dynamic_storage<IDType, MaxSize, Types...>::id_at(size_t index) const {
            return _columns.template column<0>()[index];
        }

        template<typename IDType, size_t MaxSize, typename... Types>
        template<size_t Index>
        auto dynamic_storage<IDType, MaxSize, Types...>::get() noexcept {
            return _columns.template column<Index + 1>();
        }

        template<typename IDType, size_t MaxSize, typename... Types>
        template<size_t Index>
        auto dynamic_storage<IDType, MaxSize, Types...>::get() const noexcept {
            return _columns.template column<Index + 1>();
        }

        template<typename IDType, size_t MaxSize = 1024, typename ...Types>
        struct static_storage {
            static constexpr size_t default_max_size = 1024;

            static constexpr size_t max_size = MaxSize;
            using id_type = IDType;

        private:
            size_t _end_index = 0;

        public:
            std::array<id_type, max_size> ids;
            typename static_entries_type<max_size, Types...>::result entries;

            size_t size() const;

            bool empty() const;

            void clear();

            std::expected<void, error_code> resize(size_t size);

            template<typename... Args>
            std::expected<void, error_code> push_back(id_type id, Args&&... args);

            void pop_back();

            std::expected<void, error_code> swap(size_t a_idx, size_t b_idx);

            std::expected<size_t, error_code> index(id_type id) const;
            id_type id_at(size_t index) const;

            template<size_t Index>
            auto get();

            template<size_t Index>
            auto get() const;
        };

        template<typename IDType, size_t MaxSize, typename... Types>
        auto static_storage<IDType, MaxSize, Types...>::size() const -> size_t {
            return _end_index;
        }

        template<typename IDType, size_t MaxSize, typename... Types>
        auto static_storage<IDType, MaxSize, Types...>::empty() const -> bool {
            return _end_index == 0;
        }

        template<typename IDType, size_t MaxSize, typename... Types>
        auto static_storage<IDType, MaxSize, Types...>::clear() -> void {
            _end_index = 0;
        }

        template<typename IDType, size_t MaxSize, typename... Types>
        auto static_storage<IDType, MaxSize, Types...>::resize(size_t size) -> std::expected<void, error_code> {
            if (max_size < size) {
                return std::unexpected{error_code::buffer_over};
            } else {
                this->_end_index = size;
                return {};
            }
        }

        template<typename IDType, size_t MaxSize, typename... Types>
        template<typename... Args>
        auto static_storage<IDType, MaxSize, Types...>::push_back(id_type id, Args&&... args) -> std::expected<void, error_code> {
            if (_end_index >= max_size)
                return std::unexpected{error_code::buffer_over};
            ids[_end_index] = id;
            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                ((std::get<Is>(entries)[_end_index] = std::forward<Args>(args)), ...);
            }(std::make_index_sequence<sizeof...(Types)>{});
            _end_index++;
            return {};
        }

        template<typename IDType, size_t MaxSize, typename... Types>
        auto static_storage<IDType, MaxSize, Types...>::pop_back() -> void {
            if (0 < _end_index)
                _end_index--;
        }

        template<typename IDType, size_t MaxSize, typename... Types>
        auto static_storage<IDType, MaxSize, Types...>::swap(size_t a_idx, size_t b_idx) -> std::expected<void, error_code> {
            if (a_idx >= size() || b_idx >= size())
                return std::unexpected<error_code>{error_code::buffer_over};
            std::swap(ids[a_idx], ids[b_idx]);
            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                (std::swap(std::get<Is>(entries)[a_idx], std::get<Is>(entries)[b_idx]), ...);
            }(std::make_index_sequence<sizeof...(Types)>{});
            return {};
        }

        template<typename IDType, size_t MaxSize, typename... Types>
        auto static_storage<IDType, MaxSize, Types...>::index(id_type id) const -> std::expected<size_t, error_code> {
            for (size_t i = 0; i < size(); i++) {
                if (ids[i] == id)
                    return i;
            }
            return std::unexpected{error_code::not_found_id};
        }

        template<typename IDType, size_t MaxSize, typename... Types>
        auto static_storage<IDType, MaxSize, Types...>::id_at(size_t index) const -> id_type {
            return ids[index];
        }

        template<typename IDType, size_t MaxSize, typename... Types>
        template<size_t Index>
        auto static_storage<IDType, MaxSize, Types...>::get() {
            auto& values = std::get<Index>(entries);
            return std::span{values.data(), size()};
        }

        template<typename IDType, size_t MaxSize, typename... Types>
        template<size_t Index>
        auto static_storage<IDType, MaxSize, Types...>::get() const {
            const auto& values = std::get<Index>(entries);
            return std::span{values.data(), size()};
        }
    }

// ==========================================================

    template<generator::is_generator IDGenerator = generator::iota,
            eventsystem::is_eventsystem EventSystem = eventsystem::dummy,
            template<typename, size_t, typename...> typename Storage  = storage::static_storage,
            size_t MaxSize = Storage<size_t, 1, int>::default_max_size,
            typename ...Types> requires storage::is_storage<Storage>
    struct buffer {
        using this_type = buffer<IDGenerator, EventSystem, Storage, MaxSize, Types...>;

        static constexpr size_t max_size = MaxSize;
        using id_type = typename IDGenerator::generate_type;
        using element_types = _meta::type_list<Types...>;

        struct entry;
        struct const_entry;
        struct iterator;
        struct const_iterator;

    private:
        [[no_unique_address]] IDGenerator _generator;
        [[no_unique_address]] EventSystem _eventsystem;

    public:
        Storage<id_type, max_size, Types...> storage;

        template<std::size_t N>
        [[nodiscard]] auto get();

        template<std::size_t N>
        [[nodiscard]] auto get() const;

        [[nodiscard]] std::size_t size() const;

        [[nodiscard]] bool empty() const;

        std::expected<size_t, error_code> index(id_type id) const;

        std::expected<entry, error_code> operator[](id_type id);

        std::expected<const_entry, error_code> operator[](id_type id) const;

        template<size_t N>
        std::expected<std::reference_wrapper<typename element_types::template get<N>>, error_code> get(id_type id);

        template<size_t N>
        std::expected<std::reference_wrapper<const typename element_types::template get<N>>, error_code> get(id_type id) const;

        template<typename... Args>
        std::expected<void, error_code> push_back(Args&&... args);

        void pop_back();

        void clear();

        std::expected<void, error_code> resize(size_t size);

        std::expected<void, error_code> erase(id_type id);

        iterator begin();

        iterator end();

        const_iterator begin() const;

        const_iterator end() const;

        const_iterator cbegin() const;

        const_iterator cend() const;

        struct entry {
            this_type *parent;
            size_t index;

            id_type id() const {
                return parent->storage.id_at(index);
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

        struct const_entry {
            const this_type *parent;
            size_t index;

            id_type id() const {
                return parent->storage.id_at(index);
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
            using pointer = entry *;
            using reference = entry;
        private:
            entry m_entry{};

        public:
            iterator() = default;

            iterator(this_type *parent, size_t index) : m_entry{parent, index} {};

            iterator(const iterator &) = default;

            iterator &operator=(const iterator &) = default;

            reference operator*() const {
                return m_entry;
            }

            const value_type* operator->() const {
                return &m_entry;
            }

            reference operator*() {
                return m_entry;
            }

            pointer operator->() {
                return &m_entry;
            }

            entry operator[](difference_type diff) const {
                return {m_entry.parent, m_entry.index + diff};
            }

            iterator &operator++() {
                m_entry.index++;
                return *this;
            }

            iterator operator++(int) {
                return {m_entry.parent, m_entry.index++};
            }

            iterator &operator--() {
                m_entry.index--;
                return *this;
            }

            iterator operator--(int) {
                return {m_entry.parent, m_entry.index--};
            }

            iterator &operator+=(difference_type diff) {
                m_entry.index += diff;
                return *this;
            }

            iterator &operator-=(difference_type diff) {
                m_entry.index -= diff;
                return *this;
            }

            explicit operator bool() const {
                return m_entry.parent && m_entry.index <= m_entry.parent->size();
            }

            bool operator!() const {
                return !static_cast<bool>(*this);
            }

            friend iterator operator+(const iterator &itr, difference_type diff) {
                return {itr.m_entry.parent, itr.m_entry.index + diff};
            }

            friend iterator operator+(difference_type diff, const iterator &itr) {
                return {itr.m_entry.parent, itr.m_entry.index + diff};
            }

            friend iterator operator-(const iterator &itr, difference_type diff) {
                return {itr.m_entry.parent, itr.m_entry.index - diff};
            }

            friend difference_type operator-(const iterator &l, const iterator &r) {
                return static_cast<difference_type>(l.m_entry.index) - static_cast<difference_type>(r.m_entry.index);
            }

            friend bool operator==(const iterator &l, const iterator &r) {
                return l.m_entry.parent == r.m_entry.parent && l.m_entry.index == r.m_entry.index;
            }

            friend bool operator!=(const iterator &l, const iterator &r) {
                return !(l == r);
            }

            friend bool operator<(const iterator &l, const iterator &r) {
                return l.m_entry.index < r.m_entry.index;
            }

            friend bool operator<=(const iterator &l, const iterator &r) {
                return l.m_entry.index <= r.m_entry.index;
            }

            friend bool operator>(const iterator &l, const iterator &r) {
                return l.m_entry.index > r.m_entry.index;
            }

            friend bool operator>=(const iterator& l, const iterator& r) {
                return l.m_entry.index >= r.m_entry.index;
            }
        };

        struct const_iterator {
            using iterator_category = std::random_access_iterator_tag;
            using value_type = const_entry;
            using difference_type = ptrdiff_t;
            using pointer = const_entry *;
            using reference = const_entry;

        private:
            const_entry m_entry{};

        public:
            const_iterator() = default;

            const_iterator(const this_type *parent, size_t index) : m_entry{parent, index} {};

            const_iterator(const const_iterator &) = default;

            const_iterator &operator=(const const_iterator &) = default;

            reference operator*() const {
                return m_entry;
            }

            const value_type* operator->() const {
                return &m_entry;
            }

            const_entry operator[](difference_type diff) const {
                return {m_entry.parent, m_entry.index + diff};
            }

            const_iterator &operator++() {
                m_entry.index++;
                return *this;
            }

            const_iterator operator++(int) {
                return {m_entry.parent, m_entry.index++};
            }

            const_iterator &operator--() {
                m_entry.index--;
                return *this;
            }

            const_iterator operator--(int) {
                return {m_entry.parent, m_entry.index--};
            }

            const_iterator &operator+=(difference_type diff) {
                m_entry.index += diff;
                return *this;
            }

            const_iterator &operator-=(difference_type diff) {
                m_entry.index -= diff;
                return *this;
            }

            explicit operator bool() const {
                return m_entry.parent && m_entry.index <= m_entry.parent->size();
            }

            bool operator!() const {
                return !static_cast<bool>(*this);
            }

            friend const_iterator operator+(const const_iterator &itr, difference_type diff) {
                return {itr.m_entry.parent, itr.m_entry.index + diff};
            }

            friend const_iterator operator+(difference_type diff, const const_iterator &itr) {
                return {itr.m_entry.parent, itr.m_entry.index + diff};
            }

            friend const_iterator operator-(const const_iterator &itr, difference_type diff) {
                return {itr.m_entry.parent, itr.m_entry.index - diff};
            }

            friend difference_type operator-(const const_iterator &l, const const_iterator &r) {
                return static_cast<difference_type>(l.m_entry.index) - static_cast<difference_type>(r.m_entry.index);
            }

            friend bool operator==(const const_iterator &l, const const_iterator &r) {
                return l.m_entry.parent == r.m_entry.parent && l.m_entry.index == r.m_entry.index;
            }

            friend bool operator!=(const const_iterator &l, const const_iterator &r) {
                return !(l == r);
            }

            friend bool operator<(const const_iterator &l, const const_iterator &r) {
                return l.m_entry.index < r.m_entry.index;
            }

            friend bool operator<=(const const_iterator &l, const const_iterator &r) {
                return l.m_entry.index <= r.m_entry.index;
            }

            friend bool operator>(const const_iterator &l, const const_iterator &r) {
                return l.m_entry.index > r.m_entry.index;
            }

            friend bool operator>=(const const_iterator& l, const const_iterator& r) {
                return l.m_entry.index >= r.m_entry.index;
            }
        };
    };

    template<generator::is_generator IDGenerator, eventsystem::is_eventsystem EventSystem,
             template<typename, size_t, typename...> typename Storage, size_t MaxSize, typename... Types>
    requires storage::is_storage<Storage>
    template<std::size_t N>
    [[nodiscard]] auto buffer<IDGenerator, EventSystem, Storage, MaxSize, Types...>::get() {
        return storage.template get<N>();
    }

    template<generator::is_generator IDGenerator, eventsystem::is_eventsystem EventSystem,
             template<typename, size_t, typename...> typename Storage, size_t MaxSize, typename... Types>
    requires storage::is_storage<Storage>
    template<std::size_t N>
    [[nodiscard]] auto buffer<IDGenerator, EventSystem, Storage, MaxSize, Types...>::get() const {
        return storage.template get<N>();
    }

    template<generator::is_generator IDGenerator, eventsystem::is_eventsystem EventSystem,
             template<typename, size_t, typename...> typename Storage, size_t MaxSize, typename... Types>
    requires storage::is_storage<Storage>
    [[nodiscard]] auto buffer<IDGenerator, EventSystem, Storage, MaxSize, Types...>::size() const -> std::size_t {
        return storage.size();
    }

    template<generator::is_generator IDGenerator, eventsystem::is_eventsystem EventSystem,
             template<typename, size_t, typename...> typename Storage, size_t MaxSize, typename... Types>
    requires storage::is_storage<Storage>
    [[nodiscard]] auto buffer<IDGenerator, EventSystem, Storage, MaxSize, Types...>::empty() const -> bool {
        return storage.empty();
    }

    template<generator::is_generator IDGenerator, eventsystem::is_eventsystem EventSystem,
             template<typename, size_t, typename...> typename Storage, size_t MaxSize, typename... Types>
    requires storage::is_storage<Storage>
    auto buffer<IDGenerator, EventSystem, Storage, MaxSize, Types...>::index(id_type id) const -> std::expected<size_t, error_code> {
        return storage.index(id);
    }

    template<generator::is_generator IDGenerator, eventsystem::is_eventsystem EventSystem,
             template<typename, size_t, typename...> typename Storage, size_t MaxSize, typename... Types>
    requires storage::is_storage<Storage>
    auto buffer<IDGenerator, EventSystem, Storage, MaxSize, Types...>::operator[](id_type id) -> std::expected<entry, error_code> {
        auto idx = index(id);
        if (not idx.has_value())
            return std::unexpected{idx.error()};
        return entry{this, idx.value()};
    }

    template<generator::is_generator IDGenerator, eventsystem::is_eventsystem EventSystem,
             template<typename, size_t, typename...> typename Storage, size_t MaxSize, typename... Types>
    requires storage::is_storage<Storage>
    auto buffer<IDGenerator, EventSystem, Storage, MaxSize, Types...>::operator[](id_type id) const -> std::expected<const_entry, error_code> {
        auto idx = index(id);
        if (not idx.has_value())
            return std::unexpected{idx.error()};
        return const_entry{this, idx.value()};
    }

    template<generator::is_generator IDGenerator, eventsystem::is_eventsystem EventSystem,
             template<typename, size_t, typename...> typename Storage, size_t MaxSize, typename... Types>
    requires storage::is_storage<Storage>
    template<size_t N>
    auto buffer<IDGenerator, EventSystem, Storage, MaxSize, Types...>::get(id_type id) -> std::expected<std::reference_wrapper<typename element_types::template get<N>>, error_code> {
        auto idx = index(id);
        if (not idx.has_value())
            return std::unexpected{idx.error()};
        return std::ref(get<N>()[idx.value()]);
    }

    template<generator::is_generator IDGenerator, eventsystem::is_eventsystem EventSystem,
             template<typename, size_t, typename...> typename Storage, size_t MaxSize, typename... Types>
    requires storage::is_storage<Storage>
    template<size_t N>
    auto buffer<IDGenerator, EventSystem, Storage, MaxSize, Types...>::get(id_type id) const -> std::expected<std::reference_wrapper<const typename element_types::template get<N>>, error_code> {
        auto idx = index(id);
        if (not idx.has_value())
            return std::unexpected{idx.error()};
        return std::ref(get<N>()[idx.value()]);
    }

    template<generator::is_generator IDGenerator, eventsystem::is_eventsystem EventSystem,
             template<typename, size_t, typename...> typename Storage, size_t MaxSize, typename... Types>
    requires storage::is_storage<Storage>
    template<typename... Args>
    auto buffer<IDGenerator, EventSystem, Storage, MaxSize, Types...>::push_back(Args&&... args) -> std::expected<void, error_code> {
        static_assert(std::same_as<EventSystem, eventsystem::dummy>, "Event dispatch is not implemented");
        if (size() >= max_size)
            return std::unexpected{error_code::buffer_over};
        auto id = _generator.next();
        if (!id)
            return std::unexpected{error_code::generate_id_failed};
        return storage.push_back(*id, std::forward<Args>(args)...);
    }

    template<generator::is_generator IDGenerator, eventsystem::is_eventsystem EventSystem,
             template<typename, size_t, typename...> typename Storage, size_t MaxSize, typename... Types>
    requires storage::is_storage<Storage>
    auto buffer<IDGenerator, EventSystem, Storage, MaxSize, Types...>::pop_back() -> void {
        storage.pop_back();
    }

    template<generator::is_generator IDGenerator, eventsystem::is_eventsystem EventSystem,
             template<typename, size_t, typename...> typename Storage, size_t MaxSize, typename... Types>
    requires storage::is_storage<Storage>
    auto buffer<IDGenerator, EventSystem, Storage, MaxSize, Types...>::clear() -> void {
        storage.clear();
    }

    template<generator::is_generator IDGenerator, eventsystem::is_eventsystem EventSystem,
             template<typename, size_t, typename...> typename Storage, size_t MaxSize, typename... Types>
    requires storage::is_storage<Storage>
    auto buffer<IDGenerator, EventSystem, Storage, MaxSize, Types...>::resize(size_t size) -> std::expected<void, error_code> {
        if (size > max_size)
            return std::unexpected{error_code::buffer_over};
        const auto old = this->size();
        try {
            while (this->size() < size) {
                const auto result = push_back(Types{}...);
                if (!result) {
                    while (this->size() > old)
                        pop_back();
                    return result;
                }
            }
        } catch (...) {
            while (this->size() > old)
                pop_back();
            throw;
        }
        while (this->size() > size)
            pop_back();
        return {};
    }

    template<generator::is_generator IDGenerator, eventsystem::is_eventsystem EventSystem,
             template<typename, size_t, typename...> typename Storage, size_t MaxSize, typename... Types>
    requires storage::is_storage<Storage>
    auto buffer<IDGenerator, EventSystem, Storage, MaxSize, Types...>::erase(id_type id) -> std::expected<void, error_code> {
        auto idx = index(id);
        if (!idx)
            return std::unexpected{idx.error()};
        const auto result = storage.swap(size() - 1, *idx);
        if (!result)
            return result;
        storage.pop_back();
        return {};
    }

    template<generator::is_generator IDGenerator, eventsystem::is_eventsystem EventSystem,
             template<typename, size_t, typename...> typename Storage, size_t MaxSize, typename... Types>
    requires storage::is_storage<Storage>
    auto buffer<IDGenerator, EventSystem, Storage, MaxSize, Types...>::begin() -> iterator {
        return iterator(this, 0);
    }

    template<generator::is_generator IDGenerator, eventsystem::is_eventsystem EventSystem,
             template<typename, size_t, typename...> typename Storage, size_t MaxSize, typename... Types>
    requires storage::is_storage<Storage>
    auto buffer<IDGenerator, EventSystem, Storage, MaxSize, Types...>::end() -> iterator {
        return iterator(this, size());
    }

    template<generator::is_generator IDGenerator, eventsystem::is_eventsystem EventSystem,
             template<typename, size_t, typename...> typename Storage, size_t MaxSize, typename... Types>
    requires storage::is_storage<Storage>
    auto buffer<IDGenerator, EventSystem, Storage, MaxSize, Types...>::begin() const -> const_iterator {
        return const_iterator(this, 0);
    }

    template<generator::is_generator IDGenerator, eventsystem::is_eventsystem EventSystem,
             template<typename, size_t, typename...> typename Storage, size_t MaxSize, typename... Types>
    requires storage::is_storage<Storage>
    auto buffer<IDGenerator, EventSystem, Storage, MaxSize, Types...>::end() const -> const_iterator {
        return const_iterator(this, size());
    }

    template<generator::is_generator IDGenerator, eventsystem::is_eventsystem EventSystem,
             template<typename, size_t, typename...> typename Storage, size_t MaxSize, typename... Types>
    requires storage::is_storage<Storage>
    auto buffer<IDGenerator, EventSystem, Storage, MaxSize, Types...>::cbegin() const -> const_iterator {
        return const_iterator(this, 0);
    }

    template<generator::is_generator IDGenerator, eventsystem::is_eventsystem EventSystem,
             template<typename, size_t, typename...> typename Storage, size_t MaxSize, typename... Types>
    requires storage::is_storage<Storage>
    auto buffer<IDGenerator, EventSystem, Storage, MaxSize, Types...>::cend() const -> const_iterator {
        return const_iterator(this, size());
    }

    /*
    template<generator::is_generator G, eventsystem::is_eventsystem E, size_t ChunkSize = 16, typename ...Types>
    struct chunked_buffer {
        static constexpr size_t chunk_size = ChunkSize;
        using id_type = typename G::generate_type;

        std::tuple<std::vector<buffer<G, E, storage::static_storage, ChunkSize, Types>>...> chunks;
    };
     */
}

#endif //SAITEKIKUUKAN_HPP
