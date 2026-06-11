// toolboxes/builtin/tests/mjuliandate_test.cpp
//
// Regression guard for mjuliandate() — Modified Julian Date.
// MJD = JD - 2400000.5; epoch is 1858-11-17 00:00 UTC.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class MjuliandateTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(MjuliandateTest, EpochAnchor)
{
    // 1858-11-17 00:00 UTC = MJD 0 by definition
    EXPECT_DOUBLE_EQ(evalScalar("mjuliandate(1858, 11, 17, 0, 0, 0)"), 0.0);
}

TEST_F(MjuliandateTest, UnixEpoch)
{
    // 1970-01-01 00:00 UTC = MJD 40587.0
    EXPECT_DOUBLE_EQ(evalScalar("mjuliandate(1970, 1, 1, 0, 0, 0)"), 40587.0);
}

TEST_F(MjuliandateTest, J2000)
{
    // 2000-01-01 12:00 UTC = MJD 51544.5
    EXPECT_DOUBLE_EQ(evalScalar("mjuliandate(2000, 1, 1, 12, 0, 0)"), 51544.5);
}

TEST_F(MjuliandateTest, ThreeArgScalar)
{
    EXPECT_DOUBLE_EQ(evalScalar("mjuliandate(2026, 5, 9)"), 61169.0);
}

TEST_F(MjuliandateTest, SixArgWithTime)
{
    EXPECT_DOUBLE_EQ(evalScalar("mjuliandate(2026, 5, 9, 12, 0, 0)"), 61169.5);
    EXPECT_NEAR(evalScalar("mjuliandate(2026, 5, 9, 6, 0, 0)"),
                61169.25, 1e-9);
}

TEST_F(MjuliandateTest, RowVecForm)
{
    EXPECT_DOUBLE_EQ(evalScalar("mjuliandate([2026 5 9])"), 61169.0);
    EXPECT_DOUBLE_EQ(evalScalar("mjuliandate([2026 5 9 12 0 0])"), 61169.5);
}

TEST_F(MjuliandateTest, MatrixForm)
{
    eval("mv = mjuliandate([2026 5 9; 2027 5 9; 2028 5 9]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(mv, 1)")), 3);
    EXPECT_DOUBLE_EQ(evalScalar("mv(1)"), 61169.0);
    EXPECT_DOUBLE_EQ(evalScalar("mv(2)"), 61534.0);
    EXPECT_DOUBLE_EQ(evalScalar("mv(3)"), 61900.0);
}

TEST_F(MjuliandateTest, BroadcastVectorArgs)
{
    eval("mv = mjuliandate([2026; 2027; 2028], [1; 1; 1], [1; 1; 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("mv(1)"), 61041.0);
    EXPECT_DOUBLE_EQ(evalScalar("mv(2)"), 61406.0);
    EXPECT_DOUBLE_EQ(evalScalar("mv(3)"), 61771.0);
}

TEST_F(MjuliandateTest, MatchesJDMinusOffset)
{
    // MJD = JD - 2400000.5 by definition
    EXPECT_DOUBLE_EQ(
        evalScalar("juliandate(2026, 5, 9, 12, 30, 45) - "
                   "mjuliandate(2026, 5, 9, 12, 30, 45)"),
        2400000.5);
}
