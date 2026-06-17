// toolboxes/builtin/tests/juliandate_test.cpp
//
// Regression guard for juliandate() — Julian day number from date
// components. Bit-equality with MATLAB R2025b expected since the
// algorithm is the same days_from_civil + a constant offset.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class JuliandateTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(JuliandateTest, UnixEpochAnchor)
{
    // 1970-01-01 00:00 UTC = JD 2440587.5
    EXPECT_DOUBLE_EQ(evalScalar("juliandate(1970, 1, 1, 0, 0, 0)"),
                     2440587.5);
}

TEST_F(JuliandateTest, J2000Anchor)
{
    // 2000-01-01 12:00 UTC = JD 2451545.0 (J2000.0 epoch)
    EXPECT_DOUBLE_EQ(evalScalar("juliandate(2000, 1, 1, 12, 0, 0)"),
                     2451545.0);
}

TEST_F(JuliandateTest, ThreeArgScalar)
{
    EXPECT_DOUBLE_EQ(evalScalar("juliandate(2026, 5, 9)"), 2461169.5);
}

TEST_F(JuliandateTest, SixArgWithTime)
{
    EXPECT_DOUBLE_EQ(evalScalar("juliandate(2026, 5, 9, 12, 0, 0)"),
                     2461170.0);
    EXPECT_NEAR(evalScalar("juliandate(2026, 5, 9, 6, 0, 0)"),
                2461169.75, 1e-9);
}

TEST_F(JuliandateTest, RowVecForm)
{
    EXPECT_DOUBLE_EQ(evalScalar("juliandate([2026 5 9])"), 2461169.5);
    EXPECT_DOUBLE_EQ(evalScalar("juliandate([2026 5 9 12 0 0])"), 2461170.0);
}

TEST_F(JuliandateTest, MatrixForm)
{
    eval("jv = juliandate([2026 5 9; 2027 5 9; 2028 5 9]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(jv, 1)")), 3);
    EXPECT_DOUBLE_EQ(evalScalar("jv(1)"), 2461169.5);
    EXPECT_DOUBLE_EQ(evalScalar("jv(2)"), 2461534.5);
    EXPECT_DOUBLE_EQ(evalScalar("jv(3)"), 2461900.5);
}

TEST_F(JuliandateTest, BroadcastVectorArgs)
{
    eval("jv = juliandate([2026; 2027; 2028], [1; 1; 1], [1; 1; 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("jv(1)"), 2461041.5);
    EXPECT_DOUBLE_EQ(evalScalar("jv(2)"), 2461406.5);
    EXPECT_DOUBLE_EQ(evalScalar("jv(3)"), 2461771.5);
}

TEST_F(JuliandateTest, OffsetMatchesDatenum)
{
    // juliandate = datenum + 1721058.5 by construction
    EXPECT_DOUBLE_EQ(
        evalScalar("juliandate(2026, 5, 9) - datenum(2026, 5, 9)"),
        1721058.5);
}
