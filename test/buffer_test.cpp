#include <saitekikuukan.hpp>
#include <gtest/gtest.h>

namespace {
    template<template<typename, std::size_t, typename...> typename Storage>
    void check_buffer() {
        using Buffer = kk::buffer<kk::generator::iota, kk::eventsystem::dummy, Storage, 3, int, int>;
        Buffer values{};
        static_assert(std::random_access_iterator<typename Buffer::iterator>);
        static_assert(std::random_access_iterator<typename Buffer::const_iterator>);
        int first = 10;
        ASSERT_TRUE(values.push_back(first, 20));
        ASSERT_EQ(values.size(), 1u);
        ASSERT_TRUE(values[0]);
        EXPECT_EQ(values[0]->id(), 0u);
        EXPECT_EQ(values[0]->template get<0>(), 10);
        ASSERT_TRUE(values.template get<1>(0));
        values.template get<1>(0)->get() = 21;
        const auto& read_only = values;
        ASSERT_TRUE(read_only[0]);
        EXPECT_EQ(read_only[0]->template get<1>(), 21);
        static_assert(std::same_as<decltype(read_only.template get<0>()), std::span<const int>>);
        EXPECT_FALSE(read_only[99]);
        EXPECT_FALSE(read_only.template get<0>(99));
        ASSERT_TRUE(values.resize(3));
        EXPECT_EQ(values[1]->id(), 1u);
        EXPECT_EQ(values[2]->id(), 2u);
        EXPECT_FALSE(values.push_back(1, 2));
        EXPECT_FALSE(values.resize(4));
        EXPECT_EQ(values.size(), 3u);
        EXPECT_FALSE(values.storage.swap(3, 0));
        EXPECT_FALSE(values.erase(99));
        ASSERT_TRUE(values.erase(1));
        EXPECT_EQ(values.size(), 2u);
        EXPECT_FALSE(values[1]);
        EXPECT_TRUE(values[2]);
        int count = 0;
        for (auto row : values) {
            (void)row.template get<0>();
            ++count;
        }
        EXPECT_EQ(count, 2);
        const auto begin = read_only.begin();
        EXPECT_EQ(begin->id(), 0u);
        ASSERT_TRUE(values.resize(0));
        EXPECT_TRUE(values.empty());
        values.clear();
    }

    TEST(BufferTest, FixedStorageSupportsZeroIDCapacityAndErase) {
        check_buffer<kk::storage::static_storage>();
    }

    TEST(BufferTest, DynamicStorageSharesDenseColumnsAndSupportsZeroID) {
        check_buffer<kk::storage::dynamic_storage>();
    }

    TEST(BufferTest, TypeFilterActuallyExcludesFalsePredicates) {
        using Filtered = kk::type_list<int, float, long>::filter<std::is_integral>;
        static_assert(Filtered::size == 2);
        static_assert(std::same_as<Filtered::get<0>, int>);
        static_assert(std::same_as<Filtered::get<1>, long>);
    }
} // namespace
