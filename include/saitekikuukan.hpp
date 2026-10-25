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
        operation_canceled
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
                will_swap_event(IDType &id, std::tuple<Types &...> first,
                                std::tuple<Types &...> second)
                    : id(id), first(first), second(second) {};

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
                did_swap_event(IDType id, std::tuple<Types &...> first,
                               std::tuple<Types &...> second)
                    : id(id), first(first), second(second) {};

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
         * リスナを保持せず、通知時に一時生成する。
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
         * 状態を持つリスナだけを保持し、空のリスナは通知時に生成する。
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
                constexpr auto not_empty_indices = not_empty_types::template filtered_index<
                    helper::event_targeting<Event>::is_subscribe>();
                apply_class<Event, 0>(e);
                apply_entities<Event, not_empty_indices.size(), 0, not_empty_types>(e);
            };
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
            static_assert((!is_record<Types> && ...), "record<T> must be the only schema argument");
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

        template<typename Layout, typename IDType, typename... Types>
        struct identified_storage : basic_columns<Layout, Types...> {
            using base_type = basic_columns<Layout, Types...>;
            using id_type = IDType;
            // buffer が容量 1 の型で既定値を問い合わせる従来の契約を保つ。
            static constexpr size_t default_max_size =
                Layout::resizable ? DynamicSizeUnlimited : 1024;
            static constexpr size_t max_size = Layout::max_size;
            using id_columns_type = typename Layout::template column<id_type>;
            id_columns_type ids{};

        private:
            static auto take_ids(identified_storage& other) noexcept(
                std::is_nothrow_move_constructible_v<id_columns_type>) -> id_columns_type {
                if constexpr (std::is_nothrow_move_constructible_v<id_columns_type>) {
                    return std::move(other.ids);
                } else {
                    try {
                        if constexpr (std::is_copy_constructible_v<id_type>) return other.ids;
                        else return std::move(other.ids);
                    } catch (...) {
                        if constexpr (Layout::resizable) other.ids.clear();
                        throw;
                    }
                }
            }

        public:
            identified_storage() = default;

            identified_storage(const identified_storage&) = default;

            identified_storage(identified_storage&& other) noexcept(
                std::is_nothrow_move_constructible_v<base_type> &&
                std::is_nothrow_move_constructible_v<decltype(ids)>)
                : base_type(std::move(other)), ids(take_ids(other)) {
                if constexpr (Layout::resizable) other.ids.clear();
            }

            void swap(identified_storage& other) noexcept(
                base_type::nothrow_swap && std::is_nothrow_swappable_v<decltype(ids)>) {
                base_type::swap(other);
                using std::swap;
                swap(ids, other.ids);
            }

            identified_storage& operator=(const identified_storage& other)
                requires std::is_copy_constructible_v<base_type> {
                if (this != std::addressof(other)) {
                    identified_storage copy(other);
                    swap(copy);
                }
                return *this;
            }

            identified_storage& operator=(const identified_storage&)
                requires (!std::is_copy_constructible_v<base_type>) = delete;

            identified_storage& operator=(identified_storage&& other) noexcept(
                std::is_nothrow_move_constructible_v<base_type> &&
                std::is_nothrow_move_constructible_v<decltype(ids)> &&
                base_type::nothrow_swap && std::is_nothrow_swappable_v<decltype(ids)>) {
                identified_storage moved(std::move(other));
                swap(moved);
                return *this;
            }

            std::expected<void, error_code> resize(size_t count) {
                if (count > base_type::max_size()) return std::unexpected{error_code::buffer_over};
                if constexpr (Layout::resizable) {
                    auto old_size = this->size();
                    ids.resize(count);
                    try { base_type::resize(count); }
                    catch (...) {
                        while (ids.size() > old_size) ids.pop_back();
                        throw;
                    }
                } else {
                    base_type::resize(count);
                }
                return {};
            }

            template<typename... Args>
            std::expected<void, error_code> push_back(id_type id, Args&&... args) {
                if (this->size() >= base_type::max_size())
                    return std::unexpected{error_code::buffer_over};
                typename base_type::data_tuple values(std::forward<Args>(args)...);
                if constexpr (Layout::resizable) {
                    // ID 列から値を渡された場合にも、値を退避してから再確保する。
                    if (ids.size() == ids.max_size())
                        return std::unexpected{error_code::buffer_over};
                    if (ids.size() == ids.capacity())
                        ids.reserve(ids.size() +
                                    std::min(ids.max_size() - ids.size(), ids.size() / 2 + 1));
                    this->append_values(values);
                    try { ids.push_back(std::move_if_noexcept(id)); }
                    catch (...) { base_type::pop_back(); throw; }
                } else {
                    ids[this->size()] = id;
                    this->append_values(values);
                }
                return {};
            }

            void pop_back() noexcept {
                if (this->empty()) return;
                if constexpr (Layout::resizable) ids.pop_back();
                base_type::pop_back();
            }

            void clear() noexcept {
                if constexpr (Layout::resizable) ids.clear();
                base_type::clear();
            }

            std::expected<void, error_code> swap(size_t left, size_t right) {
                if (left >= this->size() || right >= this->size())
                    return std::unexpected{error_code::buffer_over};
                this->swap_rows(left, right);
                using std::swap;
                swap(ids[left], ids[right]);
                return {};
            }

            std::expected<size_t, error_code> index(id_type id) const {
                for (size_t i = 0; i < this->size(); ++i) if (ids[i] == id) return i;
                return std::unexpected{error_code::not_found_id};
            }
        };

        template<typename IDType, size_t MaxSize = DynamicSizeUnlimited, typename... Types>
        using dynamic_storage = identified_storage<dynamic_layout<MaxSize>, IDType, Types...>;

        template<typename IDType, size_t MaxSize = 1024, typename... Types>
        using static_storage = identified_storage<fixed_layout<MaxSize>, IDType, Types...>;

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
            template<typename, typename> friend class indexed_iterator;
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

            template<typename Other> requires std::is_convertible_v<Other*, Parent*>
            constexpr indexed_iterator(const indexed_iterator<Other, Access>& other)
                : m_parent(other.m_parent), m_index(other.m_index) {}

            constexpr reference operator*() const {
                return Access::get(*m_parent, static_cast<size_t>(m_index));
            }

            constexpr pointer operator->() const {
                if constexpr (std::is_lvalue_reference_v<reference>) return std::addressof(**this);
                else return {**this};
            }

            constexpr reference operator[](difference_type offset) const {
                return *(*this + offset);
            }

            constexpr indexed_iterator& operator++() { ++m_index; return *this; }

            constexpr indexed_iterator operator++(int) { auto old = *this; ++*this; return old; }

            constexpr indexed_iterator& operator--() { --m_index; return *this; }

            constexpr indexed_iterator operator--(int) { auto old = *this; --*this; return old; }

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

            friend constexpr bool operator==(const indexed_iterator&,
                                             const indexed_iterator&) = default;

            friend constexpr auto operator<=>(const indexed_iterator&,
                                              const indexed_iterator&) = default;
        };

        template<typename Access, typename Parent>
        auto indexed_range(Parent& parent, size_t count) {
            using iterator = indexed_iterator<Parent, Access>;
            return std::ranges::subrange(
                iterator{std::addressof(parent), 0},
                iterator{std::addressof(parent), static_cast<ptrdiff_t>(count)});
        }
    }

    // ================================== soa ==================================
    // 所有列を隠し、行数と列長を一体で管理する ID なしのコンテナ。
    // 列ビューの要素は更新できるが、ビューから所有列の長さは変更できない。
    template<typename Layout, typename... Types>
    class basic_soa {
        using storage_type = storage::basic_columns<Layout, Types...>;
        using schema_type = typename storage_type::schema_type;
        storage_type m_storage;

        template<bool Const>
        using parent_type = std::conditional_t<Const, const basic_soa, basic_soa>;

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

        template<typename... Args> requires (sizeof...(Args) == element_types::size &&
            !(sizeof...(Args) == 1 && (std::same_as<std::remove_cvref_t<Args>, value_type> && ...)))
        void push_back(Args&&... args) { m_storage.push_back(std::forward<Args>(args)...); }

        // 参照プロキシは行の値そのものではない。値のコピー・代入は read/write で明示する。
        auto operator[](size_t index) { return m_storage.row(index); }

        auto operator[](size_t index) const { return m_storage.row(index); }

        auto at(size_t index) {
            if (index >= size()) throw std::out_of_range("SoA row index");
            return (*this)[index];
        }

        auto at(size_t index) const {
            if (index >= size()) throw std::out_of_range("SoA row index");
            return (*this)[index];
        }

        value_type read(size_t index) const requires std::is_copy_constructible_v<value_type> {
            return m_storage.read(index);
        }

        void write(size_t index, value_type value) { m_storage.write(index, std::move(value)); }

        void erase_unordered(size_t index) {
            if (index >= size()) throw std::out_of_range("SoA row index");
            if (index != size() - 1) m_storage.swap_rows(index, size() - 1);
            pop_back();
        }

        void swap(basic_soa& other) noexcept(storage_type::nothrow_swap) {
            m_storage.swap(other.m_storage);
        }

        friend void swap(basic_soa& a, basic_soa& b) noexcept(storage_type::nothrow_swap) {
            a.swap(b);
        }

        template<size_t N> auto column() & { return m_storage.template column<N>(); }

        template<size_t N> auto column() const & { return m_storage.template column<N>(); }

        template<size_t N> auto column() && = delete;

        template<size_t N> auto column() const && = delete;

        template<size_t N> auto get() & { return column<N>(); }

        template<size_t N> auto get() const & { return column<N>(); }

        template<size_t N> auto get() && = delete;

        template<size_t N> auto get() const && = delete;

        auto columns() & { return m_storage.columns(); }

        auto columns() const & { return m_storage.columns(); }

        auto columns() && = delete;

        auto columns() const && = delete;

#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
        template<std::meta::info Member> auto column() & {
            return column<schema_type::template member_index<Member>()>();
        }

        template<std::meta::info Member> auto column() const & {
            return column<schema_type::template member_index<Member>()>();
        }

        template<std::meta::info Member> auto column() && = delete;

        template<std::meta::info Member> auto column() const && = delete;
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

    template<typename... Types>
    using soa = basic_soa<storage::dynamic_layout<>, Types...>;
    template<size_t Size, typename... Types>
    using fixed_soa = basic_soa<storage::fixed_layout<Size>, Types...>;
#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
    template<typename Record, typename Layout = storage::dynamic_layout<>>
    using record_soa = basic_soa<Layout, record<Record>>;
#endif

    // ================================ chunked ================================
    // 固定長 SoA をブロックとして保持する。ポインタ表の拡張で既存のチャンクは移動しない。
    template<size_t ChunkSize, typename... Types>
    class chunked_soa {
        static_assert(ChunkSize > 0 &&
                          ChunkSize <= static_cast<size_t>(std::numeric_limits<ptrdiff_t>::max()),
            "ChunkSize must be positive and fit ptrdiff_t");
        using schema_type = _detail::schema<Types...>;

    public:
        static constexpr size_t chunk_size = ChunkSize;
        using chunk_type = fixed_soa<ChunkSize, Types...>;
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
        chunk_type& chunk_for_row(size_t index) {
            return *m_chunks[index / ChunkSize];
        }

        const chunk_type& chunk_for_row(size_t index) const {
            return *m_chunks[index / ChunkSize];
        }

        void check_row_index(size_t index) const {
            if (index >= size()) throw std::out_of_range("Chunked SoA row index");
        }

        void check_chunk_index(size_t index) const {
            if (index >= chunk_count()) throw std::out_of_range("Chunk index");
        }

        void append_chunk() {
            if (size() == max_size()) throw std::length_error("Chunked SoA exceeds max_size");
            m_chunks.push_back(std::make_unique<chunk_type>());
        }

        void insert(data_tuple& values) {
            if (size() == capacity()) [[unlikely]] append_chunk();
            std::apply([&](auto&&... fields) {
                chunk_for_row(m_size).push_back(std::forward<decltype(fields)>(fields)...);
            }, std::move(values));
            ++m_size;
        }

        void truncate(size_t count) noexcept {
            auto remaining = required_chunks(count);
            for (size_t i = remaining; i < chunk_count(); ++i) m_chunks[i]->clear();
            if (remaining) {
                auto& tail = *m_chunks[remaining - 1];
                auto tail_size = count - (remaining - 1) * ChunkSize;
                while (tail.size() > tail_size) tail.pop_back();
            }
            m_size = count;
        }

        template<size_t Column> decltype(auto) column_at(size_t index) {
            return chunk_for_row(index).template column<Column>()[index % ChunkSize];
        }

        template<size_t Column> decltype(auto) column_at(size_t index) const {
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
            static auto get(Parent& parent, size_t index) { return parent.chunk(index); }
        };

    public:
        // 要素の更新だけを許可し、個別チャンクの resize による行数の不一致を防ぐ。
        template<bool Const>
        class basic_chunk_view {
            friend class chunked_soa;
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
            template<size_t N> auto get() const { return column<N>(); }

            auto operator[](size_t index) const { return (*m_chunk)[index]; }

            auto at(size_t index) const { return m_chunk->at(index); }

            value_type read(size_t index) const requires std::is_copy_constructible_v<value_type> {
                return m_chunk->read(index);
            }

            void write(size_t index, value_type value) const requires (!Const) {
                m_chunk->write(index, std::move(value));
            }

            auto begin() const { return m_chunk->begin(); }

            auto end() const { return m_chunk->end(); }
        };

        using chunk_view = basic_chunk_view<false>;
        using const_chunk_view = basic_chunk_view<true>;

        // ========================== ownership ===========================
        chunked_soa() = default;

        chunked_soa(const chunked_soa& other) requires std::is_copy_constructible_v<chunk_type>
            : m_size(other.m_size) {
            m_chunks.reserve(other.m_chunks.size());
            for (const auto& chunk : other.m_chunks)
                m_chunks.push_back(std::make_unique<chunk_type>(std::as_const(*chunk)));
        }

        chunked_soa(const chunked_soa&)
            requires (!std::is_copy_constructible_v<chunk_type>) = delete;

        chunked_soa(chunked_soa&& other) noexcept
            : m_chunks(std::move(other.m_chunks)), m_size(std::exchange(other.m_size, 0)) {
            other.m_chunks.clear();
        }

        chunked_soa& operator=(const chunked_soa& other)
            requires std::is_copy_constructible_v<chunk_type>
        {
            if (this != std::addressof(other)) {
                chunked_soa copy(other);
                swap(copy);
            }
            return *this;
        }

        chunked_soa& operator=(const chunked_soa&)
            requires (!std::is_copy_constructible_v<chunk_type>) = delete;

        chunked_soa& operator=(chunked_soa&& other) noexcept {
            if (this != std::addressof(other)) {
                m_chunks = std::move(other.m_chunks);
                m_size = std::exchange(other.m_size, 0);
                other.m_chunks.clear();
            }
            return *this;
        }

        void swap(chunked_soa& other) noexcept {
            m_chunks.swap(other.m_chunks);
            std::swap(m_size, other.m_size);
        }

        friend void swap(chunked_soa& left, chunked_soa& right) noexcept { left.swap(right); }

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
            while (m_chunks.size() < desired) m_chunks.push_back(std::make_unique<chunk_type>());
        }

        void resize(size_t count) requires std::default_initializable<value_type> {
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
            if (empty()) return;
            chunk_for_row(m_size - 1).pop_back();
            --m_size;
        }

        void shrink_to_fit() {
            while (m_chunks.size() > chunk_count()) m_chunks.pop_back();
            m_chunks.shrink_to_fit();
        }

        // ============================= rows =============================
        void push_back(value_type value) {
            schema_type::apply_value(std::move(value), [&](auto&&... fields) {
                push_back(std::forward<decltype(fields)>(fields)...);
            });
        }

        template<typename... Args> requires (sizeof...(Args) == element_types::size &&
            !(sizeof...(Args) == 1 && (std::same_as<std::remove_cvref_t<Args>, value_type> && ...)))
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

        value_type read(size_t index) const requires std::is_copy_constructible_v<value_type> {
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
            if (index != last) swap_rows(index, last);
            pop_back();
        }

        // ============================= views ============================
        chunk_view chunk(size_t index) & {
            check_chunk_index(index);
            return chunk_view(m_chunks[index].get());
        }

        const_chunk_view chunk(size_t index) const & {
            check_chunk_index(index);
            return const_chunk_view(m_chunks[index].get());
        }

        chunk_view chunk(size_t) && = delete;

        const_chunk_view chunk(size_t) const && = delete;

        auto chunks() & {
            return _detail::indexed_range<chunk_access>(*this, chunk_count());
        }

        auto chunks() const & {
            return _detail::indexed_range<chunk_access>(*this, chunk_count());
        }

        auto chunks() && = delete;

        auto chunks() const && = delete;

        template<size_t Column> auto column() & {
            return _detail::indexed_range<column_access<Column>>(*this, size());
        }

        template<size_t Column> auto column() const & {
            return _detail::indexed_range<column_access<Column>>(*this, size());
        }

        template<size_t Column> auto column() && = delete;

        template<size_t Column> auto column() const && = delete;
#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
        template<std::meta::info Member>
        auto column() & {
            return column<schema_type::template member_index<Member>()>();
        }

        template<std::meta::info Member>
        auto column() const & {
            return column<schema_type::template member_index<Member>()>();
        }

        template<std::meta::info Member> auto column() && = delete;

        template<std::meta::info Member> auto column() const && = delete;
#endif
        template<size_t N> auto get() & { return column<N>(); }

        template<size_t N> auto get() const & { return column<N>(); }

        template<size_t N> auto get() && = delete;

        template<size_t N> auto get() const && = delete;

        using iterator = _detail::indexed_iterator<chunked_soa>;
        using const_iterator = _detail::indexed_iterator<const chunked_soa>;

        iterator begin() { return {this, 0}; }

        iterator end() { return {this, static_cast<ptrdiff_t>(size())}; }

        const_iterator begin() const { return {this, 0}; }

        const_iterator end() const { return {this, static_cast<ptrdiff_t>(size())}; }

        const_iterator cbegin() const { return begin(); }

        const_iterator cend() const { return end(); }
    };

