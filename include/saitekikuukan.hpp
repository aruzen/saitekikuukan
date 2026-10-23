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
#include <algorithm>
#include <compare>
#include <concepts>
#include <coroutine>
#include <exception>
#include <expected>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <source_location>
#include <ranges>
#include <vector>
#include <tuple>
#include <type_traits>
#include <utility>

#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
#include <algorithm>
#include <array>
#include <compare>
#include <cstddef>
#include <iterator>
#include <limits>
#include <memory>
#include <meta>
#include <ranges>
#include <type_traits>
#include <utility>
#include <vector>

#ifndef SAITEKIKUUKAN_NAMESPACE
#define SAITEKIKUUKAN_NAMESPACE kk
#endif

namespace SAITEKIKUUKAN_NAMESPACE::storage {
    namespace reflection_detail {
        template<std::size_t Size>
        struct array_column {
            template<typename T>
            using type = std::array<T, Size>;
        };

        template<typename Record, template<typename> typename Column>
        struct columns_type {
            static_assert(std::is_aggregate_v<Record> && !std::is_union_v<Record>,
                          "SoA records must be non-union aggregates");

            struct type;

            consteval {
                auto context = std::meta::access_context::current();
                if (!std::meta::bases_of(^^Record, context).empty())
                    throw "SoA records must not have base classes";

                std::vector<std::meta::info> columns;
                for (auto member : std::meta::nonstatic_data_members_of(^^Record, context)) {
                    if (std::meta::is_bit_field(member))
                        throw "SoA records must not contain bit-fields";
                    if (std::meta::is_reference_type(std::meta::type_of(member)))
                        throw "SoA records must not contain reference members";

                    auto column = std::meta::substitute(^^Column, {std::meta::type_of(member)});
                    columns.push_back(std::meta::data_member_spec(column, {
                        .name = std::meta::identifier_of(member)
                    }));
                }
                std::meta::define_aggregate(^^type, columns);
            }
        };
    }

    // Owns the column containers directly; member names and declaration order
    // come from Record. The resulting aggregate also supports structured bindings.
    template<typename Record, template<typename> typename Column>
    using soa = typename reflection_detail::columns_type<Record, Column>::type;

    template<typename Record, std::size_t Size>
    using static_soa = soa<Record, reflection_detail::array_column<Size>::template type>;

    template<typename Record>
    using dynamic_soa = soa<Record, std::vector>;

    // Expansion statements visit each column with its actual type, including
    // const qualification. No tuple of references or runtime lookup is needed.
    template<typename Columns, typename Function>
    constexpr void for_each_column(Columns& columns, Function&& function) {
        template for (auto& column : columns) {
            function(column);
        }
    }

    namespace reflection_detail {
        template<bool Const>
        struct element_reference {
            template<typename Column>
            using type = decltype(std::declval<
                std::conditional_t<Const, const Column, Column>&>()[std::size_t{}]);
        };

        template<typename Columns>
        struct row_access {
            using columns_type = std::remove_const_t<Columns>;
            using reference = soa<columns_type,
                element_reference<std::is_const_v<Columns>>::template type>;

            static constexpr auto members = std::define_static_array(
                std::meta::nonstatic_data_members_of(^^columns_type,
                    std::meta::access_context::current()));

            static constexpr reference get(Columns& columns, std::size_t index) {
                return [&]<std::size_t... Indices>(std::index_sequence<Indices...>) {
                    return reference{columns.[:members[Indices]:][index]...};
                }(std::make_index_sequence<members.size()>{});
            }

            static constexpr std::size_t size(const Columns& columns) {
                if constexpr (members.empty()) {
                    return 0;
                } else {
                    auto count = std::numeric_limits<std::size_t>::max();
                    for_each_column(columns, [&](const auto& column) {
                        count = std::min(count, static_cast<std::size_t>(column.size()));
                    });
                    return count;
                }
            }
        };

        template<typename Columns>
        class row_iterator {
            template<typename> friend class row_iterator;

            Columns* m_columns = nullptr;
            std::ptrdiff_t m_index = 0;

