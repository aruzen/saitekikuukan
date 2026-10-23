#include <saitekikuukan/soa.hpp>
#include <gtest/gtest.h>
#include <string>

namespace {
    struct Vertex {
        std::array<float, 3> normal{0, 0, 1};
        std::array<float, 2> uv0{}, uv1{};
        bool operator==(const Vertex&) const = default;
    };
    struct Empty {};
    struct Label {
        std::string text = "default";
        int value = 10;
    };
} // namespace

template<>
struct kk::Schema<Vertex> {
    using Members = kk::MemberList<&Vertex::normal, &Vertex::uv0, &Vertex::uv1>;
};
template<>
struct kk::Schema<Empty> {
    using Members = kk::MemberList<>;
};
template<>
struct kk::Schema<Label> {
    using Members = kk::MemberList<&Label::text, &Label::value>;
};

namespace {
    TEST(SoATest, MemberPointersDistinguishEqualFieldTypesAndRoundTripRows) {
        const std::array rows{Vertex{{1, 2, 3}, {4, 5}, {6, 7}}, Vertex{{8, 9, 10}, {11, 12}, {13, 14}}};
        kk::SoA<Vertex> vertices(rows);
        EXPECT_EQ(vertices.read(0), rows[0]);
        EXPECT_EQ(vertices.read(1), rows[1]);
        vertices.column<&Vertex::uv0>()[0][1] = 50;
        EXPECT_EQ(vertices.column<&Vertex::uv1>()[0][1], 7);
        EXPECT_EQ(vertices[0].get<&Vertex::uv0>()[1], 50);
        vertices.at(1).write(rows[0]);
        EXPECT_EQ(vertices[1].read(), rows[0]);
        const auto& view = vertices;
        static_assert(std::same_as<decltype(view.column<&Vertex::normal>()), std::span<const std::array<float, 3>>>);
        static_assert(std::same_as<decltype(view[0].get<&Vertex::uv0>()), const std::array<float, 2>&>);
        EXPECT_EQ(view.at(0).read().uv0[1], 50);
        EXPECT_THROW((void)vertices.at(2), std::out_of_range);
        EXPECT_THROW((void)view.at(2), std::out_of_range);
        EXPECT_THROW((void)vertices.read(2), std::out_of_range);
        EXPECT_THROW(vertices.write(2, rows[0]), std::out_of_range);
    }

    TEST(SoATest, GrowthUsesRowDefaultsAndShrinkKeepsExistingValues) {
        kk::SoA<Vertex> vertices(2);
        EXPECT_EQ(vertices.read(1), Vertex{});
        vertices[0].get<&Vertex::normal>()[2] = 9;
        vertices.reserve(8);
        vertices.resize(4);
        EXPECT_EQ(vertices.read(3).normal[2], 1);
        vertices.resize(1);
        EXPECT_EQ(vertices.read(0).normal[2], 9);
        EXPECT_GE(vertices.capacity(), 8u);
        vertices.pop_back();
        vertices.pop_back();
        EXPECT_TRUE(vertices.empty());
        vertices.push_back(Vertex{});
        vertices.clear();
        EXPECT_TRUE(vertices.column<&Vertex::normal>().empty());
    }

    TEST(SoATest, EmptySchemaTracksRowsAndSupportsAccess) {
        kk::SoA<Empty> values(3);
        EXPECT_EQ(values.size(), 3u);
        EXPECT_NO_THROW(values.at(2).write(Empty{}));
        EXPECT_NO_THROW((void)values.read(2));
        EXPECT_THROW((void)values.at(3), std::out_of_range);
    }

    TEST(SoATest, NonTrivialRowsCopyMoveAndReuseMovedFromStorage) {
        kk::SoA<Label> rows(2);
        rows.write(0, {"hello", 42});
        auto copy = rows;
        copy[0].get<&Label::text>() = "copy";
        EXPECT_EQ(rows.read(0).text, "hello");
        auto moved = std::move(copy);
        EXPECT_EQ(moved.read(0).text, "copy");
        EXPECT_TRUE(copy.empty());
        copy.push_back({"reused", 99});
        rows = std::move(copy);
        EXPECT_EQ(rows.read(0).value, 99);
        EXPECT_TRUE(copy.empty());
    }
} // namespace
