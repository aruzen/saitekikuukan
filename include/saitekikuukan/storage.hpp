#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <limits>
#include <span>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#ifndef SAITEKIKUUKAN_NAMESPACE
#define SAITEKIKUUKAN_NAMESPACE kk
#endif

namespace SAITEKIKUUKAN_NAMESPACE::storage {
    // 各列の要素数を揃えて管理する。IDやイベントは利用側で付加する。
    template<typename... Types>
    class DynamicColumns {
        static_assert((std::is_object_v<Types> && ...));
        static_assert((!std::same_as<Types, bool> && ...), "bool columns require a contiguous byte type");
        static_assert((std::same_as<Types, std::remove_cv_t<Types>> && ...));
        static_assert(((std::is_copy_constructible_v<Types> || std::is_nothrow_move_constructible_v<Types>) && ...));

        std::tuple<std::vector<Types>...> _entries;
        std::size_t _size = 0;

        void rollback() noexcept;

    public:
        DynamicColumns() = default;
        DynamicColumns(const DynamicColumns&) = default;
        DynamicColumns(DynamicColumns&& other) noexcept;
        DynamicColumns& operator=(DynamicColumns other) noexcept;

        [[nodiscard]] std::size_t size() const noexcept;
        [[nodiscard]] bool empty() const noexcept;
        [[nodiscard]] std::size_t capacity() const noexcept;
        [[nodiscard]] std::size_t max_size() const noexcept;

        void reserve(std::size_t count);
        void resize(std::size_t count);
        template<typename... Args>
        requires(sizeof...(Args) == sizeof...(Types))
        void push_back(Args&&... args);
        void pop_back() noexcept;
        void clear() noexcept;
        void swap(DynamicColumns& other) noexcept;

        template<std::size_t Index>
        [[nodiscard]] auto column() noexcept;
        template<std::size_t Index>
        [[nodiscard]] auto column() const noexcept;
    };

    template<typename... Types>
    DynamicColumns<Types...>::DynamicColumns(DynamicColumns&& other) noexcept
        : _entries(std::move(other._entries)), _size(std::exchange(other._size, 0)) {
        other.clear();
    }

    template<typename... Types>
    DynamicColumns<Types...>& DynamicColumns<Types...>::operator=(DynamicColumns other) noexcept {
        swap(other);
        return *this;
    }

    template<typename... Types>
    std::size_t DynamicColumns<Types...>::size() const noexcept {
        return _size;
    }

    template<typename... Types>
    bool DynamicColumns<Types...>::empty() const noexcept {
        return _size == 0;
    }

    template<typename... Types>
    std::size_t DynamicColumns<Types...>::capacity() const noexcept {
        if constexpr (sizeof...(Types) == 0)
            return std::numeric_limits<std::size_t>::max();
        else
            return std::apply([](const auto&... columns) { return std::min({columns.capacity()...}); }, _entries);
    }

    template<typename... Types>
    std::size_t DynamicColumns<Types...>::max_size() const noexcept {
        if constexpr (sizeof...(Types) == 0)
            return std::numeric_limits<std::size_t>::max();
        else
            return std::apply([](const auto&... columns) { return std::min({columns.max_size()...}); }, _entries);
    }

    template<typename... Types>
    void DynamicColumns<Types...>::reserve(std::size_t count) {
        if (count > max_size())
            throw std::length_error("Column capacity exceeds max_size");
        std::apply([count](auto&... columns) { (columns.reserve(count), ...); }, _entries);
    }

    template<typename... Types>
    void DynamicColumns<Types...>::rollback() noexcept {
        std::apply([this](auto&... columns) {
            ([this](auto& column) {
                while (column.size() > _size)
                    column.pop_back();
            }(columns),
             ...);
        },
                   _entries);
    }

    template<typename... Types>
    void DynamicColumns<Types...>::resize(std::size_t count) {
        if (count > _size) {
            reserve(count);
            try {
                std::apply([count](auto&... columns) { (columns.resize(count), ...); }, _entries);
            } catch (...) {
                rollback();
                throw;
            }
        } else {
            std::apply([count](auto&... columns) {
                ([count](auto& column) {
                    while (column.size() > count)
                        column.pop_back();
                }(columns),
                 ...);
            },
                       _entries);
        }
        _size = count;
    }

    template<typename... Types>
    template<typename... Args>
    requires(sizeof...(Args) == sizeof...(Types))
    void DynamicColumns<Types...>::push_back(Args&&... args) {
        if (_size == max_size())
            throw std::length_error("Column size exceeds max_size");
        // 自身の列を引数にした追加でも、再確保より先に値を保持する。
        std::tuple<Types...> values(std::forward<Args>(args)...);
        if (_size == capacity())
            reserve(_size + std::min(max_size() - _size, _size / 2 + 1));
        try {
            [&]<std::size_t... Indices>(std::index_sequence<Indices...>) {
                (std::get<Indices>(_entries).push_back(std::move(std::get<Indices>(values))), ...);
            }(std::index_sequence_for<Types...>{});
        } catch (...) {
            rollback();
            throw;
        }
        ++_size;
    }

    template<typename... Types>
    void DynamicColumns<Types...>::pop_back() noexcept {
        if (empty())
            return;
        std::apply([](auto&... columns) { (columns.pop_back(), ...); }, _entries);
        --_size;
    }

    template<typename... Types>
    void DynamicColumns<Types...>::clear() noexcept {
        std::apply([](auto&... columns) { (columns.clear(), ...); }, _entries);
        _size = 0;
    }

    template<typename... Types>
    void DynamicColumns<Types...>::swap(DynamicColumns& other) noexcept {
        _entries.swap(other._entries);
        std::swap(_size, other._size);
    }

    template<typename... Types>
    template<std::size_t Index>
    auto DynamicColumns<Types...>::column() noexcept {
        return std::span{std::get<Index>(_entries)};
    }

    template<typename... Types>
    template<std::size_t Index>
    auto DynamicColumns<Types...>::column() const noexcept {
        return std::span{std::get<Index>(_entries)};
    }
} // namespace SAITEKIKUUKAN_NAMESPACE::storage
