// libs/builtin/tests/yyyymmdd_test.cpp
//
// Regression guard for yyyymmdd() — packed integer date
// Y*10000 + M*100 + D from a serial date number.
//
// EXTENSION vs MATLAB: numkit accepts serial date numbers directly
// (MATLAB R2025b requires datetime). Equivalent MATLAB call:
//   yyyymmdd(datetime(d, 'ConvertFrom', 'datenum'))

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class YyyymmddTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(YyyymmddTest, BasicScalar)
{
    EXPECT_DOUBLE_EQ(evalScalar("yyyymmdd(datenum(2026, 5, 9))"),  20260509.0);
    EXPECT_DOUBLE_EQ(evalScalar("yyyymmdd(datenum(2000, 1, 1))"),  20000101.0);
    EXPECT_DOUBLE_EQ(evalScalar("yyyymmdd(datenum(1970, 12, 31))"), 19701231.0);
}

TEST_F(YyyymmddTest, UnixEpoch)
{
    EXPECT_DOUBLE_EQ(evalScalar("yyyymmdd(719529)"), 19700101.0);
}

TEST_F(YyyymmddTest, YearZeroEdge)
{
    EXPECT_DOUBLE_EQ(evalScalar("yyyymmdd(datenum(0, 1, 1))"), 101.0);
}

TEST_F(YyyymmddTest, ZeroSerialEdge)
{
    // Matches datevec(0) convention.
    EXPECT_DOUBLE_EQ(evalScalar("yyyymmdd(0)"), 0.0);
}

TEST_F(YyyymmddTest, ColumnVector)
{
    eval("v = yyyymmdd([datenum(2026,5,9); datenum(2027,6,10); "
         "              datenum(2028,7,11)]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(v, 1)")), 3);
    EXPECT_DOUBLE_EQ(evalScalar("v(1)"), 20260509.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(2)"), 20270610.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(3)"), 20280711.0);
}

TEST_F(YyyymmddTest, MatrixShapePreserved)
{
    eval("M = yyyymmdd([datenum(2024,1,1) datenum(2024,2,2); "
         "              datenum(2024,3,3) datenum(2024,4,4)]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(M, 1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(M, 2)")), 2);
    EXPECT_DOUBLE_EQ(evalScalar("M(1,1)"), 20240101.0);
    EXPECT_DOUBLE_EQ(evalScalar("M(1,2)"), 20240202.0);
    EXPECT_DOUBLE_EQ(evalScalar("M(2,1)"), 20240303.0);
    EXPECT_DOUBLE_EQ(evalScalar("M(2,2)"), 20240404.0);
}

TEST_F(YyyymmddTest, FractionalDayTruncates)
{
    // Only the integer day matters; time fraction is ignored.
    EXPECT_DOUBLE_EQ(
        evalScalar("yyyymmdd(datenum(2026, 5, 9) + 0.7)"),
        20260509.0);
}
