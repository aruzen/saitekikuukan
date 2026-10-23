#include <saitekikuukan/storage.hpp>
#include <gtest/gtest.h>
#include <string>

namespace {
    struct ThrowValue {
        inline static bool FAIL_MOVE = false;
        inline static bool FAIL_DEFAULT = false;
        inline static bool FAIL_COPY = false;
        int value = 7;

        ThrowValue();
        explicit ThrowValue(int v);
        ThrowValue(const ThrowValue& other);
        ThrowValue(ThrowValue&& other);
        ThrowValue& operator=(const ThrowValue&) = default;
    };

    ThrowValue::ThrowValue() {
        if (FAIL_DEFAULT)
            throw std::runtime_error("default");
    }

    ThrowValue::ThrowValue(int v) : value(v) {}

    ThrowValue::ThrowValue(const ThrowValue& other) : value(other.value) {
        if (FAIL_COPY)
            throw std::runtime_error("copy");
    }

    ThrowValue::ThrowValue(ThrowValue&& other) : value(other.value) {
        if (FAIL_MOVE)
            throw std::runtime_error("move");
    }

    TEST(DynamicColumnsTest, SameTypeColumnsAreIndependentAndContiguous) {
        kk::storage::DynamicColumns<int, int> columns;
        columns.reserve(4);
        columns.push_back(10, 20);
        columns.push_back(30, 40);
        EXPECT_GE(columns.capacity(), 4u);
        ASSERT_EQ(columns.size(), 2u);
        EXPECT_EQ(columns.column<0>()[1], 30);
        EXPECT_EQ(columns.column<1>()[0], 20);
        EXPECT_EQ(&columns.column<0>()[1], &columns.column<0>()[0] + 1);
        static_assert(std::same_as<decltype(std::as_const(columns).column<0>()), std::span<const int>>);
        columns.resize(4);
        EXPECT_EQ(columns.column<1>()[3], 0);
        columns.resize(1);
        EXPECT_EQ(columns.column<0>()[0], 10);
        columns.pop_back();
        columns.pop_back();
        EXPECT_TRUE(columns.empty());
        columns.push_back(1, 2);
        columns.clear();
        EXPECT_TRUE(columns.empty());
        EXPECT_TRUE(columns.column<1>().empty());
    }

    TEST(DynamicColumnsTest, NoFieldsStillRetainRowCount) {
        kk::storage::DynamicColumns<> columns;
        columns.push_back();
        columns.resize(5);
        EXPECT_EQ(columns.size(), 5u);
        columns.pop_back();
        EXPECT_EQ(columns.size(), 4u);
        EXPECT_GE(columns.capacity(), columns.size());
    }

    TEST(DynamicColumnsTest, AppendingOwnColumnSurvivesReallocation) {
        kk::storage::DynamicColumns<std::string> columns;
        columns.push_back(std::string(100, 'a'));
        while (columns.size() < columns.capacity())
            columns.push_back("other");
        const auto old = columns.size();
        columns.push_back(columns.column<0>()[0]);
        EXPECT_EQ(columns.column<0>()[old], std::string(100, 'a'));
    }

    TEST(DynamicColumnsTest, CopyAndMoveKeepColumnsAndCountTogether) {
        kk::storage::DynamicColumns<int, std::string> source;
        source.push_back(42, "value");
        auto copied = source;
        copied.column<0>()[0] = 99;
        EXPECT_EQ(source.column<0>()[0], 42);
        auto moved = std::move(copied);
        EXPECT_TRUE(copied.empty());
        EXPECT_TRUE(copied.column<1>().empty());
        copied.push_back(2, "reuse");
        moved = source;
        EXPECT_EQ(moved.column<0>()[0], 42);
        EXPECT_EQ(moved.column<1>()[0], "value");
        source = std::move(copied);
        EXPECT_EQ(source.column<1>()[0], "reuse");
        EXPECT_TRUE(copied.empty());
    }

    TEST(DynamicColumnsTest, AppendAndResizeFailureRollBackEarlierColumns) {
        kk::storage::DynamicColumns<int, ThrowValue> columns;
        columns.reserve(4);
        columns.push_back(11, ThrowValue(22));
        const ThrowValue value(33);
        ThrowValue::FAIL_MOVE = true;
        EXPECT_THROW(columns.push_back(44, value), std::runtime_error);
        ThrowValue::FAIL_MOVE = false;
        EXPECT_EQ(columns.size(), 1u);
        ASSERT_EQ(columns.column<0>().size(), 1u);
        ASSERT_EQ(columns.column<1>().size(), 1u);
        EXPECT_EQ(columns.column<0>()[0], 11);
        EXPECT_EQ(columns.column<1>()[0].value, 22);
        ThrowValue::FAIL_DEFAULT = true;
        EXPECT_THROW(columns.resize(3), std::runtime_error);
        ThrowValue::FAIL_DEFAULT = false;
        EXPECT_EQ(columns.size(), 1u);
        EXPECT_EQ(columns.column<0>().size(), 1u);
        EXPECT_EQ(columns.column<1>().size(), 1u);
        columns.resize(3);
        EXPECT_EQ(columns.column<1>()[2].value, 7);
    }

    TEST(DynamicColumnsTest, FailedCopyAssignmentPreservesDestination) {
        kk::storage::DynamicColumns<int, ThrowValue> source, destination;
        source.push_back(1, ThrowValue(2));
        destination.push_back(8, ThrowValue(9));
        ThrowValue::FAIL_COPY = true;
        EXPECT_THROW(destination = source, std::runtime_error);
        ThrowValue::FAIL_COPY = false;
        ASSERT_EQ(destination.size(), 1u);
        EXPECT_EQ(destination.column<0>()[0], 8);
        EXPECT_EQ(destination.column<1>()[0].value, 9);
    }
} // namespace
