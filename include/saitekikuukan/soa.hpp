#pragma once
#include "storage.hpp"
#include <array>

namespace SAITEKIKUUKAN_NAMESPACE {
    template<typename Row>
    struct Schema;

    template<auto... Members>
    struct MemberList {
        static_assert((std::is_member_object_pointer_v<decltype(Members)> && ...));
    };

    namespace detail {
        template<typename T>
        struct MemberType;

        template<typename Owner, typename Value>
        struct MemberType<Value Owner::*> {
            using OwnerType = Owner;
            using ValueType = Value;
        };

        template<auto Left, auto Right>
        consteval bool same_member() {
            if constexpr (std::same_as<decltype(Left), decltype(Right)>)
                return Left == Right;
            else
                return false;
        }

        template<auto Target, auto... Members>
        consteval std::size_t member_index() {
            constexpr std::array<bool, sizeof...(Members)> matches{same_member<Target, Members>()...};
            static_assert(std::count(matches.begin(), matches.end(), true) == 1,
                          "The field must occur exactly once in Schema");
            return static_cast<std::size_t>(std::find(matches.begin(), matches.end(), true) - matches.begin());
        }

        template<typename Row, typename List>
        struct SchemaStorage;

        template<typename Row, auto... Members>
        struct SchemaStorage<Row, MemberList<Members...>> {
            static_assert((std::same_as<Row, typename MemberType<decltype(Members)>::OwnerType> && ...),
                          "Schema fields must belong to the row type");
            static_assert(((member_index<Members, Members...>() < sizeof...(Members)) && ...));
            using Columns = storage::DynamicColumns<typename MemberType<decltype(Members)>::ValueType...>;
        };
    } // namespace detail

    template<typename Row, typename Members = typename Schema<Row>::Members>
    class SoA;

    template<typename Row, auto... Members>
    class SoA<Row, MemberList<Members...>> {
        static_assert(std::default_initializable<Row> && std::copy_constructible<Row>);
        using Columns = typename detail::SchemaStorage<Row, MemberList<Members...>>::Columns;
        Columns _columns;

    public:
        template<bool IS_CONST>
        class RowRef {
            using Parent = std::conditional_t<IS_CONST, const SoA, SoA>;
            Parent* _parent;
            std::size_t _index;

        public:
            RowRef(Parent& parent, std::size_t index) noexcept;
            RowRef(const RowRef&) = default;
            RowRef& operator=(const RowRef&) = delete;

            template<auto Member>
            [[nodiscard]] decltype(auto) get() const noexcept;
            [[nodiscard]] Row read() const;
            void write(const Row& row) const
            requires(!IS_CONST);
        };

        using ValueType = Row;
        SoA() = default;
        explicit SoA(std::span<const Row> rows);
        explicit SoA(std::size_t count);

        [[nodiscard]] std::size_t size() const noexcept;
        [[nodiscard]] bool empty() const noexcept;
        [[nodiscard]] std::size_t capacity() const noexcept;
        void reserve(std::size_t count);
        void resize(std::size_t count);
        void push_back(const Row& row);
        void pop_back() noexcept;
        void clear() noexcept;

        template<auto Member>
        [[nodiscard]] auto column() noexcept;
        template<auto Member>
        [[nodiscard]] auto column() const noexcept;

        [[nodiscard]] RowRef<false> operator[](std::size_t index) noexcept;
        [[nodiscard]] RowRef<true> operator[](std::size_t index) const noexcept;
        [[nodiscard]] RowRef<false> at(std::size_t index);
        [[nodiscard]] RowRef<true> at(std::size_t index) const;
        [[nodiscard]] Row read(std::size_t index) const;
        void write(std::size_t index, const Row& row);
    };

    template<typename Row, auto... Members>
    SoA<Row, MemberList<Members...>>::SoA(std::span<const Row> rows) {
        reserve(rows.size());
        for (const auto& row : rows)
            push_back(row);
    }

    template<typename Row, auto... Members>
    SoA<Row, MemberList<Members...>>::SoA(std::size_t count) {
        resize(count);
    }

    template<typename Row, auto... Members>
    std::size_t SoA<Row, MemberList<Members...>>::size() const noexcept {
        return _columns.size();
    }

