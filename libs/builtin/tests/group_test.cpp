// libs/builtin/tests/group_test.cpp
//
// Regression guard for findgroups + groupcounts.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class GroupTest : public ::testing::Test
{
public:
    Engine engine;
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── findgroups ──────────────────────────────────────────────────

TEST_F(GroupTest, FindgroupsBasicColumn)
{
    eval("g = [10; 20; 10; 30; 20; 10; 30]; [G, ID] = findgroups(g);");
    EXPECT_EQ(static_cast<int>(evalScalar("G(1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("G(2)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("G(4)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("ID(1)")), 10);
    EXPECT_EQ(static_cast<int>(evalScalar("ID(2)")), 20);
    EXPECT_EQ(static_cast<int>(evalScalar("ID(3)")), 30);
}

TEST_F(GroupTest, FindgroupsNaNPropagates)
{
    eval("g = [1; 2; NaN; 1; NaN; 3]; [G, ID] = findgroups(g);");
    EXPECT_EQ(static_cast<int>(evalScalar("G(1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("G(2)")), 2);
    EXPECT_TRUE(std::isnan(evalScalar("G(3)")));
    EXPECT_TRUE(std::isnan(evalScalar("G(5)")));
    EXPECT_EQ(static_cast<int>(evalScalar("size(ID, 1)")), 3);  // NaN not in ID
}

// ── groupcounts ─────────────────────────────────────────────────

TEST_F(GroupTest, GroupcountsBasicCounts)
{
    eval("g = [10; 20; 10; 30; 20; 10; 30]; C = groupcounts(g);");
    EXPECT_EQ(static_cast<int>(evalScalar("C(1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("C(2)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("C(3)")), 2);
}

TEST_F(GroupTest, GroupcountsReturnsGR)
{
    eval("g = [10; 20; 10; 30; 20; 10; 30]; [C, GR] = groupcounts(g);");
    EXPECT_EQ(static_cast<int>(evalScalar("GR(1)")), 10);
    EXPECT_EQ(static_cast<int>(evalScalar("GR(2)")), 20);
    EXPECT_EQ(static_cast<int>(evalScalar("GR(3)")), 30);
}

TEST_F(GroupTest, GroupcountsReturnsPercent)
{
    eval("g = [10; 20; 10; 30; 20; 10; 30]; [C, GR, P] = groupcounts(g);");
    EXPECT_NEAR(evalScalar("P(1)"), 100.0 * 3.0 / 7.0, 1e-12);
    EXPECT_NEAR(evalScalar("P(2)"), 100.0 * 2.0 / 7.0, 1e-12);
    EXPECT_NEAR(evalScalar("P(3)"), 100.0 * 2.0 / 7.0, 1e-12);
}

TEST_F(GroupTest, GroupcountsNaNTrailingBucket)
{
    eval("g = [1; 2; NaN; 1; NaN; 3]; [C, GR] = groupcounts(g);");
    EXPECT_EQ(static_cast<int>(evalScalar("C(1)")), 2);  // 1 appears 2x
    EXPECT_EQ(static_cast<int>(evalScalar("C(2)")), 1);  // 2
    EXPECT_EQ(static_cast<int>(evalScalar("C(3)")), 1);  // 3
    EXPECT_EQ(static_cast<int>(evalScalar("C(4)")), 2);  // NaN bucket
    EXPECT_EQ(static_cast<int>(evalScalar("GR(1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("GR(2)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("GR(3)")), 3);
    EXPECT_TRUE(std::isnan(evalScalar("GR(4)")));
}

TEST_F(GroupTest, GroupcountsEmpty)
{
    eval("C = groupcounts([]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(C, 1)")), 0);
}

// ── errors ─────────────────────────────────────────────────────

TEST_F(GroupTest, FindgroupsNoArgsThrows)
{
    EXPECT_THROW(eval("findgroups();"), std::exception);
}

TEST_F(GroupTest, GroupcountsNoArgsThrows)
{
    EXPECT_THROW(eval("groupcounts();"), std::exception);
}