        public:
            using iterator_concept = std::random_access_iterator_tag;
            // Dereference returns a proxy value; C++20 ranges use iterator_concept.
            using iterator_category = std::input_iterator_tag;
            using difference_type = std::ptrdiff_t;
            using reference = typename row_access<Columns>::reference;
            using value_type = reference;

            struct pointer {
                reference row;

                constexpr const reference* operator->() const {
                    return std::addressof(row);
                }
            };

            constexpr row_iterator() = default;
            constexpr row_iterator(Columns* columns, difference_type index)
                : m_columns(columns), m_index(index) {}

            template<typename Other> requires std::is_convertible_v<Other*, Columns*>
            constexpr row_iterator(const row_iterator<Other>& other)
                : m_columns(other.m_columns), m_index(other.m_index) {}

            constexpr reference operator*() const {
                return row_access<Columns>::get(*m_columns, static_cast<std::size_t>(m_index));
            }

            constexpr pointer operator->() const { return {**this}; }
            constexpr reference operator[](difference_type offset) const { return *(*this + offset); }

            constexpr row_iterator& operator++() { ++m_index; return *this; }
            constexpr row_iterator operator++(int) { auto old = *this; ++*this; return old; }
            constexpr row_iterator& operator--() { --m_index; return *this; }
            constexpr row_iterator operator--(int) { auto old = *this; --*this; return old; }
            constexpr row_iterator& operator+=(difference_type offset) { m_index += offset; return *this; }
            constexpr row_iterator& operator-=(difference_type offset) { m_index -= offset; return *this; }

            friend constexpr row_iterator operator+(row_iterator it, difference_type offset) {
                return it += offset;
            }
            friend constexpr row_iterator operator+(difference_type offset, row_iterator it) {
                return it += offset;
            }
            friend constexpr row_iterator operator-(row_iterator it, difference_type offset) {
                return it -= offset;
            }
            friend constexpr difference_type operator-(row_iterator left, row_iterator right) {
                return left.m_index - right.m_index;
            }
            friend constexpr bool operator==(const row_iterator&, const row_iterator&) = default;
            friend constexpr auto operator<=>(const row_iterator&, const row_iterator&) = default;
        };

        template<typename Columns>
        class rows_view : public std::ranges::view_interface<rows_view<Columns>> {
            Columns* m_columns = nullptr;
            std::size_t m_size = 0;

        public:
            using iterator = row_iterator<Columns>;

            constexpr rows_view() = default;
            constexpr explicit rows_view(Columns& columns)
                : m_columns(std::addressof(columns)), m_size(row_access<Columns>::size(columns)) {}

            constexpr iterator begin() const { return {m_columns, 0}; }
            constexpr iterator end() const { return {m_columns, static_cast<std::ptrdiff_t>(m_size)}; }
            constexpr std::size_t size() const { return m_size; }
        };
    }

    // Each row is an aggregate of column element references with the same names.
    // The shortest column determines the row count; an empty schema has no rows.
    template<typename Columns>
    [[nodiscard]] constexpr auto rows(Columns& columns) {
        return reflection_detail::rows_view<Columns>(columns);
    }

    template<typename Columns>
    void rows(const Columns&&) = delete;
}

