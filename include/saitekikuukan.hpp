// 2025/08/16 morimoto_hibiki 作成。
#pragma once

#ifndef SAITEKIKUUKAN_HPP
#define SAITEKIKUUKAN_HPP

#ifndef SAITEKIKUUKAN_NAMESPACE
#define SAITEKIKUUKAN_NAMESPACE kk
#endif

// UUID ジェネレータを利用する場合に定義する。
// #define HAS_BOOST_UUID

#include <algorithm>
#include <array>
#include <compare>
#include <concepts>
#include <coroutine>
#include <cstddef>
#include <exception>
#include <expected>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <ranges>
#include <source_location>
#include <span>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
#include <meta>
#endif

namespace SAITEKIKUUKAN_NAMESPACE {
    // ================================= _meta =================================
    // 型リストとメタ関数
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
                using combined = typename L1::template append<L2>;
                if constexpr (sizeof...(Types) == 0)
                    return combined{};
                else
                    return _concat<combined, Types...>();
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
                } else if constexpr (has_bool_value_type<Filter<T>>) {
                    return static_cast<bool>(Filter<T>::value);
                } else {
                    static_assert(
                        has_bool_value_type<Filter<T>>,
                        "Invalid Filter<T> type: must provide either '::value', '::type' or same_as<std::true_type>");
                }
                return false;
            }

            template<template<typename...> typename Filter, typename N>
            auto _filter() { return N{}; }

            template<template<typename...> typename Filter, typename N, typename T,
                     typename ...Types>
            auto _filter() {
                using next =
                    std::conditional_t<_apply_filter<Filter, T>(), typename N::template add<T>, N>;
                return _filter<Filter, next, Types...>();
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

            template<template<typename...> typename Filter, size_t Current, size_t Size,
                     std::array<size_t, Size> Arr>
            constexpr auto _filtered_index() { return Arr; }

            template<template<typename...> typename Filter, size_t Current, size_t Size,
                     std::array<size_t, Size> Arr, typename T, typename ...Types>
            constexpr auto _filtered_index() {
                if constexpr (_apply_filter<Filter, T>())
                    return _filtered_index<Filter, Current + 1, Size + 1,
                        _array_push_back<Current, Size>(Arr), Types...>();
                else
                    return _filtered_index<Filter, Current + 1, Size, Arr, Types...>();
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
            static constexpr auto filtered_index() {
                return helper::_filtered_index<Filter, 0, 0, std::array<size_t, 0>{}, Types...>();
            }

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
             * Start は識別子の開始値。テンプレート引数として型の生成を遅延させる。
             */
            template<size_t Start = 0>
            using wrap_meta_id = decltype(helper::_append_id<Start, type_list<>, Types...>());

            template<size_t Size>
            using wrap_array = type_list<std::array<Types, Size>...>;
        };
    }

    using _meta::type_list;

    // =============================== error_code ==============================
    // 操作結果を表すエラー
    enum class error_code {
        out_of_range,
        not_found_id,
        buffer_over,
        generate_id_failed,
        operation_canceled,
        duplicate_id
    };

    // =============================== generator ===============================
    // ID の生成方針
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

    // ============================== eventsystem ==============================
    // 操作前後のイベントとリスナ
    namespace eventsystem {
        // will_* は操作前、did_* は操作完了後の通知を表す。
        namespace event {
            class cancellable {
                bool m_canceled = false;

            public:
                bool canceled() const { return m_canceled; }

                void cancel() { m_canceled = true; }
            };

            template<typename... Types>
            struct will_insert_row_event : cancellable {
                const size_t index;
                std::tuple<Types&...> data;

                will_insert_row_event(size_t index, std::tuple<Types&...> data)
                    : index(index), data(data) {}
            };

            template<typename... Types>
            struct did_insert_row_event {
                const size_t index;
                const std::tuple<Types&...> data;

                did_insert_row_event(size_t index, std::tuple<Types&...> data)
                    : index(index), data(data) {}
            };

            template<typename ID, typename... Types>
            struct will_insert_event : cancellable {
                ID& id;
                std::tuple<Types&...> data;

                will_insert_event(ID& id, std::tuple<Types&...> data) : id(id), data(data) {}
            };

            template<typename ID, typename... Types>
            struct did_insert_event {
                const ID id;
                const std::tuple<Types&...> data;

                did_insert_event(ID id, std::tuple<Types&...> data)
                    : id(std::move(id)), data(data) {}
            };

            // 削除前は読み取り専用の行を通知し、削除後は破棄済み要素を参照しない。
            template<typename... Types>
            struct will_delete_row_event : cancellable {
                const size_t index;
                const std::tuple<const Types&...> data;

                will_delete_row_event(size_t index, std::tuple<const Types&...> data)
                    : index(index), data(data) {}
            };

            template<typename... Types>
            struct did_delete_row_event {
                const size_t index;

                explicit did_delete_row_event(size_t index) : index(index) {}
            };

            template<typename ID, typename... Types>
            struct will_delete_event : will_delete_row_event<Types...> {
                const ID& id;

                will_delete_event(const ID& id, size_t index, std::tuple<const Types&...> data)
                    : will_delete_row_event<Types...>(index, data), id(id) {}
            };

            template<typename ID, typename... Types>
            struct did_delete_event : did_delete_row_event<Types...> {
                const ID id;

                did_delete_event(ID id, size_t index)
                    : did_delete_row_event<Types...>(index), id(std::move(id)) {}
            };

            template<typename... Types>
            struct will_swap_row_event : cancellable {
                const size_t first_index, second_index;
                const std::tuple<const Types&...> first, second;

                will_swap_row_event(size_t first_index, size_t second_index,
                    std::tuple<const Types&...> first, std::tuple<const Types&...> second)
                    : first_index(first_index), second_index(second_index), first(first),
                      second(second) {}
            };

            template<typename... Types>
            struct did_swap_row_event {
                const size_t first_index, second_index;
                const std::tuple<Types&...> first, second;

                did_swap_row_event(size_t first_index, size_t second_index,
                    std::tuple<Types&...> first, std::tuple<Types&...> second)
                    : first_index(first_index), second_index(second_index), first(first),
                      second(second) {}
            };

            template<typename ID, typename... Types>
            struct will_swap_event : will_swap_row_event<Types...> {
                const ID& first_id;
                const ID& second_id;

                will_swap_event(const ID& first_id, const ID& second_id, size_t first_index,
                    size_t second_index, std::tuple<const Types&...> first,
                    std::tuple<const Types&...> second)
                    : will_swap_row_event<Types...>(first_index, second_index, first, second),
                      first_id(first_id), second_id(second_id) {}
            };

            template<typename ID, typename... Types>
            struct did_swap_event : did_swap_row_event<Types...> {
                const ID& first_id;
                const ID& second_id;

                did_swap_event(const ID& first_id, const ID& second_id, size_t first_index,
                    size_t second_index, std::tuple<Types&...> first, std::tuple<Types&...> second)
                    : did_swap_row_event<Types...>(first_index, second_index, first, second),
                      first_id(first_id), second_id(second_id) {}
            };

            // 更新前の data は代入予定の値。更新後の data は格納済みの行を参照する。
            template<typename... Types>
            struct will_update_row_event : cancellable {
                const size_t index;
                std::tuple<Types&...> data;

                will_update_row_event(size_t index, std::tuple<Types&...> data)
                    : index(index), data(data) {}
            };

            template<typename... Types>
            struct did_update_row_event {
                const size_t index;
                const std::tuple<Types&...> data;

                did_update_row_event(size_t index, std::tuple<Types&...> data)
                    : index(index), data(data) {}
            };

            template<typename ID, typename... Types>
            struct will_update_event : will_update_row_event<Types...> {
                const ID& id;

                will_update_event(const ID& id, size_t index, std::tuple<Types&...> data)
                    : will_update_row_event<Types...>(index, data), id(id) {}
            };

            template<typename ID, typename... Types>
            struct did_update_event : did_update_row_event<Types...> {
                const ID& id;

                did_update_event(const ID& id, size_t index, std::tuple<Types&...> data)
                    : did_update_row_event<Types...>(index, data), id(id) {}
            };
        }

        template<typename Event>
        struct listener_tag {
            using listen_event = Event;

            void on(Event&) {};
        };

        template<typename Event>
        struct listener_interface : public listener_tag<Event> {
            virtual void on(Event&) = 0;
        };

        template<typename T, typename E>
        concept eventsystem_for = requires(T t, E& e) {
            { t.on(e) };
        };

        template<typename T>
        concept is_eventsystem =
            requires { typename T::event_types; } && []<typename... Ev>(type_list<Ev...>*) {
                return (eventsystem_for<T, Ev> && ...);
            }(static_cast<typename T::event_types*>(nullptr));

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

        template<typename... Events>
        struct registration {
            using event_types = type_list<Events...>;

        private:
            std::tuple<std::vector<listener_interface<Events>*>...> listeners;

        public:
            template<typename Event>
            listener_interface<Event>* add(listener_interface<Event>* listener) {
                std::get<std::vector<listener_interface<Event>*>>(listeners).push_back(listener);
                return listener;
            }

            template<typename Event>
            void remove(listener_interface<Event>* listener) {
                std::erase(std::get<std::vector<listener_interface<Event>*>>(listeners), listener);
            }

            template<typename Event>
            void on(Event& e) {
                for (auto* listener : std::get<std::vector<listener_interface<Event>*>>(listeners))
                    listener->on(e);
            };
        };

        template<typename... Events>
        struct one_event_each_only {
            using event_types = type_list<Events...>;

        private:
            std::tuple<listener_interface<Events>*...> listeners{};

        public:
            template<typename Event>
            listener_interface<Event>* add(listener_interface<Event>* listener) {
                std::get<listener_interface<Event>*>(listeners) = listener;
                return listener;
            }

            template<typename Event>
            void remove() {
                std::get<listener_interface<Event>*>(listeners) = nullptr;
            }

            template<typename Event>
            void on(Event& e) {
                if (auto* listener = std::get<listener_interface<Event>*>(listeners))
                    listener->on(e);
            };
        };

        // リスナを保持せず、通知時に一時生成する。
        template<typename... Listeners>
        struct zero_cost {
            using listener_types = type_list<Listeners...>;
            using event_types = type_list<typename Listeners::listen_event...>;

            template<typename Event>
            void on(Event& event) {
                (
                    [&] {
                        if constexpr (std::same_as<typename Listeners::listen_event, Event>)
                            Listeners{}.on(event);
                    }(),
                    ...);
            }
        };

        // 状態を持つリスナだけを保持し、宣言順に通知する。
        template<typename... Listeners>
        struct minimum_cost {
            using listener_types = type_list<Listeners...>;
            using event_types = type_list<typename Listeners::listen_event...>;
            using stored_types = listener_types::template filter<helper::not_empty>;
            [[no_unique_address]] stored_types::template apply<std::tuple> listeners;

        private:
            template<typename Event, size_t Index = 0, size_t Stored = 0>
            void dispatch(Event& event) {
                if constexpr (Index < listener_types::size) {
                    using listener = typename listener_types::template get<Index>;
                    if constexpr (std::is_empty_v<listener>) {
                        if constexpr (std::same_as<typename listener::listen_event, Event>)
                            listener{}.on(event);
                        dispatch<Event, Index + 1, Stored>(event);
                    } else {
                        if constexpr (std::same_as<typename listener::listen_event, Event>)
                            std::get<Stored>(listeners).on(event);
                        dispatch<Event, Index + 1, Stored + 1>(event);
                    }
                }
            }

        public:
            template<typename Event>
            void on(Event& event) {
                dispatch(event);
            }
        };

        struct dummy {
            using event_types = type_list<>;

            template<typename Event>
            void on(Event&) {};
        };
    }

    // =============================== reflection ==============================
    // リフレクションによる列と行参照の生成
