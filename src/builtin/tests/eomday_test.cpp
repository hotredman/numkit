// toolboxes/builtin/tests/eomday_test.cpp
//
// Regression guard for eomday() — last day of the given month with
// proleptic-Gregorian leap-year rule.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class EomdayTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(EomdayTest, FixedLengthMonths)
{
    EXPECT_DOUBLE_EQ(evalScalar("eomday(2026, 1)"),  31.0);
    EXPECT_DOUBLE_EQ(evalScalar("eomday(2026, 3)"),  31.0);
    EXPECT_DOUBLE_EQ(evalScalar("eomday(2026, 5)"),  31.0);
    EXPECT_DOUBLE_EQ(evalScalar("eomday(2026, 7)"),  31.0);
    EXPECT_DOUBLE_EQ(evalScalar("eomday(2026, 8)"),  31.0);
    EXPECT_DOUBLE_EQ(evalScalar("eomday(2026, 10)"), 31.0);
    EXPECT_DOUBLE_EQ(evalScalar("eomday(2026, 12)"), 31.0);
    EXPECT_DOUBLE_EQ(evalScalar("eomday(2026, 4)"),  30.0);
    EXPECT_DOUBLE_EQ(evalScalar("eomday(2026, 6)"),  30.0);
    EXPECT_DOUBLE_EQ(evalScalar("eomday(2026, 9)"),  30.0);
    EXPECT_DOUBLE_EQ(evalScalar("eomday(2026, 11)"), 30.0);
}

TEST_F(EomdayTest, FebruaryCommonYear)
{
    EXPECT_DOUBLE_EQ(evalScalar("eomday(2026, 2)"), 28.0);
    EXPECT_DOUBLE_EQ(evalScalar("eomday(2025, 2)"), 28.0);
}

TEST_F(EomdayTest, FebruaryLeapDivisibleByFour)
{
    // Standard 4-year leap rule
    EXPECT_DOUBLE_EQ(evalScalar("eomday(2024, 2)"), 29.0);
    EXPECT_DOUBLE_EQ(evalScalar("eomday(2020, 2)"), 29.0);
}

TEST_F(EomdayTest, FebruaryLeapCentury400)
{
    // 400-year exception: leap
    EXPECT_DOUBLE_EQ(evalScalar("eomday(2000, 2)"), 29.0);
    EXPECT_DOUBLE_EQ(evalScalar("eomday(1600, 2)"), 29.0);
}

TEST_F(EomdayTest, FebruaryNonLeapCentury100)
{
    // Century exception: NOT leap
    EXPECT_DOUBLE_EQ(evalScalar("eomday(1900, 2)"), 28.0);
    EXPECT_DOUBLE_EQ(evalScalar("eomday(1800, 2)"), 28.0);
    EXPECT_DOUBLE_EQ(evalScalar("eomday(2100, 2)"), 28.0);
}

TEST_F(EomdayTest, BroadcastScalarYearVectorMonth)
{
    eval("v = eomday(2024, 1:12);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(v)")), 12);
    EXPECT_DOUBLE_EQ(evalScalar("v(1)"), 31.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(2)"), 29.0);  // leap Feb
    EXPECT_DOUBLE_EQ(evalScalar("v(3)"), 31.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(4)"), 30.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(12)"), 31.0);
}

TEST_F(EomdayTest, BroadcastVectorYearScalarMonth)
{
    eval("v = eomday([2024; 2025; 2026], 2);");
    EXPECT_DOUBLE_EQ(evalScalar("v(1)"), 29.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(2)"), 28.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(3)"), 28.0);
}

TEST_F(EomdayTest, MatrixShapePreserved)
{
    eval("M = eomday([2024 2025; 2026 2027], [1 2; 3 4]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(M, 1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(M, 2)")), 2);
    EXPECT_DOUBLE_EQ(evalScalar("M(1,1)"), 31.0);  // Jan 2024
    EXPECT_DOUBLE_EQ(evalScalar("M(1,2)"), 28.0);  // Feb 2025
    EXPECT_DOUBLE_EQ(evalScalar("M(2,1)"), 31.0);  // Mar 2026
    EXPECT_DOUBLE_EQ(evalScalar("M(2,2)"), 30.0);  // Apr 2027
}