#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
    template<typename Record, size_t ChunkSize = 128>
    using record_chunked_soa = chunked_soa<ChunkSize, record<Record>>;
#endif


    // ================================= buffer ================================
    // ID・イベント・有効行数を管理する共通の buffer
    template<generator::is_generator IDGenerator = generator::iota,
            eventsystem::is_eventsystem EventSystem = eventsystem::dummy,
            template<typename, size_t, typename...> typename Storage = storage::static_storage,
            size_t MaxSize = Storage<size_t, 1, int>::default_max_size,
            typename... Types> requires storage::is_storage<Storage>
    struct buffer {
        using this_type = buffer<IDGenerator, EventSystem, Storage, MaxSize, Types...>;
        using schema_type = _detail::schema<Types...>;
        static_assert([] { schema_type::validate_buffer_names(); return true; }());
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

        // 型パック形式では空の基底、record 形式では同名メンバへの参照を継承する。
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

        // const 性だけを切り替え、通常・const イテレータで同じ操作を共有する。
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

            friend basic_iterator operator+(basic_iterator it, difference_type offset) {
                return it += offset;
            }

            friend basic_iterator operator+(difference_type offset, basic_iterator it) {
                return it += offset;
            }

            friend basic_iterator operator-(basic_iterator it, difference_type offset) {
                return it -= offset;
            }

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

        // 元の型リストを適用して、挿入前後のイベント型を組み立てる。
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
            static_assert(sizeof...(Args) == element_types::size,
                          "Incorrect number of column values");
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
                    auto references =
                        std::apply([](auto&... values) { return std::tie(values...); }, data);
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
        [[nodiscard]] decltype(auto) get() {
            return schema_type::template column<N>(storage.entries);
        }

        template<size_t N>
        [[nodiscard]] decltype(auto) get() const {
            return schema_type::template column<N>(storage.entries);
        }

        [[nodiscard]] size_t size() const { return storage.size(); }

        [[nodiscard]] bool empty() const { return storage.empty(); }

        template<size_t N> auto column() {
            return std::views::counted(std::ranges::begin(get<N>()),
                                       static_cast<ptrdiff_t>(size()));
        }

        template<size_t N> auto column() const {
            return std::views::counted(std::ranges::begin(get<N>()),
                                       static_cast<ptrdiff_t>(size()));
        }

        auto columns() { return schema_type::views(storage.entries, size()); }

        auto columns() const { return schema_type::views(storage.entries, size()); }

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
        std::expected<_detail::lookup_value<const_element_reference<N>>, error_code>
        get(id_type id) const {
            auto index = this->index(id);
            if (!index)
                return std::unexpected{index.error()};
            return _detail::lookup_reference(get<N>()[*index]);
        }

        template<typename... Args>
        std::expected<void, error_code> push_back(Args&&... args) {
            if constexpr (schema_type::named && sizeof...(Args) == 1) {
                using argument = std::tuple_element_t<0, std::tuple<Args...>>;
                if constexpr (std::same_as<std::remove_cvref_t<argument>,
                                           typename schema_type::record_type>) {
                    return schema_type::apply_record(
                        std::forward<Args>(args)..., [&](auto&&... values) {
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
                // 未使用の ID 領域に先に書き込み、退避用 vector を確保せずに行数を更新する。
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
    // record<T> を既存の buffer に渡すための別名。
    template<typename Record,
            generator::is_generator IDGenerator = generator::iota,
            eventsystem::is_eventsystem EventSystem = eventsystem::dummy,
            template<typename, size_t, typename...> typename Storage = storage::static_storage,
            size_t MaxSize = Storage<size_t, 1, int>::default_max_size>
    using record_buffer = buffer<IDGenerator, EventSystem, Storage, MaxSize, record<Record>>;
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