#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
    namespace storage {
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

        // 元のメンバ名と宣言順を保った集成体として、列コンテナを直接保持する。
        // 生成した型は構造化束縛でも扱える。
        template<typename Record, template<typename> typename Column>
        using soa = typename reflection_detail::columns_type<Record, Column>::type;

        template<typename Record, std::size_t Size>
        using static_soa = soa<Record, reflection_detail::array_column<Size>::template type>;

        template<typename Record>
        using dynamic_soa = soa<Record, std::vector>;

        // 各列の型と const 性を保って処理を展開し、参照タプルや実行時の検索を省く。
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

        }
    }
#endif

    // ================================= schema ================================
    // 型パックと構造体に共通のスキーマ
    // 構造体を一つの列として使う従来形式と区別するため、展開は record<T> で明示する。
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
#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
            static_assert((!is_record<Types> && ...), "record<T> must be the only schema argument");
#else
            static_assert((!is_record<Types> && ...),
                "Struct schemas require C++26 reflection; use type_list<Types...> in C++23");
#endif
            static constexpr bool named = false;
            using element_types = type_list<Types...>;
            using record_type = void;
            using value_type = std::tuple<Types...>;

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

            static consteval void validate_buffer_names() {}

            template<typename Value, typename Function>
            static constexpr decltype(auto) apply_value(Value&& value, Function&& function) {
                return std::apply(std::forward<Function>(function), std::forward<Value>(value));
            }

            template<typename Columns>
            static constexpr auto reference(Columns& columns, std::size_t index) {
                return [&]<std::size_t... Indices>(std::index_sequence<Indices...>) {
                    return std::tuple<decltype(column<Indices>(columns)[index])...>{
                        column<Indices>(columns)[index]...};
                }(std::index_sequence_for<Types...>{});
            }

            template<typename Columns>
            static constexpr auto views(Columns& columns, std::size_t size) {
                return [&]<std::size_t... Indices>(std::index_sequence<Indices...>) {
                    return std::tuple{
                        std::views::counted(std::ranges::begin(column<Indices>(columns)),
                            static_cast<std::ptrdiff_t>(size))...};
                }(std::index_sequence_for<Types...>{});
            }
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
            using value_type = Record;
            using element_types = [:record_element_types<Record>():];
            static constexpr auto members = std::define_static_array(
                std::meta::nonstatic_data_members_of(
                    ^^Record, std::meta::access_context::current()));

            static consteval void validate_buffer_names() {
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

            template<typename Columns>
            static constexpr auto reference(Columns& columns, std::size_t index) {
                return row(columns, index);
            }

            template<bool Const>
            struct view_column {
                template<typename Column>
                using type = decltype(std::views::counted(std::ranges::begin(std::declval<
                    std::conditional_t<Const, const Column, Column>&>()), std::ptrdiff_t{}));
            };

            template<typename Columns>
            static constexpr auto views(Columns& columns, std::size_t size) {
                using access = storage::reflection_detail::row_access<Columns>;
                using view_type = storage::soa<std::remove_const_t<Columns>,
                    view_column<std::is_const_v<Columns>>::template type>;
                return [&]<std::size_t... Indices>(std::index_sequence<Indices...>) {
                    return view_type{
                        std::views::counted(std::ranges::begin(column<Indices>(columns)),
                            static_cast<std::ptrdiff_t>(size))...};
                }(std::make_index_sequence<access::members.size()>{});
            }

            template<std::meta::info Member>
            static consteval std::size_t member_index() {
                for (std::size_t i = 0; i < members.size(); ++i)
                    if (members[i] == Member) return i;
                throw "The selected member must belong to this record";
            }

            template<typename Value, typename Function>
            static constexpr decltype(auto) apply_record(Value&& value, Function&& function) {
                return [&]<std::size_t... Indices>(
                    std::index_sequence<Indices...>) -> decltype(auto) {
                    return std::invoke(std::forward<Function>(function),
                        std::forward<Value>(value).[:members[Indices]:]...);
                }(std::make_index_sequence<members.size()>{});
            }

            template<typename Value, typename Function>
            static constexpr decltype(auto) apply_value(Value&& value, Function&& function) {
                return apply_record(std::forward<Value>(value), std::forward<Function>(function));
            }
        };