namespace std::ranges {
    template<typename Columns>
    inline constexpr bool enable_borrowed_range<
        ::SAITEKIKUUKAN_NAMESPACE::storage::reflection_detail::rows_view<Columns>> = true;
}
#endif

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
        generate_id_failed,
        operation_canceled
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
                std::get<std::vector<listener_interface<Event>*>>(listeners).push_back(listener);
                return listener;
            }

            template<typename Event>
            void remove(listener_interface<Event> *listener) {
                std::erase(std::get<std::vector<listener_interface<Event>*>>(listeners), listener);
            }

            template<typename Event>
            void on(Event &e) {
                for (auto* listener: std::get<std::vector<listener_interface<Event>*>>(listeners))
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
                std::get<listener_interface<Event>*>(listeners) = listener;
                return listener;
            }

            template<typename Event>
            void remove() {
                std::get<listener_interface<Event>*>(listeners) = nullptr;
            }

            template<typename Event>
            void on(Event &e) {
                if (auto* listener = std::get<listener_interface<Event>*>(listeners))
                    listener->on(e);
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
                if constexpr (Idx < listener_types::size) {
                    using Listener = listener_types::template get<Idx>;
                    if constexpr (std::same_as<typename Listener::listen_event, Event>)
                        Listener().on(e);
                    apply<Event, Idx + 1>(e);
                }
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

    // A record schema is explicit, so a struct in a legacy type pack remains
    // one component rather than being flattened implicitly.
    template<typename Record>
    struct record {};

    namespace _detail {
        struct unnamed_fields {};

        template<typename T>
        inline constexpr bool is_record = false;

        template<typename T>
        inline constexpr bool is_record<record<T>> = true;

        template<typename... Types>
        struct schema {
            static_assert((!is_record<Types> && ...), "record<T> must be the only schema argument");
            static constexpr bool named = false;
            using element_types = type_list<Types...>;
            using record_type = void;

            template<template<typename> typename Column>
            using columns = std::tuple<Column<Types>...>;

            template<std::size_t N, typename Columns>
            static constexpr decltype(auto) column(Columns& columns) {
                return std::get<N>(columns);
            }

            template<typename Columns>
            using fields = unnamed_fields;

            template<typename Columns>
            static constexpr fields<Columns> row(Columns&, std::size_t) { return {}; }
        };

#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
        template<typename Record>
        consteval auto record_element_types() {
            std::vector<std::meta::info> types;
            for (auto member : std::meta::nonstatic_data_members_of(^^Record,
                    std::meta::access_context::current()))
                types.push_back(std::meta::type_of(member));
            return std::meta::substitute(^^_meta::type_list, types);
        }

        template<typename Record>
        struct schema<record<Record>> {
            static constexpr bool named = true;
            using record_type = Record;
            using element_types = [:record_element_types<Record>():];
            static constexpr auto members = std::define_static_array(
                std::meta::nonstatic_data_members_of(^^Record, std::meta::access_context::current()));

            consteval {
                for (auto member : members) {
                    auto name = std::meta::identifier_of(member);
                    if (name == "id" || name == "get" || name == "parent" || name == "index")
                        throw "Record member name conflicts with buffer entry API: id, get, parent, index";
                }
            }

            template<template<typename> typename Column>
            using columns = storage::soa<Record, Column>;

            template<std::size_t N, typename Columns>
            static constexpr decltype(auto) column(Columns& columns) {
                using access = storage::reflection_detail::row_access<Columns>;
                static_assert(N < access::members.size());
                return (columns.[:access::members[N]:]);
            }

            template<typename Columns>
            using fields = typename storage::reflection_detail::row_access<Columns>::reference;

            template<typename Columns>
            static constexpr fields<Columns> row(Columns& columns, std::size_t index) {
                return storage::reflection_detail::row_access<Columns>::get(columns, index);
            }

            template<typename Value, typename Function>
            static constexpr decltype(auto) apply_record(Value&& value, Function&& function) {
                return [&]<std::size_t... Indices>(std::index_sequence<Indices...>) -> decltype(auto) {
                    return std::invoke(std::forward<Function>(function),
                        std::forward<Value>(value).[:members[Indices]:]...);
                }(std::make_index_sequence<members.size()>{});
            }
        };
#endif

        template<typename Reference>
        using lookup_value = std::conditional_t<std::is_lvalue_reference_v<Reference>,
            std::reference_wrapper<std::remove_reference_t<Reference>>, Reference>;

        template<typename Value>
        constexpr auto lookup_reference(Value&& value) {
            if constexpr (std::is_lvalue_reference_v<Value>)
                return std::ref(value);
            else
                return std::forward<Value>(value);
        }
    }

    namespace storage {
        template<template<typename, size_t, typename...> typename T>
        concept is_storage = requires(T<size_t, 1, int> t, const T<size_t, 1, int> tc) {
            T<size_t, 1, int>::default_max_size;
            T<size_t, 1, int>::max_size;
            typename T<size_t, 1, int>::id_type;
            t.ids;
            t.entries;

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
            using result = typename _detail::schema<Types...>::template columns<std::vector>;
        };

        template<size_t Size, typename ...Types>
        struct static_entries_type {
            template<typename T>
            using array = std::array<T, Size>;
            using result = typename _detail::schema<Types...>::template columns<array>;
        };

        constexpr size_t DynamicSizeUnlimited = std::numeric_limits<size_t>::max();

        template<typename IDType, size_t MaxSize = DynamicSizeUnlimited, typename ...Types>
        struct dynamic_storage {
            static constexpr size_t default_max_size = DynamicSizeUnlimited;

            static constexpr size_t max_size = MaxSize;
            using id_type = IDType;
            using schema_type = _detail::schema<Types...>;
            using element_types = typename schema_type::element_types;

            std::vector<id_type> ids;
            typename dynamic_entries_type<Types...>::result entries;

            template<std::size_t N>
            decltype(auto) get() { return schema_type::template column<N>(entries); }

            template<std::size_t N>
            decltype(auto) get() const { return schema_type::template column<N>(entries); }

            size_t size() const {
                return ids.size();
            }

            bool empty() const {
                return ids.empty();
            }

            void clear() {
                ids.clear();
                [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                    (get<Is>().clear(), ...);
                }(std::make_index_sequence<element_types::size>{});
            }

            std::expected<void, error_code> resize(size_t size) {
                if (size > max_size)
                    return std::unexpected{error_code::buffer_over};
                auto old_size = this->size();
                try {
                    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                        (get<Is>().resize(size), ...);
                    }(std::make_index_sequence<element_types::size>{});
                    ids.resize(size);
                } catch (...) {
                    rollback(old_size);
                    throw;
                }
                return {};
            }

            template<typename ...Args>
            std::expected<void, error_code> push_back(id_type id, Args &&... args) {
                static_assert(sizeof...(Args) == element_types::size);
                if (size() >= max_size)
                    return std::unexpected{error_code::buffer_over};
                auto old_size = size();
                try {
                    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                        (get<Is>().push_back(std::forward<Args>(args)), ...);
                    }(std::make_index_sequence<element_types::size>{});
                    ids.push_back(id);
                } catch (...) {
                    rollback(old_size);
                    throw;
                }
                return {};
            }

            void pop_back() {
                if (empty())
                    return;
                ids.pop_back();
                [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                    (get<Is>().pop_back(), ...);
                }(std::make_index_sequence<element_types::size>{});
            }

            std::expected<void, error_code> swap(size_t a_idx, size_t b_idx) {
                if (a_idx >= size() || b_idx >= size())
                    return std::unexpected<error_code>{error_code::buffer_over};
                std::swap(ids[a_idx], ids[b_idx]);
                [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                    (std::swap(get<Is>()[a_idx], get<Is>()[b_idx]), ...);
                }(std::make_index_sequence<element_types::size>{});
                return {};
            }

            std::expected<size_t, error_code> index(id_type id) const {
                for (size_t i = 0; i < size(); i++) {
                    if (ids[i] == id)
                        return i;
                }
                return std::unexpected{error_code::not_found_id};
            }

        private:
            void rollback(size_t size) {
                while (ids.size() > size)
                    ids.pop_back();
                [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                    ([&] { while (get<Is>().size() > size) get<Is>().pop_back(); }(), ...);
                }(std::make_index_sequence<element_types::size>{});
            }
        };

        template<typename IDType, size_t MaxSize = 1024, typename ...Types>
        struct static_storage {
            static constexpr size_t default_max_size = 1024;

            static constexpr size_t max_size = MaxSize;
            using id_type = IDType;
            using schema_type = _detail::schema<Types...>;
            using element_types = typename schema_type::element_types;

        private:
            size_t m_end_index = 0;
        public:
            std::array<id_type, max_size> ids;
            typename static_entries_type<max_size, Types...>::result entries;

            template<std::size_t N>
            decltype(auto) get() { return schema_type::template column<N>(entries); }

            template<std::size_t N>
            decltype(auto) get() const { return schema_type::template column<N>(entries); }

            size_t size() const {
                return m_end_index;
            }

            bool empty() const {
                return m_end_index == 0;
            }

            void clear() {
                m_end_index = 0;
            }

            std::expected<void, error_code> resize(size_t size) {
                if (max_size < size) {
                    return std::unexpected{error_code::buffer_over};
                } else {
                    this->m_end_index = size;
                    return {};
                }
            }

            template<typename ...Args>
            std::expected<void, error_code> push_back(id_type id, Args &&... args) {
                static_assert(sizeof...(Args) == element_types::size);
                if (size() >= max_size)
                    return std::unexpected{error_code::buffer_over};
                [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                    ((get<Is>()[m_end_index] = std::forward<Args>(args)), ...);
                }(std::make_index_sequence<element_types::size>{});
                ids[m_end_index] = id;
                m_end_index++;
                return {};
            }

            void pop_back() {
                if (0 < m_end_index)
                    m_end_index--;
            }

            std::expected<void, error_code> swap(size_t a_idx, size_t b_idx) {
                if (a_idx >= size() || b_idx >= size())
                    return std::unexpected<error_code>{error_code::buffer_over};
                std::swap(ids[a_idx], ids[b_idx]);
                [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                    (std::swap(get<Is>()[a_idx], get<Is>()[b_idx]), ...);
                }(std::make_index_sequence<element_types::size>{});
                return {};
            }

            std::expected<size_t, error_code> index(id_type id) const {
                for (size_t i = 0; i < size(); i++) {
                    if (ids[i] == id)
                        return i;
                }
                return std::unexpected{error_code::not_found_id};
            }
        };
    }

