#pragma once

#ifndef SAITEKIKUUKAN_REFLECTION_HPP
#define SAITEKIKUUKAN_REFLECTION_HPP

#if !defined(__cpp_impl_reflection) || __cpp_impl_reflection < 202506L
#error "saitekikuukan_reflection.hpp requires C++26 reflection (GCC 16: -std=c++26 -freflection)"
#else

#include <array>
#include <cstddef>
#include <meta>
#include <type_traits>
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
}

#endif // reflection support
#endif // SAITEKIKUUKAN_REFLECTION_HPP