#endif

        // expected に参照を直接格納せず、通常の列は reference_wrapper で返す。
        // vector<bool> などの列では、要素アクセスが返すプロキシ値をそのまま使う。
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

    // ================================ storage ================================
    // 列ストレージと固定長・可変長の管理
    namespace storage {
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

        // 列コンテナの選択だけを方針に分離する。独自方針でも同じ行管理を使える。
        template<size_t MaxSize = DynamicSizeUnlimited>
        struct dynamic_layout {
            static constexpr bool resizable = true;
            static constexpr size_t max_size = MaxSize;
            template<typename T> using column = std::vector<T>;
        };

        template<size_t Size>
        struct fixed_layout {
            static constexpr bool resizable = false;
            static constexpr size_t max_size = Size;
            template<typename T> using column = std::array<T, Size>;
        };

        template<size_t ChunkSize>
        struct chunked_layout {
            static_assert(ChunkSize > 0, "ChunkSize must be positive");
            static constexpr bool resizable = true;
            static constexpr size_t max_size = DynamicSizeUnlimited;
            static constexpr size_t chunk_size = ChunkSize;
        };

        // ID やイベントに依存しない低水準の列管理。公開 entries の長さを直接変更しないこと。
        // 通常利用では、所有列を隠してビューを返す basic_soa を使う。
        template<typename Layout, typename... Types>
        class basic_columns {
        public:
            using schema_type = _detail::schema<Types...>;
            using element_types = typename schema_type::element_types;
            using value_type = typename schema_type::value_type;
            using data_tuple = typename element_types::template apply<std::tuple>;
            using columns_type = typename schema_type::template columns<Layout::template column>;

        private:
            using column_indices = std::make_index_sequence<element_types::size>;
            template<size_t N>
            using column_reference =
                decltype(schema_type::template column<N>(std::declval<columns_type&>()));

            template<typename... Values>
            using copy_traits = std::bool_constant<(std::is_copy_constructible_v<Values> && ...)>;
            template<typename... Values>
            using relocation_traits = std::bool_constant<
                ((std::is_copy_constructible_v<Values> ||
                  std::is_nothrow_move_constructible_v<Values>) && ...)>;
            static constexpr bool copyable = element_types::template apply<copy_traits>::value;
            static_assert(
                !Layout::resizable || element_types::template apply<relocation_traits>::value,
                "Resizable columns require copy construction or noexcept move construction");

            static constexpr bool vector_columns =
                []<size_t... Indices>(std::index_sequence<Indices...>) {
                    return Layout::resizable &&
                        (std::same_as<column_reference<Indices>,
                            std::vector<typename element_types::template get<Indices>>&> && ...);
                }(column_indices{});

            // スカラの値初期化を標準 vector にまとめても、行の構築・破棄の副作用が変わらない場合に限る。
            // 独自列やメンバ初期化子を持つ構造体は、従来どおり行ごとに初期化する。
            static constexpr bool bulk_resizable =
                []<size_t... Indices>(std::index_sequence<Indices...>) {
                    return vector_columns &&
                        (std::is_scalar_v<typename element_types::template get<Indices>> && ...) &&
                        (!schema_type::named ||
                            (std::is_trivially_default_constructible_v<value_type> &&
                             std::is_trivially_destructible_v<value_type>));
                }(column_indices{});
            size_t m_size = 0;

            // 型パック・reflection の違いを隠し、宣言順に各列を処理する。
            template<typename Self, typename Function>
            static void for_each_column(Self& self, Function&& function) {
                [&]<size_t... Indices>(std::index_sequence<Indices...>) {
                    (function(self.template get<Indices>()), ...);
                }(column_indices{});
            }

            static columns_type take_columns(basic_columns& other) noexcept(
                std::is_nothrow_move_constructible_v<columns_type>) {
                if constexpr (std::is_nothrow_move_constructible_v<columns_type>) {
                    return std::move(other.entries);
                } else if constexpr (copyable) {
                    return other.entries;
                } else {
                    // 列の移動中に失敗しても、移動元の行数と列長を一致させる。
                    try {
                        return std::move(other.entries);
                    } catch (...) {
                        other.clear();
                        throw;
                    }
                }
            }

            void rollback() noexcept {
                if constexpr (Layout::resizable) {
                    for_each_column(*this, [&](auto& column) {
                        while (column.size() > m_size) column.pop_back();
                    });
                }
            }

            void assign_row(size_t index, data_tuple& values) {
                [&]<size_t... Indices>(std::index_sequence<Indices...>) {
                    ((get<Indices>()[index] =
                        std::move_if_noexcept(std::get<Indices>(values))), ...);
                }(column_indices{});
            }

            template<typename T>
            static bool has_spare_capacity(const std::vector<T>& column) {
                if constexpr (std::same_as<T, bool>)
                    return column.size() != column.capacity();
                else
                    return column.data() &&
                           column.data() + column.size() < column.data() + column.capacity();
            }

            // 再確保を含む処理は、通常の追加経路から分離する。
            void append_with_growth(data_tuple& values) {
                if (size() == max_size())
                    throw std::length_error("Column count exceeds max_size");
                if constexpr (Layout::resizable) {
                    if (size() == capacity())
                        reserve(size() + std::min(max_size() - size(), size() / 2 + 1));
                    try {
                        [&]<size_t... Indices>(std::index_sequence<Indices...>) {
                            (get<Indices>().push_back(
                                std::move_if_noexcept(std::get<Indices>(values))), ...);
                        }(column_indices{});
                    } catch (...) {
                        rollback();
                        throw;
                    }
                } else {
                    assign_row(m_size, values);
                }
                ++m_size;
            }

            template<size_t... Indices>
            void append_available(data_tuple& values, std::index_sequence<Indices...>) {
                if constexpr (vector_columns) {
                    // 標準 allocator の配列内でポインタを比較し、再確保が不要なことを直接示す。
                    if (size() < max_size() &&
                        (has_spare_capacity(get<Indices>()) && ...)) [[likely]] {
                        try {
                            (get<Indices>().emplace_back(
                                std::move_if_noexcept(std::get<Indices>(values))), ...);
                        } catch (...) {
                            rollback();
                            throw;
                        }
                        ++m_size;
                        return;
                    }
                }
                append_with_growth(values);
            }

        protected:
            void append_values(data_tuple& values) {
                append_available(values, column_indices{});
            }

        public:
            columns_type entries{};

            basic_columns() = default;

            basic_columns(const basic_columns&) requires copyable = default;

            basic_columns(const basic_columns&) requires (!copyable) = delete;

            basic_columns(basic_columns&& other) noexcept(
                std::is_nothrow_move_constructible_v<columns_type>)
                : m_size(other.m_size), entries(take_columns(other)) {
                other.clear();
            }

            static constexpr bool nothrow_swap =
                []<size_t... Indices>(std::index_sequence<Indices...>) {
                    return (std::is_nothrow_swappable_v<column_reference<Indices>> && ...);
                }(column_indices{});

            basic_columns& operator=(const basic_columns& other) requires copyable {
                if (this != std::addressof(other)) {
                    basic_columns copy(other);
                    swap(copy);
                }
                return *this;
            }

            basic_columns& operator=(const basic_columns&) requires (!copyable) = delete;

            basic_columns& operator=(basic_columns&& other) noexcept(
                std::is_nothrow_move_constructible_v<columns_type> && nothrow_swap) {
                basic_columns moved(std::move(other));
                swap(moved);
                return *this;
            }

            void swap(basic_columns& other) noexcept(nothrow_swap) {
                using std::swap;
                [&]<size_t... Indices>(std::index_sequence<Indices...>) {
                    (swap(get<Indices>(), other.template get<Indices>()), ...);
                }(column_indices{});
                swap(m_size, other.m_size);
            }

            template<size_t N>
            decltype(auto) get() {
                return schema_type::template column<N>(entries);
            }

            template<size_t N>
            decltype(auto) get() const {
                return schema_type::template column<N>(entries);
            }

            [[nodiscard]] size_t size() const noexcept { return m_size; }

            [[nodiscard]] bool empty() const noexcept { return m_size == 0; }

            [[nodiscard]] size_t max_size() const noexcept {
                auto limit = std::min(Layout::max_size,
                                      static_cast<size_t>(std::numeric_limits<ptrdiff_t>::max()));
                if constexpr (Layout::resizable) {
                    for_each_column(*this, [&](const auto& column) {
                        if constexpr (requires { column.max_size(); })
                            limit = std::min(limit, column.max_size());
                    });
                }
                return limit;
            }

            [[nodiscard]] size_t capacity() const noexcept {
                auto count = max_size();
                if constexpr (Layout::resizable) {
                    for_each_column(*this, [&](const auto& column) {
                        if constexpr (requires { column.capacity(); })
                            count = std::min(count, column.capacity());
                        else
                            count = std::min(count, column.size());
                    });
                }
                return count;
            }

            void reserve(size_t count) {
                if (count > max_size())
                    throw std::length_error("Column capacity exceeds max_size");
                if constexpr (Layout::resizable) {
                    for_each_column(*this, [&](auto& column) {
                        if constexpr (requires { column.reserve(count); })
                            column.reserve(count);
                    });
                }
            }

            template<typename... Args> requires (sizeof...(Args) == element_types::size)
            void push_back(Args&&... args) {
                // 自分の列への参照も、いずれかの列を再確保する前に退避する。
                data_tuple values(std::forward<Args>(args)...);
                append_values(values);
            }

            void push_value(value_type value) {
                schema_type::apply_value(std::move(value), [&](auto&&... values) {
                    push_back(std::forward<decltype(values)>(values)...);
                });
            }

            void resize(size_t count) requires std::default_initializable<value_type> {
                if (count > max_size())
                    throw std::length_error("Column count exceeds max_size");
                if constexpr (element_types::size == 0) {
                    m_size = count;
                } else if constexpr (bulk_resizable) {
                    try {
                        for_each_column(*this, [&](auto& column) { column.resize(count); });
                    } catch (...) {
                        rollback();
                        throw;
                    }
                    m_size = count;
                } else if (count < size()) {
                    while (size() > count) pop_back();
                } else {
                    reserve(count);
                    auto old_size = size();
                    try {
                        while (size() < count) push_value(value_type{});
                    } catch (...) {
                        while (size() > old_size) pop_back();
                        throw;
                    }
                }
            }

            void pop_back() noexcept {
                if (empty()) return;
                if constexpr (Layout::resizable) {
                    for_each_column(*this, [](auto& column) { column.pop_back(); });
                }
                --m_size;
            }

            void clear() noexcept {
                if constexpr (Layout::resizable) {
                    for_each_column(*this, [](auto& column) { column.clear(); });
                }
                m_size = 0;
            }

            void swap_rows(size_t left, size_t right) {
                if (left >= size() || right >= size())
                    throw std::out_of_range("Row index exceeds size");
                [&]<size_t... Indices>(std::index_sequence<Indices...>) {
                    (std::ranges::iter_swap(std::ranges::begin(get<Indices>()) + left,
                                           std::ranges::begin(get<Indices>()) + right), ...);
                }(column_indices{});
            }

            template<size_t N> auto column() {
                return std::views::counted(std::ranges::begin(get<N>()),
                                           static_cast<ptrdiff_t>(size()));
            }

            template<size_t N> auto column() const {
                return std::views::counted(std::ranges::begin(get<N>()),
                                           static_cast<ptrdiff_t>(size()));
            }

            auto columns() { return schema_type::views(entries, size()); }

            auto columns() const { return schema_type::views(entries, size()); }

            auto row(size_t index) { return schema_type::reference(entries, index); }

            auto row(size_t index) const { return schema_type::reference(entries, index); }

            value_type read(size_t index) const requires std::is_copy_constructible_v<value_type> {
                if (index >= size()) throw std::out_of_range("Row index exceeds size");
                return [&]<size_t... Indices>(std::index_sequence<Indices...>) {
                    return value_type{get<Indices>()[index]...};
                }(column_indices{});
            }

            void write(size_t index, value_type value) {
                if (index >= size()) throw std::out_of_range("Row index exceeds size");
                schema_type::apply_value(std::move(value), [&](auto&&... fields) {
                    data_tuple values(std::forward<decltype(fields)>(fields)...);
                    assign_row(index, values);
                });
            }
        };

    }

    // =============================== iteration ===============================
    // 行インデックスと参照の取り出し方を分離し、列やチャンクの走査でも共有する。
    namespace _detail {
        struct indexed_access {
            template<typename Parent>
            static constexpr decltype(auto) get(Parent& parent, size_t index) {
                return parent[index];
            }
        };

        template<typename Parent, typename Access = indexed_access>
        class indexed_iterator {
            template<typename, typename>
            friend class indexed_iterator;
            Parent* m_parent = nullptr;
            ptrdiff_t m_index = 0;

        public:
            using iterator_concept = std::random_access_iterator_tag;
            using difference_type = ptrdiff_t;
            using reference = decltype(Access::get(std::declval<Parent&>(), size_t{}));
            using value_type = std::remove_cvref_t<reference>;
            using iterator_category = std::conditional_t<std::is_lvalue_reference_v<reference>,
                std::random_access_iterator_tag, std::input_iterator_tag>;

            struct arrow_proxy {
                value_type row;

                constexpr const value_type* operator->() const { return std::addressof(row); }
            };

            using pointer = std::conditional_t<std::is_lvalue_reference_v<reference>,
                std::add_pointer_t<std::remove_reference_t<reference>>, arrow_proxy>;

            constexpr indexed_iterator() = default;

            constexpr indexed_iterator(Parent* parent, difference_type index)
                : m_parent(parent), m_index(index) {}

            template<typename Other>
                requires std::is_convertible_v<Other*, Parent*>
            constexpr indexed_iterator(const indexed_iterator<Other, Access>& other)
                : m_parent(other.m_parent), m_index(other.m_index) {}

            constexpr reference operator*() const {
                return Access::get(*m_parent, static_cast<size_t>(m_index));
            }

            constexpr pointer operator->() const {
                if constexpr (std::is_lvalue_reference_v<reference>)
                    return std::addressof(**this);
                else
                    return {**this};
            }

            constexpr reference operator[](difference_type offset) const {
                return *(*this + offset);
            }

            constexpr indexed_iterator& operator++() {
                ++m_index;
                return *this;
            }

            constexpr indexed_iterator operator++(int) {
                auto old = *this;
                ++*this;
                return old;
            }

            constexpr indexed_iterator& operator--() {
                --m_index;
                return *this;
            }

            constexpr indexed_iterator operator--(int) {
                auto old = *this;
                --*this;
                return old;
            }

            constexpr indexed_iterator& operator+=(difference_type offset) {
                m_index += offset;
                return *this;
            }

            constexpr indexed_iterator& operator-=(difference_type offset) {
                m_index -= offset;
                return *this;
            }

            friend constexpr indexed_iterator operator+(indexed_iterator it, difference_type n) {
                return it += n;
            }

            friend constexpr indexed_iterator operator+(difference_type n, indexed_iterator it) {
                return it += n;
            }

            friend constexpr indexed_iterator operator-(indexed_iterator it, difference_type n) {
                return it -= n;
            }

            friend constexpr difference_type operator-(indexed_iterator a, indexed_iterator b) {
                return a.m_index - b.m_index;
            }

            friend constexpr bool operator==(
                const indexed_iterator&, const indexed_iterator&) = default;

            friend constexpr auto operator<=>(
                const indexed_iterator&, const indexed_iterator&) = default;
        };

        template<typename Access, typename Parent>
        auto indexed_range(Parent& parent, size_t count) {
            using iterator = indexed_iterator<Parent, Access>;
            return std::ranges::subrange(iterator{std::addressof(parent), 0},
                iterator{std::addressof(parent), static_cast<ptrdiff_t>(count)});
        }

        // ================================== soa ==================================
        // 所有列を隠し、行数と列長を一体で管理する ID なしのコンテナ。
        // 列ビューの要素は更新できるが、ビューから所有列の長さは変更できない。
        template<typename Layout, typename... Types>
        class soa_buffer {
            using storage_type = storage::basic_columns<Layout, Types...>;
            using schema_type = typename storage_type::schema_type;
            storage_type m_storage;

            template<bool Const>
            using parent_type = std::conditional_t<Const, const soa_buffer, soa_buffer>;

        public:
            using value_type = typename storage_type::value_type;
            using element_types = typename storage_type::element_types;

            template<bool Const>
            using basic_iterator = _detail::indexed_iterator<parent_type<Const>>;

            size_t size() const { return m_storage.size(); }

            bool empty() const { return m_storage.empty(); }

            size_t capacity() const { return m_storage.capacity(); }

            size_t max_size() const { return m_storage.max_size(); }

            void reserve(size_t count) { m_storage.reserve(count); }

            void resize(size_t count)
                requires std::default_initializable<value_type>
            {
                m_storage.resize(count);
            }

            void clear() { m_storage.clear(); }

            void pop_back() { m_storage.pop_back(); }

            void push_back(value_type value) { m_storage.push_value(std::move(value)); }

            template<typename... Args>
                requires(sizeof...(Args) == element_types::size &&
                         !(sizeof...(Args) == 1 &&
                             (std::same_as<std::remove_cvref_t<Args>, value_type> && ...)))
            void push_back(Args&&... args) {
                m_storage.push_back(std::forward<Args>(args)...);
            }

            // 参照プロキシは行の値そのものではない。値のコピー・代入は read/write で明示する。
            auto operator[](size_t index) { return m_storage.row(index); }

            auto operator[](size_t index) const { return m_storage.row(index); }

            auto at(size_t index) {
                if (index >= size())
                    throw std::out_of_range("SoA row index");
                return (*this)[index];
            }

            auto at(size_t index) const {
                if (index >= size())
                    throw std::out_of_range("SoA row index");
                return (*this)[index];
            }

            value_type read(size_t index) const
                requires std::is_copy_constructible_v<value_type>
            {
                return m_storage.read(index);
            }

            void write(size_t index, value_type value) { m_storage.write(index, std::move(value)); }

            void erase_unordered(size_t index) {
                if (index >= size())
                    throw std::out_of_range("SoA row index");
                if (index != size() - 1)
                    m_storage.swap_rows(index, size() - 1);
                pop_back();
            }

            void swap(soa_buffer& other) noexcept(storage_type::nothrow_swap) {
                m_storage.swap(other.m_storage);
            }

            friend void swap(soa_buffer& a, soa_buffer& b) noexcept(storage_type::nothrow_swap) {
                a.swap(b);
            }

            template<size_t N>
            auto column() & {
                return m_storage.template column<N>();
            }

            template<size_t N>
            auto column() const& {
                return m_storage.template column<N>();
            }

            template<size_t N>
            auto column() && = delete;

            template<size_t N>
            auto column() const&& = delete;

            template<size_t N>
            auto get() & {
                return column<N>();
            }

            template<size_t N>
            auto get() const& {
                return column<N>();
            }

            template<size_t N>
            auto get() && = delete;

            template<size_t N>
            auto get() const&& = delete;

            auto columns() & { return m_storage.columns(); }

            auto columns() const& { return m_storage.columns(); }

            auto columns() && = delete;

            auto columns() const&& = delete;

#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
            template<std::meta::info Member>
            auto column() & {
                return column<schema_type::template member_index<Member>()>();
            }

            template<std::meta::info Member>
            auto column() const& {
                return column<schema_type::template member_index<Member>()>();
            }

            template<std::meta::info Member>
            auto column() && = delete;

            template<std::meta::info Member>
            auto column() const&& = delete;
#endif

            using iterator = basic_iterator<false>;
            using const_iterator = basic_iterator<true>;

            iterator begin() { return {this, 0}; }

            iterator end() { return {this, static_cast<ptrdiff_t>(size())}; }

            const_iterator begin() const { return {this, 0}; }

            const_iterator end() const { return {this, static_cast<ptrdiff_t>(size())}; }

            const_iterator cbegin() const { return begin(); }

            const_iterator cend() const { return end(); }
        };

        // ================================ chunked ================================
        // 固定長 SoA をブロックとして保持する。ポインタ表の拡張で既存のチャンクは移動しない。
        template<size_t ChunkSize, typename... Types>
        class chunked_buffer {
            static_assert(ChunkSize > 0 && ChunkSize <= static_cast<size_t>(
                                                            std::numeric_limits<ptrdiff_t>::max()),
                "ChunkSize must be positive and fit ptrdiff_t");
            using schema_type = _detail::schema<Types...>;

        public:
            static constexpr size_t chunk_size = ChunkSize;
            using chunk_type = soa_buffer<storage::fixed_layout<ChunkSize>, Types...>;
            using value_type = typename chunk_type::value_type;
            using element_types = typename chunk_type::element_types;

        private:
            using data_tuple = typename element_types::template apply<std::tuple>;
            std::vector<std::unique_ptr<chunk_type>> m_chunks;
            size_t m_size = 0;

            static constexpr size_t required_chunks(size_t count) noexcept {
                return count / ChunkSize + (count % ChunkSize != 0);
            }

            // unique_ptr 自体の const 性は参照先に伝わらないため、ここで const を付ける。
            chunk_type& chunk_for_row(size_t index) { return *m_chunks[index / ChunkSize]; }

            const chunk_type& chunk_for_row(size_t index) const {
                return *m_chunks[index / ChunkSize];
            }

            void check_row_index(size_t index) const {
                if (index >= size())
                    throw std::out_of_range("Chunked SoA row index");
            }

            void check_chunk_index(size_t index) const {
                if (index >= chunk_count())
                    throw std::out_of_range("Chunk index");
            }

            void append_chunk() {
                if (size() == max_size())
                    throw std::length_error("Chunked SoA exceeds max_size");
                m_chunks.push_back(std::make_unique<chunk_type>());
            }

            void insert(data_tuple& values) {
                if (size() == capacity()) [[unlikely]]
                    append_chunk();
                std::apply(
                    [&](auto&&... fields) {
                        chunk_for_row(m_size).push_back(std::forward<decltype(fields)>(fields)...);
                    },
                    std::move(values));
                ++m_size;
            }

            void truncate(size_t count) noexcept {
                auto remaining = required_chunks(count);
                for (size_t i = remaining; i < chunk_count(); ++i)
                    m_chunks[i]->clear();
                if (remaining) {
                    auto& tail = *m_chunks[remaining - 1];
                    auto tail_size = count - (remaining - 1) * ChunkSize;
                    while (tail.size() > tail_size)
                        tail.pop_back();
                }
                m_size = count;
            }

            template<size_t Column>
            decltype(auto) column_at(size_t index) {
                return chunk_for_row(index).template column<Column>()[index % ChunkSize];
            }

            template<size_t Column>
            decltype(auto) column_at(size_t index) const {
                return chunk_for_row(index).template column<Column>()[index % ChunkSize];
            }

            void swap_rows(size_t left, size_t right) {
                [&]<size_t... Indices>(std::index_sequence<Indices...>) {
                    (std::ranges::swap(column_at<Indices>(left), column_at<Indices>(right)), ...);
                }(std::make_index_sequence<element_types::size>{});
            }

            template<size_t Column>
            struct column_access {
                template<typename Parent>
                static decltype(auto) get(Parent& parent, size_t index) {
                    return parent.template column_at<Column>(index);
                }
            };

            struct chunk_access {
                template<typename Parent>
                static auto get(Parent& parent, size_t index) {
                    return parent.chunk(index);
                }
            };

        public:
            // 要素の更新だけを許可し、個別チャンクの resize による行数の不一致を防ぐ。
            template<bool Const>
            class basic_chunk_view {
                friend class chunked_buffer;
                using parent_type = std::conditional_t<Const, const chunk_type, chunk_type>;
                parent_type* m_chunk;

                explicit basic_chunk_view(parent_type* chunk) : m_chunk(chunk) {}

            public:
                size_t size() const { return m_chunk->size(); }

                bool empty() const { return m_chunk->empty(); }

                static constexpr size_t capacity() { return ChunkSize; }

                auto columns() const { return m_chunk->columns(); }

                template<size_t Column>
                auto column() const {
                    return m_chunk->template column<Column>();
                }
#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
                template<std::meta::info Member>
                auto column() const {
                    return m_chunk->template column<Member>();
                }
#endif
                template<size_t N>
                auto get() const {
                    return column<N>();
                }

                auto operator[](size_t index) const { return (*m_chunk)[index]; }

                auto at(size_t index) const { return m_chunk->at(index); }

                value_type read(size_t index) const
                    requires std::is_copy_constructible_v<value_type>
                {
                    return m_chunk->read(index);
                }

                void write(size_t index, value_type value) const
                    requires(!Const)
                {
                    m_chunk->write(index, std::move(value));
                }

                auto begin() const { return m_chunk->begin(); }

                auto end() const { return m_chunk->end(); }
            };

            using chunk_view = basic_chunk_view<false>;
            using const_chunk_view = basic_chunk_view<true>;

            // ========================== ownership ===========================
            chunked_buffer() = default;

            chunked_buffer(const chunked_buffer& other)
                requires std::is_copy_constructible_v<chunk_type>
                : m_size(other.m_size) {
                m_chunks.reserve(other.m_chunks.size());
                for (const auto& chunk : other.m_chunks)
                    m_chunks.push_back(std::make_unique<chunk_type>(std::as_const(*chunk)));
            }

            chunked_buffer(const chunked_buffer&)
                requires(!std::is_copy_constructible_v<chunk_type>)
            = delete;

            chunked_buffer(chunked_buffer&& other) noexcept
                : m_chunks(std::move(other.m_chunks)), m_size(std::exchange(other.m_size, 0)) {
                other.m_chunks.clear();
            }

            chunked_buffer& operator=(const chunked_buffer& other)
                requires std::is_copy_constructible_v<chunk_type>
            {
                if (this != std::addressof(other)) {
                    chunked_buffer copy(other);
                    swap(copy);
                }
                return *this;
            }

            chunked_buffer& operator=(const chunked_buffer&)
                requires(!std::is_copy_constructible_v<chunk_type>)
            = delete;

            chunked_buffer& operator=(chunked_buffer&& other) noexcept {
                if (this != std::addressof(other)) {
                    m_chunks = std::move(other.m_chunks);
                    m_size = std::exchange(other.m_size, 0);
                    other.m_chunks.clear();
                }
                return *this;
            }

            void swap(chunked_buffer& other) noexcept {
                m_chunks.swap(other.m_chunks);
                std::swap(m_size, other.m_size);
            }

            friend void swap(chunked_buffer& left, chunked_buffer& right) noexcept {
                left.swap(right);
            }

            // =========================== capacity ===========================
            size_t size() const noexcept { return m_size; }

            bool empty() const noexcept { return size() == 0; }

            size_t chunk_count() const noexcept { return required_chunks(size()); }

            size_t max_size() const noexcept {
                auto limit = static_cast<size_t>(std::numeric_limits<ptrdiff_t>::max());
                return m_chunks.max_size() < required_chunks(limit)
                           ? m_chunks.max_size() * ChunkSize
                           : limit;
            }

            size_t capacity() const noexcept {
                auto limit = max_size();
                return m_chunks.size() > limit / ChunkSize ? limit : m_chunks.size() * ChunkSize;
            }

            void reserve(size_t count) {
                if (count > max_size())
                    throw std::length_error("Chunked SoA capacity exceeds max_size");
                auto desired = required_chunks(count);
                m_chunks.reserve(desired);
                while (m_chunks.size() < desired)
                    m_chunks.push_back(std::make_unique<chunk_type>());
            }

            void resize(size_t count)
                requires std::default_initializable<value_type>
            {
                if (count <= size()) {
                    truncate(count);
                    return;
                }
                reserve(count);
                auto old_size = size();
                try {
                    while (size() < count) {
                        auto index = size() / ChunkSize;
                        auto& chunk = *m_chunks[index];
                        auto previous = chunk.size();
                        auto desired = std::min(ChunkSize, count - index * ChunkSize);
                        chunk.resize(desired);
                        m_size += desired - previous;
                    }
                } catch (...) {
                    truncate(old_size);
                    throw;
                }
            }

            void clear() noexcept { truncate(0); }

            void pop_back() noexcept {
                if (empty())
                    return;
                chunk_for_row(m_size - 1).pop_back();
                --m_size;
            }

            void shrink_to_fit() {
                while (m_chunks.size() > chunk_count())
                    m_chunks.pop_back();
                m_chunks.shrink_to_fit();
            }

            // ============================= rows =============================
            void push_back(value_type value) {
                schema_type::apply_value(std::move(value), [&](auto&&... fields) {
                    push_back(std::forward<decltype(fields)>(fields)...);
                });
            }

            template<typename... Args>
                requires(sizeof...(Args) == element_types::size &&
                         !(sizeof...(Args) == 1 &&
                             (std::same_as<std::remove_cvref_t<Args>, value_type> && ...)))
            void push_back(Args&&... args) {
                data_tuple values(std::forward<Args>(args)...);
                insert(values);
            }

            auto operator[](size_t index) { return chunk_for_row(index)[index % ChunkSize]; }

            auto operator[](size_t index) const { return chunk_for_row(index)[index % ChunkSize]; }

            auto at(size_t index) {
                check_row_index(index);
                return (*this)[index];
            }

            auto at(size_t index) const {
                check_row_index(index);
                return (*this)[index];
            }

            value_type read(size_t index) const
                requires std::is_copy_constructible_v<value_type>
            {
                check_row_index(index);
                return chunk_for_row(index).read(index % ChunkSize);
            }

            void write(size_t index, value_type value) {
                check_row_index(index);
                chunk_for_row(index).write(index % ChunkSize, std::move(value));
            }

            void erase_unordered(size_t index) {
                check_row_index(index);
                auto last = size() - 1;
                if (index != last)
                    swap_rows(index, last);
                pop_back();
            }

            // ============================= views ============================
            chunk_view chunk(size_t index) & {
                check_chunk_index(index);
                return chunk_view(m_chunks[index].get());
            }

            const_chunk_view chunk(size_t index) const& {
                check_chunk_index(index);
                return const_chunk_view(m_chunks[index].get());
            }

            chunk_view chunk(size_t) && = delete;

            const_chunk_view chunk(size_t) const&& = delete;

            auto chunks() & { return _detail::indexed_range<chunk_access>(*this, chunk_count()); }

            auto chunks() const& {
                return _detail::indexed_range<chunk_access>(*this, chunk_count());
            }

            auto chunks() && = delete;

            auto chunks() const&& = delete;

            template<size_t Column>
            auto column() & {
                return _detail::indexed_range<column_access<Column>>(*this, size());
            }

            template<size_t Column>
            auto column() const& {
                return _detail::indexed_range<column_access<Column>>(*this, size());
            }

            template<size_t Column>
            auto column() && = delete;

            template<size_t Column>
            auto column() const&& = delete;
#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
            template<std::meta::info Member>
            auto column() & {
                return column<schema_type::template member_index<Member>()>();
            }

            template<std::meta::info Member>
            auto column() const& {
                return column<schema_type::template member_index<Member>()>();
            }

            template<std::meta::info Member>
            auto column() && = delete;

            template<std::meta::info Member>
            auto column() const&& = delete;
#endif
            template<size_t N>
            auto get() & {
                return column<N>();
            }

            template<size_t N>
            auto get() const& {
                return column<N>();
            }

            template<size_t N>
            auto get() && = delete;

            template<size_t N>
            auto get() const&& = delete;

            using iterator = _detail::indexed_iterator<chunked_buffer>;
            using const_iterator = _detail::indexed_iterator<const chunked_buffer>;

            iterator begin() { return {this, 0}; }

            iterator end() { return {this, static_cast<ptrdiff_t>(size())}; }

            const_iterator begin() const { return {this, 0}; }

            const_iterator end() const { return {this, static_cast<ptrdiff_t>(size())}; }

            const_iterator cbegin() const { return begin(); }

            const_iterator cend() const { return end(); }
        };

        // 通常の列は直接参照し、vector<bool> などのプロキシ列だけ値を退避して書き戻す。
        template<typename Fields, typename Owner, typename Function>
        void with_row_references(Owner& owner, size_t position, Function&& function) {
            [&]<size_t... Indices>(std::index_sequence<Indices...>) {
                using references =
                    std::tuple<std::conditional_t<std::is_lvalue_reference_v<decltype(owner
                                                          .template column<Indices>()[position])>,
                        decltype(owner.template column<Indices>()[position]),
                        typename Fields::template get<Indices>>...>;
                references values(owner.template column<Indices>()[position]...);
                auto tied = std::apply([](auto&... fields) { return std::tie(fields...); }, values);
                auto commit = [&] {
                    (
                        [&] {
                            if constexpr (!std::is_const_v<Owner> &&
                                          !std::is_lvalue_reference_v<
                                              decltype(owner.template column<Indices>()[position])>)
                                owner.template column<Indices>()[position] =
                                    std::move(std::get<Indices>(values));
                        }(),
                        ...);
                };
                try {
                    function(tied);
                } catch (...) {
                    commit();
                    throw;
                }
                commit();
            }(std::make_index_sequence<Fields::size>{});
        }

        template<typename Fields, typename ID = void>
        struct mutation_events;

        template<typename... Fields, typename ID>
        struct mutation_events<type_list<Fields...>, ID> {
            using before_delete = eventsystem::event::will_delete_event<ID, Fields...>;
            using after_delete = eventsystem::event::did_delete_event<ID, Fields...>;
            using before_swap = eventsystem::event::will_swap_event<ID, Fields...>;
            using after_swap = eventsystem::event::did_swap_event<ID, Fields...>;
            using before_update = eventsystem::event::will_update_event<ID, Fields...>;
            using after_update = eventsystem::event::did_update_event<ID, Fields...>;
        };

        template<typename... Fields>
        struct mutation_events<type_list<Fields...>, void> {
            using before_delete = eventsystem::event::will_delete_row_event<Fields...>;
            using after_delete = eventsystem::event::did_delete_row_event<Fields...>;
            using before_swap = eventsystem::event::will_swap_row_event<Fields...>;
            using after_swap = eventsystem::event::did_swap_row_event<Fields...>;
            using before_update = eventsystem::event::will_update_row_event<Fields...>;
            using after_update = eventsystem::event::did_update_row_event<Fields...>;
        };

        template<typename Event, typename Events>
        inline constexpr bool observes = Events::event_types::template contains<Event>;

        template<typename Event, typename Events, typename... Args>
        bool dispatch_event(Events& events, Args&&... args) {
            if constexpr (observes<Event, Events>) {
                Event event{std::forward<Args>(args)...};
                events.on(event);
                if constexpr (requires { event.canceled(); })
                    return !event.canceled();
            }
            return true;
        }

        template<typename Event, typename Events, typename Owner, typename... IDs>
        bool notify_row(Events& events, Owner& owner, size_t position, const IDs&... ids) {
            if constexpr (observes<Event, Events>) {
                bool accepted = true;
                with_row_references<typename Owner::element_types>(
                    owner, position, [&](auto references) {
                        accepted = dispatch_event<Event>(events, ids..., position, references);
                    });
                return accepted;
            }
            return true;
        }

        template<typename Event, typename Events, typename Owner, typename... IDs>
        bool notify_swap(
            Events& events, Owner& owner, size_t left, size_t right, const IDs&... ids) {
            if constexpr (observes<Event, Events>) {
                bool accepted = true;
                with_row_references<typename Owner::element_types>(owner, left, [&](auto first) {
                    with_row_references<typename Owner::element_types>(
                        owner, right, [&](auto second) {
                            accepted =
                                dispatch_event<Event>(events, ids..., left, right, first, second);
                        });
                });
                return accepted;
            }
            return true;
        }

        template<typename Owner>
        void swap_row_values(Owner& owner, size_t left, size_t right) {
            [&]<size_t... Indices>(std::index_sequence<Indices...>) {
                (
                    [&] {
                        auto view = owner.template column<Indices>();
                        auto first = std::ranges::begin(view);
                        std::ranges::iter_swap(first + left, first + right);
                    }(),
                    ...);
            }(std::make_index_sequence<Owner::element_types::size>{});
        }

        // ID 管理を列の配置から分離する。
        template<typename Storage, generator::is_generator Generator,
            eventsystem::is_eventsystem Events>
        class identified_buffer {
            Storage m_storage;
            [[no_unique_address]] Generator m_generator;
            [[no_unique_address]] Events m_events;
            using schema_type = typename Storage::schema_type;
            using data_tuple = typename Storage::element_types::template apply<std::tuple>;
            using notifications =
                mutation_events<typename Storage::element_types, typename Generator::generate_type>;

        public:
            using value_type = typename Storage::value_type;
            using element_types = typename Storage::element_types;
            using id_type = typename Generator::generate_type;
            static constexpr bool has_identity = true;
            static_assert([] {
                schema_type::validate_buffer_names();
                return true;
            }());

            template<bool Const>
            struct basic_entry : Storage::template field_reference<Const> {
                using parent_type =
                    std::conditional_t<Const, const identified_buffer, identified_buffer>;
                using fields_type = typename Storage::template field_reference<Const>;
                parent_type* parent;
                size_t index;

                basic_entry(parent_type* parent, size_t index)
                    : fields_type(parent->m_storage.fields(index)), parent(parent), index(index) {}

                id_type id() const { return parent->m_storage.ids[index]; }

                template<size_t N>
                decltype(auto) get() const {
                    return parent->template column<N>()[index];
                }
            };

            using entry = basic_entry<false>;
            using const_entry = basic_entry<true>;
            template<bool Const>
            using basic_iterator = indexed_iterator<
                std::conditional_t<Const, const identified_buffer, identified_buffer>>;
            using iterator = basic_iterator<false>;
            using const_iterator = basic_iterator<true>;

            size_t size() const { return m_storage.size(); }

            bool empty() const { return m_storage.empty(); }

            size_t max_size() const { return m_storage.row_limit(); }

            size_t capacity() const { return m_storage.capacity(); }

            void reserve(size_t count) { m_storage.reserve(count); }

            void swap(identified_buffer& other) noexcept(
                noexcept(m_storage.swap(other.m_storage)) &&
                std::is_nothrow_swappable_v<Generator> && std::is_nothrow_swappable_v<Events>) {
                m_storage.swap(other.m_storage);
                using std::swap;
                swap(m_generator, other.m_generator);
                swap(m_events, other.m_events);
            }

            friend void swap(identified_buffer& a, identified_buffer& b) noexcept(
                noexcept(a.swap(b))) {
                a.swap(b);
            }

            template<size_t N>
            auto column() & {
                return m_storage.template column<N>();
            }

            template<size_t N>
            auto column() const& {
                return m_storage.template column<N>();
            }

            template<size_t N>
            auto column() && = delete;

            template<size_t N>
            auto column() const&& = delete;

            template<size_t N>
            auto get() & {
                return column<N>();
            }

            template<size_t N>
            auto get() const& {
                return column<N>();
            }

            template<size_t N>
            auto get() && = delete;

            template<size_t N>
            auto get() const&& = delete;

            auto columns() &
                requires requires { m_storage.columns(); }
            {
                return m_storage.columns();
            }

            auto columns() const&
                requires requires { m_storage.columns(); }
            {
                return m_storage.columns();
            }

            auto columns() && = delete;

            auto columns() const&& = delete;

            auto chunks() &
                requires requires { m_storage.chunks(); }
            {
                return m_storage.chunks();
            }

            auto chunks() const&
                requires requires { m_storage.chunks(); }
            {
                return m_storage.chunks();
            }

            size_t chunk_count() const
                requires requires { m_storage.chunk_count(); }
            {
                return m_storage.chunk_count();
            }

            auto chunks() && = delete;

            auto chunks() const&& = delete;

            auto chunk(size_t i) &
                requires requires { m_storage.chunk(i); }
            {
                return m_storage.chunk(i);
            }

            auto chunk(size_t i) const&
                requires requires { m_storage.chunk(i); }
            {
                return m_storage.chunk(i);
            }

            auto chunk(size_t) && = delete;

            auto chunk(size_t) const&& = delete;

#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
            template<std::meta::info Member>
            auto column() & {
                return column<schema_type::template member_index<Member>()>();
            }

            template<std::meta::info Member>
            auto column() const& {
                return column<schema_type::template member_index<Member>()>();
            }

            template<std::meta::info Member>
            auto column() && = delete;

            template<std::meta::info Member>
            auto column() const&& = delete;
#endif

            // 添字は常に物理行。ID の検索は find に明示する。
            entry operator[](size_t index) { return {this, index}; }

            const_entry operator[](size_t index) const { return {this, index}; }

            entry at(size_t index) {
                if (index >= size())
                    throw std::out_of_range("Buffer row index");
                return (*this)[index];
            }

            const_entry at(size_t index) const {
                if (index >= size())
                    throw std::out_of_range("Buffer row index");
                return (*this)[index];
            }

            std::expected<size_t, error_code> index(const id_type& id) const {
                return m_storage.index(id);
            }

            std::expected<entry, error_code> find(const id_type& id) {
                auto position = index(id);
                if (!position)
                    return std::unexpected{position.error()};
                return (*this)[*position];
            }

            std::expected<const_entry, error_code> find(const id_type& id) const {
                auto position = index(id);
                if (!position)
                    return std::unexpected{position.error()};
                return (*this)[*position];
            }

            template<size_t N>
            auto get(const id_type& id) -> std::expected<
                lookup_value<decltype(std::declval<Storage&>().template column<N>()[size_t{}])>,
                error_code> {
                auto position = index(id);
                if (!position)
                    return std::unexpected{position.error()};
                return lookup_reference(column<N>()[*position]);
            }

            template<size_t N>
            auto get(const id_type& id) const
                -> std::expected<lookup_value<decltype(std::declval<const Storage&>()
                                         .template column<N>()[size_t{}])>,
                    error_code> {
                auto position = index(id);
                if (!position)
                    return std::unexpected{position.error()};
                return lookup_reference(column<N>()[*position]);
            }

            value_type read(size_t i) const
                requires std::is_copy_constructible_v<value_type>
            {
                return m_storage.read(i);
            }

            [[nodiscard]] std::expected<void, error_code> write(size_t i, value_type value) {
                if (i >= size())
                    throw std::out_of_range("Buffer row index");
                using before_update = typename notifications::before_update;
                if constexpr (observes<before_update, Events>) {
                    bool accepted = schema_type::apply_value(value, [&](auto&... fields) {
                        return dispatch_event<before_update>(
                            m_events, m_storage.ids[i], i, std::tie(fields...));
                    });
                    if (!accepted)
                        return std::unexpected{error_code::operation_canceled};
                }
                m_storage.write(i, std::move(value));
                notify_row<typename notifications::after_update>(
                    m_events, *this, i, m_storage.ids[i]);
                return {};
            }

            Events& events() { return m_events; }

            const Events& events() const { return m_events; }

        private:
            template<typename... Fields>
            using before_event = eventsystem::event::will_insert_event<id_type, Fields...>;
            template<typename... Fields>
            using after_event = eventsystem::event::did_insert_event<id_type, Fields...>;
            using before = typename element_types::template apply<before_event>;
            using after = typename element_types::template apply<after_event>;

            template<typename... Args>
            std::expected<void, error_code> insert(Args&&... args) {
                if (size() >= max_size())
                    return std::unexpected{error_code::buffer_over};
                auto id = m_generator.next();
                if (!id)
                    return std::unexpected{error_code::generate_id_failed};
                if constexpr (!Events::event_types::template contains<before> &&
                              !Events::event_types::template contains<after>) {
                    return m_storage.push_back(std::move(*id), std::forward<Args>(args)...);
                } else {
                    data_tuple values(std::forward<Args>(args)...);
                    if constexpr (Events::event_types::template contains<before>) {
                        auto references =
                            std::apply([](auto&... fields) { return std::tie(fields...); }, values);
                        before event{*id, references};
                        m_events.on(event);
                        if (event.canceled())
                            return std::unexpected{error_code::operation_canceled};
                    }
                    auto result = std::apply(
                        [&](auto&&... fields) {
                            return m_storage.push_back(
                                std::move(*id), std::forward<decltype(fields)>(fields)...);
                        },
                        std::move(values));
                    if (!result)
                        return result;
                    if constexpr (Events::event_types::template contains<after>) {
                        with_row_references<element_types>(*this, size() - 1, [&](auto references) {
                            after event{m_storage.ids[size() - 1], references};
                            m_events.on(event);
                        });
                    }
                    return {};
                }
            }

        public:
            std::expected<void, error_code> push_back(value_type value) {
                return schema_type::apply_value(std::move(value), [&](auto&&... fields) {
                    return insert(std::forward<decltype(fields)>(fields)...);
                });
            }

            template<typename... Args>
                requires(sizeof...(Args) == element_types::size &&
                         !(sizeof...(Args) == 1 &&
                             (std::same_as<std::remove_cvref_t<Args>, value_type> && ...)))
            std::expected<void, error_code> push_back(Args&&... args) {
                return insert(std::forward<Args>(args)...);
            }

            std::expected<void, error_code> resize(size_t count)
                requires std::default_initializable<value_type>
            {
                if (count > max_size())
                    return std::unexpected{error_code::buffer_over};
                if constexpr (observes<typename notifications::before_delete, Events> ||
                              observes<typename notifications::after_delete, Events>) {
                    if (count < size()) {
                        while (size() > count) {
                            auto result = pop_back();
                            if (!result)
                                return result;
                        }
                        return {};
                    }
                }
                if constexpr (observes<before, Events> || observes<after, Events>) {
                    if (count > size()) {
                        while (size() < count) {
                            auto result = push_back(value_type{});
                            if (!result)
                                return result;
                        }
                        return {};
                    }
                }
                return m_storage.resize_with_generator(count, m_generator);
            }

            [[nodiscard]] std::expected<void, error_code> pop_back() {
                if (empty())
                    return {};
                return erase_unordered(size() - 1);
            }

            [[nodiscard]] std::expected<void, error_code> clear() {
                if constexpr (observes<typename notifications::before_delete, Events> ||
                              observes<typename notifications::after_delete, Events>) {
                    while (!empty()) {
                        auto result = pop_back();
                        if (!result)
                            return result;
                    }
                } else {
                    m_storage.clear();
                }
                return {};
            }

            [[nodiscard]] std::expected<void, error_code> swap_rows(size_t left, size_t right) {
                if (left >= size() || right >= size())
                    throw std::out_of_range("Buffer row index");
                if (left == right)
                    return {};
                if (!notify_swap<typename notifications::before_swap>(m_events,
                        std::as_const(*this), left, right, m_storage.ids[left],
                        m_storage.ids[right]))
                    return std::unexpected{error_code::operation_canceled};
                auto result = m_storage.swap(left, right);
                if (!result)
                    return result;
                notify_swap<typename notifications::after_swap>(
                    m_events, *this, left, right, m_storage.ids[left], m_storage.ids[right]);
                return {};
            }

            [[nodiscard]] std::expected<void, error_code> erase_unordered(size_t index) {
                if (index >= size())
                    throw std::out_of_range("Buffer row index");
                if (!notify_row<typename notifications::before_delete>(
                        m_events, std::as_const(*this), index, m_storage.ids[index]))
                    return std::unexpected{error_code::operation_canceled};
                auto erase_row = [&] {
                    // 削除の内部移動は交換イベントを発火させない。
                    if (index != size() - 1)
                        (void)m_storage.swap(index, size() - 1);
                    m_storage.pop_back();
                };
                if constexpr (observes<typename notifications::after_delete, Events>) {
                    auto id = m_storage.ids[index];
                    erase_row();
                    dispatch_event<typename notifications::after_delete>(
                        m_events, std::move(id), index);
                } else {
                    erase_row();
                }
                return {};
            }

            std::expected<void, error_code> erase(const id_type& id) {
                auto position = index(id);
                if (!position)
                    return std::unexpected{position.error()};
                return erase_unordered(*position);
            }

            iterator begin() { return {this, 0}; }

            iterator end() { return {this, static_cast<ptrdiff_t>(size())}; }

            const_iterator begin() const { return {this, 0}; }

            const_iterator end() const { return {this, static_cast<ptrdiff_t>(size())}; }

            const_iterator cbegin() const { return begin(); }

            const_iterator cend() const { return end(); }
        };

    }

    // ================================ strategy ===============================
    namespace _detail {
        template<typename Layout, typename... Types>
        struct soa_layout_buffer {
            using type = soa_buffer<Layout, Types...>;
        };

        template<size_t Size, typename... Types>
        struct soa_layout_buffer<storage::chunked_layout<Size>, Types...> {
            using type = chunked_buffer<Size, Types...>;
        };

        struct empty_id_index {};
    }

    namespace strategy {
        struct structure_of_array {
            template<typename Layout, typename... Types>
            using container = typename _detail::soa_layout_buffer<Layout, Types...>::type;
        };

        // ID の生成とは独立して、検索用の索引を選ぶ。
        struct linear_id_lookup {
            static constexpr bool indexed = false;
            template<typename ID>
            using index = _detail::empty_id_index;
        };

        struct hash_id_lookup {
            static constexpr bool indexed = true;
            template<typename ID>
            using index = std::unordered_map<ID, size_t>;
        };
    }

    namespace identity {
        struct none {};

        template<generator::is_generator Generator = generator::iota,
            typename Lookup = strategy::hash_id_lookup>
        struct generated {
            using generator_type = Generator;
            using lookup_type = Lookup;
        };

    }

    namespace _detail {

        template<typename Rows>
        using rows_schema =
            std::conditional_t<std::same_as<typename Rows::value_type,
                                   typename Rows::element_types::template apply<std::tuple>>,
                typename Rows::element_types::template apply<schema>,
                schema<record<typename Rows::value_type>>>;

        template<typename Rows, bool Const, bool = rows_schema<Rows>::named>
        struct indexed_fields {
            using type = unnamed_fields;
            using parent_type = std::conditional_t<Const, const Rows, Rows>;

            static type get(parent_type&, size_t) { return {}; }
        };

#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
        // 配置方針が返す行の型に依存せず、各列の実際の参照型から行を生成する。
        template<typename Record, typename References>
        struct named_indexed_fields;

        template<typename Record, typename... References>
        struct named_indexed_fields<Record, type_list<References...>> {
            struct type;
            consteval {
                constexpr std::array<std::meta::info, sizeof...(References)> types{^^References...};
                auto members = std::meta::nonstatic_data_members_of(
                    ^^Record, std::meta::access_context::current());
                if (members.size() != types.size())
                    throw "Record and column counts must match";
                std::vector<std::meta::info> fields;
                if constexpr (sizeof...(References) != 0) {
                    for (size_t i = 0; i < types.size(); ++i)
                        fields.push_back(std::meta::data_member_spec(
                            types[i], {.name = std::meta::identifier_of(members[i])}));
                }
                std::meta::define_aggregate(^^type, fields);
            }
        };

        template<typename Rows, bool Const>
        struct indexed_fields<Rows, Const, true> {
            using parent_type = std::conditional_t<Const, const Rows, Rows>;
            using indices = std::make_index_sequence<Rows::element_types::size>;

            template<size_t... Indices>
            static auto reference_type(std::index_sequence<Indices...>) ->
                typename named_indexed_fields<typename rows_schema<Rows>::record_type,
                    type_list<decltype(std::declval<parent_type&>()
                            .template column<Indices>()[size_t{}])...>>::type;

            using type = decltype(reference_type(indices{}));

            static type get(parent_type& rows, size_t position) {
                return [&]<size_t... Indices>(std::index_sequence<Indices...>) {
                    return type{rows.template column<Indices>()[position]...};
                }(indices{});
            }
        };
#endif

        // 列の所有・配置は Rows に任せ、ID 列と検索索引だけを管理する。
        template<typename Rows, typename Layout, typename ID, typename Lookup>
        class indexed_storage {
            using index_type = typename Lookup::template index<ID>;
            Rows m_rows;
            [[no_unique_address]] index_type m_index;
            static_assert(std::is_nothrow_move_constructible_v<index_type> &&
                              std::is_nothrow_swappable_v<index_type>,
                "ID lookup indexes require noexcept move and swap");
            using data_tuple = typename Rows::element_types::template apply<std::tuple>;

            static_assert(
                std::is_copy_constructible_v<ID> && std::is_nothrow_move_constructible_v<ID> &&
                    std::is_nothrow_move_assignable_v<ID> && std::is_nothrow_swappable_v<ID>,
                "IDs require copy construction and noexcept move/swap");
            static_assert(!Lookup::indexed ||
                              (std::is_nothrow_invocable_v<std::hash<ID>, const ID&> &&
                                  noexcept(std::declval<const ID&>() == std::declval<const ID&>())),
                "Indexed ID lookup requires noexcept hashing and equality");

            static Rows take_rows(indexed_storage& other) noexcept(
                std::is_nothrow_move_constructible_v<Rows>) {
                if constexpr (std::is_nothrow_move_constructible_v<Rows>) {
                    return std::move(other.m_rows);
                } else {
                    try {
                        return std::move(other.m_rows);
                    } catch (...) {
                        other.clear();
                        throw;
                    }
                }
            }

            void append_id(ID id, size_t position) {
                if constexpr (Layout::resizable)
                    ids.push_back(std::move(id));
                else
                    ids[position] = std::move(id);
            }

            void discard_ids(size_t count) noexcept {
                if constexpr (Layout::resizable)
                    while (ids.size() > count)
                        ids.pop_back();
            }

        public:
            using schema_type = rows_schema<Rows>;
            using element_types = typename Rows::element_types;
            using id_type = ID;
            using value_type = typename Rows::value_type;
            using id_columns_type = std::conditional_t<Layout::resizable, std::vector<ID>,
                std::array<ID, Layout::max_size>>;
            static_assert(Layout::resizable || std::default_initializable<ID>,
                "Fixed-capacity identity storage requires default-constructible IDs");
            id_columns_type ids{};

            template<bool Const>
            using field_reference = typename indexed_fields<Rows, Const>::type;

            indexed_storage() = default;
            indexed_storage(const indexed_storage&) = default;

            indexed_storage(indexed_storage&& other) noexcept(
                std::is_nothrow_move_constructible_v<Rows> &&
                std::is_nothrow_move_constructible_v<decltype(ids)> &&
                std::is_nothrow_move_constructible_v<decltype(m_index)>)
                : m_rows(take_rows(other)), m_index(std::move(other.m_index)),
                  ids(std::move(other.ids)) {
                other.clear();
            }

            void swap(indexed_storage& other) noexcept(
                noexcept(m_rows.swap(other.m_rows)) && std::is_nothrow_swappable_v<decltype(ids)> &&
                std::is_nothrow_swappable_v<decltype(m_index)>) {
                if constexpr (noexcept(m_rows.swap(other.m_rows))) {
                    m_rows.swap(other.m_rows);
                } else {
                    try {
                        m_rows.swap(other.m_rows);
                    } catch (...) {
                        clear();
                        other.clear();
                        throw;
                    }
                }
                using std::swap;
                swap(ids, other.ids);
                swap(m_index, other.m_index);
            }

            indexed_storage& operator=(const indexed_storage& other)
                requires std::is_copy_constructible_v<Rows>
            {
                if (this != std::addressof(other)) {
                    indexed_storage copy(other);
                    swap(copy);
                }
                return *this;
            }

            indexed_storage& operator=(const indexed_storage&)
                requires(!std::is_copy_constructible_v<Rows>)
            = delete;

            indexed_storage& operator=(indexed_storage&& other) noexcept(
                std::is_nothrow_move_constructible_v<indexed_storage> && noexcept(swap(other))) {
                if (this != std::addressof(other)) {
                    indexed_storage moved(std::move(other));
                    swap(moved);
                }
                return *this;
            }

            size_t size() const { return m_rows.size(); }

            bool empty() const { return m_rows.empty(); }

            size_t row_limit() const {
                auto limit = m_rows.max_size();
                if constexpr (Layout::resizable)
                    limit = std::min(limit, ids.max_size());
                if constexpr (Lookup::indexed)
                    limit = std::min(limit, m_index.max_size());
                return limit;
            }

            void reserve(size_t count) {
                if (count > row_limit())
                    throw std::length_error("Buffer capacity exceeds max_size");
                m_rows.reserve(count);
                if constexpr (Layout::resizable)
                    ids.reserve(count);
                if constexpr (Lookup::indexed)
                    m_index.reserve(count);
            }

            size_t capacity() const { return m_rows.capacity(); }

            template<size_t N>
            auto column() {
                return m_rows.template column<N>();
            }

            template<size_t N>
            auto column() const {
                return m_rows.template column<N>();
            }

            auto columns()
                requires requires { m_rows.columns(); }
            {
                return m_rows.columns();
            }

            auto columns() const
                requires requires { m_rows.columns(); }
            {
                return m_rows.columns();
            }

            auto chunks()
                requires requires { m_rows.chunks(); }
            {
                return m_rows.chunks();
            }

            auto chunks() const
                requires requires { m_rows.chunks(); }
            {
                return m_rows.chunks();
            }

            size_t chunk_count() const
                requires requires { m_rows.chunk_count(); }
            {
                return m_rows.chunk_count();
            }

            auto chunk(size_t i)
                requires requires { m_rows.chunk(i); }
            {
                return m_rows.chunk(i);
            }

            auto chunk(size_t i) const
                requires requires { m_rows.chunk(i); }
            {
                return m_rows.chunk(i);
            }

            auto fields(size_t i) { return indexed_fields<Rows, false>::get(m_rows, i); }

            auto fields(size_t i) const { return indexed_fields<Rows, true>::get(m_rows, i); }

            value_type read(size_t i) const
                requires std::is_copy_constructible_v<value_type>
            {
                return m_rows.read(i);
            }

            void write(size_t i, value_type value) { m_rows.write(i, std::move(value)); }

            std::expected<size_t, error_code> index(const ID& id) const {
                if constexpr (Lookup::indexed) {
                    auto found = m_index.find(id);
                    if (found != m_index.end())
                        return found->second;
                } else {
                    for (size_t i = 0; i < size(); ++i)
                        if (ids[i] == id)
                            return i;
                }
                return std::unexpected{error_code::not_found_id};
            }

            template<typename... Args>
            std::expected<void, error_code> push_back(ID id, Args&&... args) {
                if (size() >= row_limit())
                    return std::unexpected{error_code::buffer_over};
                if constexpr (!Lookup::indexed) {
                    if (index(id))
                        return std::unexpected{error_code::duplicate_id};
                }
                data_tuple values(std::forward<Args>(args)...);
                const auto previous = size();
                if constexpr (Layout::resizable) {
                    if (ids.size() == ids.capacity())
                        ids.reserve(size() + std::min(row_limit() - size(), size() / 2 + 1));
                }
                std::apply(
                    [&](auto&&... fields) {
                        m_rows.push_back(std::forward<decltype(fields)>(fields)...);
                    },
                    std::move(values));
                try {
                    append_id(std::move(id), previous);
                    if constexpr (Lookup::indexed) {
                        if (!m_index.emplace(ids[previous], previous).second) {
                            discard_ids(previous);
                            m_rows.pop_back();
                            return std::unexpected{error_code::duplicate_id};
                        }
                    }
                } catch (...) {
                    discard_ids(previous);
                    m_rows.pop_back();
                    throw;
                }
                return {};
            }

            void pop_back() noexcept {
                if (empty())
                    return;
                if constexpr (Lookup::indexed)
                    m_index.erase(ids[size() - 1]);
                discard_ids(size() - 1);
                m_rows.pop_back();
            }

            void clear() noexcept {
                if constexpr (Lookup::indexed)
                    m_index.clear();
                discard_ids(0);
                m_rows.clear();
            }

            std::expected<void, error_code> swap(size_t left, size_t right) {
                if (left >= size() || right >= size())
                    return std::unexpected{error_code::buffer_over};
                if (left == right)
                    return {};
                [&]<size_t... Indices>(std::index_sequence<Indices...>) {
                    (
                        [&] {
                            auto view = column<Indices>();
                            auto first = std::ranges::begin(view);
                            std::ranges::iter_swap(first + left, first + right);
                        }(),
                        ...);
                }(std::make_index_sequence<Rows::element_types::size>{});
                using std::swap;
                swap(ids[left], ids[right]);
                if constexpr (Lookup::indexed) {
                    m_index.find(ids[left])->second = left;
                    m_index.find(ids[right])->second = right;
                }
                return {};
            }

            template<typename Generator>
            std::expected<void, error_code> resize_with_generator(
                size_t count, Generator& generator) {
                if (count > row_limit())
                    return std::unexpected{error_code::buffer_over};
                if (count <= size()) {
                    while (size() > count)
                        pop_back();
                    return {};
                }
                const auto previous = size();
                std::vector<ID> generated;
                generated.reserve(count - previous);
                for (size_t i = previous; i < count; ++i) {
                    auto id = generator.next();
                    if (!id)
                        return std::unexpected{error_code::generate_id_failed};
                    generated.push_back(std::move(*id));
                }
                reserve(count);
                m_rows.resize(count);
                auto indexed = previous;
                auto rollback = [&] {
                    if constexpr (Lookup::indexed)
                        for (size_t i = previous; i < indexed; ++i)
                            m_index.erase(ids[i]);
                    discard_ids(previous);
                    m_rows.resize(previous);
                };
                try {
                    for (auto& id : generated) {
                        if constexpr (!Lookup::indexed) {
                            for (size_t i = 0; i < indexed; ++i) {
                                if (ids[i] == id) {
                                    rollback();
                                    return std::unexpected{error_code::duplicate_id};
                                }
                            }
                        }
                        append_id(std::move(id), indexed);
                        if constexpr (Lookup::indexed) {
                            if (!m_index.emplace(ids[indexed], indexed).second) {
                                rollback();
                                return std::unexpected{error_code::duplicate_id};
                            }
                        }
                        ++indexed;
                    }
                } catch (...) {
                    rollback();
                    throw;
                }
                return {};
            }
        };

        template<typename Rows, typename Events>
        class unidentified_buffer : public Rows {
        public:
            using element_types = typename Rows::element_types;

        private:
            [[no_unique_address]] Events m_events;
            using data_tuple = typename element_types::template apply<std::tuple>;
            using notifications = mutation_events<element_types>;
            using before =
                typename element_types::template apply<eventsystem::event::will_insert_row_event>;
            using after =
                typename element_types::template apply<eventsystem::event::did_insert_row_event>;

            template<typename... Args>
            std::expected<void, error_code> insert(Args&&... args) {
                if (this->size() >= this->max_size())
                    return std::unexpected{error_code::buffer_over};
                if constexpr (!Events::event_types::template contains<before> &&
                              !Events::event_types::template contains<after>) {
                    Rows::push_back(std::forward<Args>(args)...);
                    return {};
                } else {
                    data_tuple values(std::forward<Args>(args)...);
                    if constexpr (Events::event_types::template contains<before>) {
                        auto references =
                            std::apply([](auto&... fields) { return std::tie(fields...); }, values);
                        before event{this->size(), references};
                        m_events.on(event);
                        if (event.canceled())
                            return std::unexpected{error_code::operation_canceled};
                    }
                    std::apply(
                        [&](auto&&... fields) {
                            Rows::push_back(std::forward<decltype(fields)>(fields)...);
                        },
                        std::move(values));
                    if constexpr (Events::event_types::template contains<after>) {
                        with_row_references<element_types>(
                            *this, this->size() - 1, [&](auto references) {
                                after event{this->size() - 1, references};
                                m_events.on(event);
                            });
                    }
                    return {};
                }
            }

        public:
            static constexpr bool has_identity = false;

            Events& events() { return m_events; }

            const Events& events() const { return m_events; }

            std::expected<void, error_code> push_back(typename Rows::value_type value) {
                using schema_type = rows_schema<Rows>;
                return schema_type::apply_value(std::move(value), [&](auto&&... fields) {
                    return insert(std::forward<decltype(fields)>(fields)...);
                });
            }

            template<typename... Args>
                requires(sizeof...(Args) == element_types::size &&
                         !(sizeof...(Args) == 1 &&
                             (std::same_as<std::remove_cvref_t<Args>, typename Rows::value_type> &&
                                 ...)))
            std::expected<void, error_code> push_back(Args&&... args) {
                return insert(std::forward<Args>(args)...);
            }

            std::expected<void, error_code> resize(size_t count)
                requires std::default_initializable<typename Rows::value_type>
            {
                if (count > this->max_size())
                    return std::unexpected{error_code::buffer_over};
                if constexpr (observes<typename notifications::before_delete, Events> ||
                              observes<typename notifications::after_delete, Events>) {
                    if (count < this->size()) {
                        while (this->size() > count) {
                            auto result = pop_back();
                            if (!result)
                                return result;
                        }
                        return {};
                    }
                }
                if constexpr (observes<before, Events> || observes<after, Events>) {
                    if (count > this->size()) {
                        while (this->size() < count) {
                            auto result = push_back(typename Rows::value_type{});
                            if (!result)
                                return result;
                        }
                        return {};
                    }
                }
                Rows::resize(count);
                return {};
            }

            [[nodiscard]] std::expected<void, error_code> write(
                size_t i, typename Rows::value_type value) {
                if (i >= this->size())
                    throw std::out_of_range("Buffer row index");
                using before_update = typename notifications::before_update;
                if constexpr (observes<before_update, Events>) {
                    bool accepted = rows_schema<Rows>::apply_value(value, [&](auto&... fields) {
                        return dispatch_event<before_update>(m_events, i, std::tie(fields...));
                    });
                    if (!accepted)
                        return std::unexpected{error_code::operation_canceled};
                }
                Rows::write(i, std::move(value));
                notify_row<typename notifications::after_update>(m_events, *this, i);
                return {};
            }

            [[nodiscard]] std::expected<void, error_code> swap_rows(size_t left, size_t right) {
                if (left >= this->size() || right >= this->size())
                    throw std::out_of_range("Buffer row index");
                if (left == right)
                    return {};
                if (!notify_swap<typename notifications::before_swap>(
                        m_events, std::as_const(*this), left, right))
                    return std::unexpected{error_code::operation_canceled};
                swap_row_values(*this, left, right);
                notify_swap<typename notifications::after_swap>(m_events, *this, left, right);
                return {};
            }

            [[nodiscard]] std::expected<void, error_code> erase_unordered(size_t index) {
                if (index >= this->size())
                    throw std::out_of_range("Buffer row index");
                if (!notify_row<typename notifications::before_delete>(
                        m_events, std::as_const(*this), index))
                    return std::unexpected{error_code::operation_canceled};
                Rows::erase_unordered(index);
                dispatch_event<typename notifications::after_delete>(m_events, index);
                return {};
            }

            [[nodiscard]] std::expected<void, error_code> pop_back() {
                if (this->empty())
                    return {};
                return erase_unordered(this->size() - 1);
            }

            [[nodiscard]] std::expected<void, error_code> clear() {
                if constexpr (observes<typename notifications::before_delete, Events> ||
                              observes<typename notifications::after_delete, Events>) {
                    while (!this->empty()) {
                        auto result = pop_back();
                        if (!result)
                            return result;
                    }
                } else {
                    Rows::clear();
                }
                return {};
            }

            void swap(unidentified_buffer& other) noexcept(
                noexcept(std::declval<Rows&>().swap(other)) &&
                std::is_nothrow_swappable_v<Events>) {
                Rows::swap(other);
                using std::swap;
                swap(m_events, other.m_events);
            }

            friend void swap(unidentified_buffer& a, unidentified_buffer& b) noexcept(
                noexcept(a.swap(b))) {
                a.swap(b);
            }
        };

        template<typename Structure, typename Layout, typename Identity, typename Events,
            typename... Types>
        struct buffer_type;

        template<typename Structure, typename Layout, typename Events, typename... Types>
        struct buffer_type<Structure, Layout, identity::none, Events, Types...> {
            using rows = typename Structure::template container<Layout, Types...>;
            using type = unidentified_buffer<rows, Events>;
        };

        template<typename Structure, typename Layout, typename Generator, typename Lookup,
            typename Events, typename... Types>
        struct buffer_type<Structure, Layout, identity::generated<Generator, Lookup>, Events,
            Types...> {
            using type = identified_buffer<
                indexed_storage<typename Structure::template container<Layout, Types...>, Layout,
                    typename Generator::generate_type, Lookup>,
                Generator, Events>;
        };

        template<typename Schema>
        struct schema_arguments {
            using type = type_list<record<Schema>>;
        };

        template<typename Record>
        struct schema_arguments<record<Record>> {
            using type = type_list<record<Record>>;
        };

        template<typename... Types>
        struct schema_arguments<type_list<Types...>> {
            using type = type_list<Types...>;
        };

        template<typename Structure, typename Layout, typename Identity, typename Events,
            typename Arguments>
        struct select_buffer;

        template<typename Structure, typename Layout, typename Identity, typename Events,
            typename... Types>
        struct select_buffer<Structure, Layout, Identity, Events, type_list<Types...>>
            : buffer_type<Structure, Layout, Identity, Events, Types...> {};
    }

    template<typename Schema = type_list<>, typename Structure = strategy::structure_of_array,
        typename Layout = storage::dynamic_layout<>, typename Identity = identity::none,
        eventsystem::is_eventsystem Events = eventsystem::dummy>
    class buffer : public _detail::select_buffer<Structure, Layout, Identity, Events,
                       typename _detail::schema_arguments<Schema>::type>::type {
        using base_type = typename _detail::select_buffer<Structure, Layout, Identity, Events,
            typename _detail::schema_arguments<Schema>::type>::type;

    public:
        using base_type::base_type;
        using schema_type = Schema;
        using structure_type = Structure;
        using layout_type = Layout;
        using identity_type = Identity;
        using event_system_type = Events;

        friend void swap(buffer& a, buffer& b) noexcept(noexcept(a.swap(b))) { a.swap(b); }
    };

    template<typename Layout, typename... Types>
    using basic_soa = buffer<type_list<Types...>, strategy::structure_of_array, Layout>;

    template<typename... Types>
    using soa = basic_soa<storage::dynamic_layout<>, Types...>;

    template<size_t Size, typename... Types>
    using fixed_soa = basic_soa<storage::fixed_layout<Size>, Types...>;

    template<size_t Size, typename... Types>
    using chunked_soa = basic_soa<storage::chunked_layout<Size>, Types...>;