// ==========================================================

    template<generator::is_generator IDGenerator = generator::iota,
            eventsystem::is_eventsystem EventSystem = eventsystem::dummy,
            template<typename, size_t, typename...> typename Storage = storage::static_storage,
            size_t MaxSize = Storage<size_t, 1, int>::default_max_size,
            typename... Types> requires storage::is_storage<Storage>
    struct buffer {
        using this_type = buffer<IDGenerator, EventSystem, Storage, MaxSize, Types...>;
        using schema_type = _detail::schema<Types...>;
        using element_types = typename schema_type::element_types;
        using id_type = typename IDGenerator::generate_type;
        using storage_type = Storage<id_type, MaxSize, Types...>;
        using columns_type = decltype(std::declval<storage_type&>().entries);
        static constexpr size_t max_size = MaxSize;

        template<bool Const>
        using parent_type = std::conditional_t<Const, const this_type, this_type>;

        template<bool Const>
        using fields_type = typename schema_type::template fields<
            std::conditional_t<Const, const columns_type, columns_type>>;

        template<bool Const>
        struct basic_entry : fields_type<Const> {
            parent_type<Const>* parent;
            size_t index;

            basic_entry(parent_type<Const>* parent, size_t index)
                : fields_type<Const>(schema_type::row(parent->storage.entries, index)),
                  parent(parent), index(index) {}

            id_type id() const { return parent->storage.ids[index]; }

            template<size_t N>
            decltype(auto) get() { return parent->template get<N>()[index]; }

            template<size_t N>
            decltype(auto) get() const { return std::as_const(*parent).template get<N>()[index]; }
        };

        using entry = basic_entry<false>;
        using const_entry = basic_entry<true>;

        template<bool Const>
        class basic_iterator {
            template<bool> friend class basic_iterator;
            parent_type<Const>* m_parent = nullptr;
            ptrdiff_t m_index = 0;

        public:
            using iterator_concept = std::random_access_iterator_tag;
            using iterator_category = std::conditional_t<schema_type::named,
                std::input_iterator_tag, std::random_access_iterator_tag>;
            using value_type = basic_entry<Const>;
            using difference_type = ptrdiff_t;
            using reference = value_type;

            struct pointer {
                reference row;
                reference* operator->() { return std::addressof(row); }
                const reference* operator->() const { return std::addressof(row); }
            };

            basic_iterator() = default;
            basic_iterator(parent_type<Const>* parent, difference_type index)
                : m_parent(parent), m_index(index) {}

            template<bool Other> requires (Const && !Other)
            basic_iterator(const basic_iterator<Other>& other)
                : m_parent(other.m_parent), m_index(other.m_index) {}

            reference operator*() const { return {m_parent, static_cast<size_t>(m_index)}; }
            pointer operator->() const { return {**this}; }
            reference operator[](difference_type offset) const { return *(*this + offset); }
            basic_iterator& operator++() { ++m_index; return *this; }
            basic_iterator operator++(int) { auto old = *this; ++*this; return old; }
            basic_iterator& operator--() { --m_index; return *this; }
            basic_iterator operator--(int) { auto old = *this; --*this; return old; }
            basic_iterator& operator+=(difference_type offset) { m_index += offset; return *this; }
            basic_iterator& operator-=(difference_type offset) { m_index -= offset; return *this; }

            explicit operator bool() const {
                return m_parent && m_index >= 0 && static_cast<size_t>(m_index) <= m_parent->size();
            }
            bool operator!() const { return !static_cast<bool>(*this); }

            friend basic_iterator operator+(basic_iterator it, difference_type offset) { return it += offset; }
            friend basic_iterator operator+(difference_type offset, basic_iterator it) { return it += offset; }
            friend basic_iterator operator-(basic_iterator it, difference_type offset) { return it -= offset; }
            friend difference_type operator-(basic_iterator left, basic_iterator right) {
                return left.m_index - right.m_index;
            }
            friend bool operator==(const basic_iterator&, const basic_iterator&) = default;
            friend auto operator<=>(const basic_iterator&, const basic_iterator&) = default;
        };

        using iterator = basic_iterator<false>;
        using const_iterator = basic_iterator<true>;

    private:
        [[no_unique_address]] IDGenerator m_generator;
        [[no_unique_address]] EventSystem m_eventsystem;
        using event_types = typename EventSystem::event_types;
        using data_tuple = typename element_types::template apply<std::tuple>;

        template<typename... Fields>
        using will_insert = eventsystem::event::will_insert_event<id_type, Fields...>;
        template<typename... Fields>
        using did_insert = eventsystem::event::did_insert_event<id_type, Fields...>;
        using will_insert_type = typename element_types::template apply<will_insert>;
        using did_insert_type = typename element_types::template apply<did_insert>;

        auto row_references(size_t index) {
            return [&]<size_t... Indices>(std::index_sequence<Indices...>) {
                return std::tie(get<Indices>()[index]...);
            }(std::make_index_sequence<element_types::size>{});
        }

        template<typename... Args>
        std::expected<void, error_code> insert_columns(Args&&... args) {
            static_assert(sizeof...(Args) == element_types::size, "Incorrect number of column values");
            if (size() >= max_size)
                return std::unexpected{error_code::buffer_over};
            std::optional<id_type> id = m_generator.next();
            if (!id)
                return std::unexpected{error_code::generate_id_failed};

            constexpr bool observes_will = event_types::template contains<will_insert_type>;
            constexpr bool observes_did = event_types::template contains<did_insert_type>;
            if constexpr (!observes_will && !observes_did) {
                return storage.push_back(*id, std::forward<Args>(args)...);
            } else {
                data_tuple data(std::forward<Args>(args)...);
                if constexpr (observes_will) {
                    auto references = std::apply([](auto&... values) { return std::tie(values...); }, data);
                    will_insert_type event{*id, references};
                    m_eventsystem.on(event);
                    if (event.canceled())
                        return std::unexpected{error_code::operation_canceled};
                }
                auto result = std::apply([&](auto&&... values) {
                    return storage.push_back(*id, std::forward<decltype(values)>(values)...);
                }, std::move(data));
                if (!result)
                    return result;
                if constexpr (observes_did) {
                    did_insert_type event{*id, row_references(size() - 1)};
                    m_eventsystem.on(event);
                }
                return {};
            }
        }

    public:
        storage_type storage;

        template<size_t N>
        [[nodiscard]] decltype(auto) get() { return schema_type::template column<N>(storage.entries); }

        template<size_t N>
        [[nodiscard]] decltype(auto) get() const { return schema_type::template column<N>(storage.entries); }

        [[nodiscard]] size_t size() const { return storage.size(); }
        [[nodiscard]] bool empty() const { return storage.empty(); }
        EventSystem& events() { return m_eventsystem; }
        const EventSystem& events() const { return m_eventsystem; }

        std::expected<size_t, error_code> index(id_type id) const { return storage.index(id); }

        std::expected<entry, error_code> operator[](id_type id) {
            auto index = this->index(id);
            if (!index)
                return std::unexpected{index.error()};
            return entry{this, *index};
        }

        std::expected<const_entry, error_code> operator[](id_type id) const {
            auto index = this->index(id);
            if (!index)
                return std::unexpected{index.error()};
            return const_entry{this, *index};
        }

        template<size_t N>
        using element_reference = decltype(schema_type::template column<N>(
            std::declval<columns_type&>())[size_t{}]);
        template<size_t N>
        using const_element_reference = decltype(schema_type::template column<N>(
            std::declval<const columns_type&>())[size_t{}]);

        template<size_t N>
        std::expected<_detail::lookup_value<element_reference<N>>, error_code> get(id_type id) {
            auto index = this->index(id);
            if (!index)
                return std::unexpected{index.error()};
            return _detail::lookup_reference(get<N>()[*index]);
        }

        template<size_t N>
        std::expected<_detail::lookup_value<const_element_reference<N>>, error_code> get(id_type id) const {
            auto index = this->index(id);
            if (!index)
                return std::unexpected{index.error()};
            return _detail::lookup_reference(get<N>()[*index]);
        }

        template<typename... Args>
        std::expected<void, error_code> push_back(Args&&... args) {
            if constexpr (schema_type::named && sizeof...(Args) == 1) {
                using argument = std::tuple_element_t<0, std::tuple<Args...>>;
                if constexpr (std::same_as<std::remove_cvref_t<argument>, typename schema_type::record_type>) {
                    return schema_type::apply_record(std::forward<Args>(args)..., [&](auto&&... values) {
                        return insert_columns(std::forward<decltype(values)>(values)...);
                    });
                } else {
                    return insert_columns(std::forward<Args>(args)...);
                }
            } else {
                return insert_columns(std::forward<Args>(args)...);
            }
        }

        void pop_back() { storage.pop_back(); }
        void clear() { storage.clear(); }

        std::expected<void, error_code> resize(size_t size) {
            if (size > max_size)
                return std::unexpected{error_code::buffer_over};
            auto old_size = this->size();
            if constexpr (requires { std::tuple_size<decltype(storage.ids)>::value; }) {
                // Fixed storage already owns inactive ID slots. Populate them
                // before committing the new size, without allocating a staging vector.
                for (size_t i = old_size; i < size; ++i) {
                    std::optional<id_type> id = m_generator.next();
                    if (!id)
                        return std::unexpected{error_code::generate_id_failed};
                    storage.ids[i] = *id;
                }
                return storage.resize(size);
            }
            std::vector<id_type> new_ids;
            if (size > old_size) {
                new_ids.reserve(size - old_size);
                for (size_t i = old_size; i < size; ++i) {
                    std::optional<id_type> id = m_generator.next();
                    if (!id)
                        return std::unexpected{error_code::generate_id_failed};
                    new_ids.push_back(*id);
                }
            }
            auto result = storage.resize(size);
            if (!result)
                return result;
            for (size_t i = 0; i < new_ids.size(); ++i)
                storage.ids[old_size + i] = new_ids[i];
            return {};
        }

        std::expected<void, error_code> erase(id_type id) {
            auto index = this->index(id);
            if (!index)
                return std::unexpected{index.error()};
            if (*index != size() - 1) {
                auto result = storage.swap(*index, size() - 1);
                if (!result)
                    return result;
            }
            storage.pop_back();
            return {};
        }

        iterator begin() { return {this, 0}; }
        iterator end() { return {this, static_cast<ptrdiff_t>(size())}; }
        const_iterator begin() const { return {this, 0}; }
        const_iterator end() const { return {this, static_cast<ptrdiff_t>(size())}; }
        const_iterator cbegin() const { return begin(); }
        const_iterator cend() const { return end(); }
    };

#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
    template<typename Record,
            generator::is_generator IDGenerator = generator::iota,
            eventsystem::is_eventsystem EventSystem = eventsystem::dummy,
            template<typename, size_t, typename...> typename Storage = storage::static_storage,
            size_t MaxSize = Storage<size_t, 1, int>::default_max_size>
    using record_buffer = buffer<IDGenerator, EventSystem, Storage, MaxSize, record<Record>>;
#endif


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
