#pragma once

#ifndef SAITEKIKUUKAN_REFLECTION_HPP
#define SAITEKIKUUKAN_REFLECTION_HPP

#if !defined(__cpp_impl_reflection) || __cpp_impl_reflection < 202506L
#error "saitekikuukan_reflection.hpp requires C++26 reflection (GCC 16: -std=c++26 -freflection)"
#else

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

#endif // reflection support
#endif // SAITEKIKUUKAN_REFLECTION_HPP