#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
    template<typename Record, typename Layout = storage::dynamic_layout<>>
    using record_soa = buffer<Record, strategy::structure_of_array, Layout>;

    template<typename Record, size_t Size = 128>
    using record_chunked_soa =
        buffer<Record, strategy::structure_of_array, storage::chunked_layout<Size>>;
#endif

    // ================================== rows =================================
    // 列ストレージ単体の行ビュー
#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
    namespace storage {
        namespace reflection_detail {
            template<typename Columns>
            class row_iterator {
                template<typename> friend class row_iterator;

                Columns* m_columns = nullptr;
                std::ptrdiff_t m_index = 0;

            public:
                using iterator_concept = std::random_access_iterator_tag;
                // 間接参照はプロキシ値を返す。C++20 ranges は iterator_concept を参照する。
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

                constexpr reference operator[](difference_type offset) const {
                    return *(*this + offset);
                }

                constexpr row_iterator& operator++() { ++m_index; return *this; }

                constexpr row_iterator operator++(int) { auto old = *this; ++*this; return old; }

                constexpr row_iterator& operator--() { --m_index; return *this; }

                constexpr row_iterator operator--(int) { auto old = *this; --*this; return old; }

                constexpr row_iterator& operator+=(difference_type offset) {
                    m_index += offset;
                    return *this;
                }

                constexpr row_iterator& operator-=(difference_type offset) {
                    m_index -= offset;
                    return *this;
                }

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

                friend constexpr bool operator==(const row_iterator&,
                                                 const row_iterator&) = default;

                friend constexpr auto operator<=>(const row_iterator&,
                                                  const row_iterator&) = default;
            };

            template<typename Columns>
            class rows_view : public std::ranges::view_interface<rows_view<Columns>> {
                Columns* m_columns = nullptr;
                std::size_t m_size = 0;

            public:
                using iterator = row_iterator<Columns>;

                constexpr rows_view() = default;

                constexpr explicit rows_view(Columns& columns)
                    : m_columns(std::addressof(columns)),
                      m_size(row_access<Columns>::size(columns)) {}

                constexpr iterator begin() const { return {m_columns, 0}; }

                constexpr iterator end() const {
                    return {m_columns, static_cast<std::ptrdiff_t>(m_size)};
                }

                constexpr std::size_t size() const { return m_size; }
            };
        }

        // 列要素への参照・プロキシを、同名のメンバを持つ行として返す。
        // 最短の列長を行数とし、列のないスキーマは 0 行とする。
        template<typename Columns>
        [[nodiscard]] constexpr auto rows(Columns& columns) {
            return reflection_detail::rows_view<Columns>(columns);
        }

        template<typename Columns>
        void rows(const Columns&&) = delete;
    }
#endif

        }

// ============================== std::ranges ==============================
// 行ビューが一時オブジェクトでも、列ストレージが生きていればイテレータを利用できる。
#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
namespace std::ranges {
    template<typename Columns>
    inline constexpr bool enable_borrowed_range<
        ::SAITEKIKUUKAN_NAMESPACE::storage::reflection_detail::rows_view<Columns>> = true;
}
#endif

#endif // SAITEKIKUUKAN_HPP