    template<typename Row, auto... Members>
    bool SoA<Row, MemberList<Members...>>::empty() const noexcept {
        return _columns.empty();
    }

    template<typename Row, auto... Members>
    std::size_t SoA<Row, MemberList<Members...>>::capacity() const noexcept {
        return _columns.capacity();
    }

    template<typename Row, auto... Members>
    void SoA<Row, MemberList<Members...>>::reserve(std::size_t count) {
        _columns.reserve(count);
    }

    template<typename Row, auto... Members>
    void SoA<Row, MemberList<Members...>>::resize(std::size_t count) {
        if (count <= size()) {
            _columns.resize(count);
            return;
        }
        const auto old = size();
        reserve(count);
        try {
            while (size() < count)
                push_back(Row{});
        } catch (...) {
            while (size() > old)
                pop_back();
            throw;
        }
    }

    template<typename Row, auto... Members>
    void SoA<Row, MemberList<Members...>>::push_back(const Row& row) {
        _columns.push_back(row.*Members...);
    }

    template<typename Row, auto... Members>
    void SoA<Row, MemberList<Members...>>::pop_back() noexcept {
        _columns.pop_back();
    }

    template<typename Row, auto... Members>
    void SoA<Row, MemberList<Members...>>::clear() noexcept {
        _columns.clear();
    }

    template<typename Row, auto... Members>
    template<auto Member>
    auto SoA<Row, MemberList<Members...>>::column() noexcept {
        return _columns.template column<detail::member_index<Member, Members...>()>();
    }

    template<typename Row, auto... Members>
    template<auto Member>
    auto SoA<Row, MemberList<Members...>>::column() const noexcept {
        return _columns.template column<detail::member_index<Member, Members...>()>();
    }

    template<typename Row, auto... Members>
    auto SoA<Row, MemberList<Members...>>::operator[](std::size_t index) noexcept -> RowRef<false> {
        return {*this, index};
    }

    template<typename Row, auto... Members>
    auto SoA<Row, MemberList<Members...>>::operator[](std::size_t index) const noexcept -> RowRef<true> {
        return {*this, index};
    }

    template<typename Row, auto... Members>
    auto SoA<Row, MemberList<Members...>>::at(std::size_t index) -> RowRef<false> {
        if (index >= size())
            throw std::out_of_range("SoA row index exceeds size");
        return (*this)[index];
    }

    template<typename Row, auto... Members>
    auto SoA<Row, MemberList<Members...>>::at(std::size_t index) const -> RowRef<true> {
        if (index >= size())
            throw std::out_of_range("SoA row index exceeds size");
        return (*this)[index];
    }

    template<typename Row, auto... Members>
    Row SoA<Row, MemberList<Members...>>::read(std::size_t index) const {
        (void)at(index);
        Row result{};
        ((result.*Members = column<Members>()[index]), ...);
        return result;
    }

    template<typename Row, auto... Members>
    void SoA<Row, MemberList<Members...>>::write(std::size_t index, const Row& row) {
        (void)at(index);
        ((column<Members>()[index] = row.*Members), ...);
    }

    template<typename Row, auto... Members>
    template<bool IS_CONST>
    SoA<Row, MemberList<Members...>>::RowRef<IS_CONST>::RowRef(Parent& parent, std::size_t index) noexcept
        : _parent(&parent), _index(index) {}

    template<typename Row, auto... Members>
    template<bool IS_CONST>
    template<auto Member>
    decltype(auto) SoA<Row, MemberList<Members...>>::RowRef<IS_CONST>::get() const noexcept {
        return _parent->template column<Member>()[_index];
    }

    template<typename Row, auto... Members>
    template<bool IS_CONST>
    Row SoA<Row, MemberList<Members...>>::RowRef<IS_CONST>::read() const {
        return _parent->read(_index);
    }

    template<typename Row, auto... Members>
    template<bool IS_CONST>
    void SoA<Row, MemberList<Members...>>::RowRef<IS_CONST>::write(const Row& row) const
    requires(!IS_CONST)
    {
        _parent->write(_index, row);
    }
} // namespace SAITEKIKUUKAN_NAMESPACE
