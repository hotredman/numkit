// toolboxes/builtin/tests/weekday_test.cpp
//
// Regression guard for weekday() — MATLAB day-of-week index
// (1=Sun .. 7=Sat) with optional name string output.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class WeekdayTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(WeekdayTest, IndexSaturday)
{
    // 2026-05-09 = Saturday
    EXPECT_DOUBLE_EQ(evalScalar("weekday(datenum(2026, 5, 9))"), 7.0);
}

TEST_F(WeekdayTest, IndexSunday)
{
    // 2026-05-10 = Sunday
    EXPECT_DOUBLE_EQ(evalScalar("weekday(datenum(2026, 5, 10))"), 1.0);
}

TEST_F(WeekdayTest, IndexHistoricalThursday)
{
    // 1970-01-01 = Thursday
    EXPECT_DOUBLE_EQ(evalScalar("weekday(datenum(1970, 1, 1))"), 5.0);
}

TEST_F(WeekdayTest, ShortNameOutput)
{
    eval("[d, n] = weekday(datenum(2026, 5, 9));");
    EXPECT_DOUBLE_EQ(evalScalar("d"), 7.0);
    auto nv = eval("n");
    EXPECT_TRUE(nv.isChar() || nv.isString());
    EXPECT_EQ(nv.toString(), "Sat");
}

TEST_F(WeekdayTest, LongNameOutput)
{
    eval("[d, n] = weekday(datenum(2026, 5, 9), 'long');");
    EXPECT_DOUBLE_EQ(evalScalar("d"), 7.0);
    auto nv = eval("n");
    EXPECT_EQ(nv.toString(), "Saturday");
}

TEST_F(WeekdayTest, ColumnVectorInput)
{
    eval("v = weekday([datenum(2026, 5, 9); datenum(2026, 5, 10); "
         "             datenum(2026, 5, 11)]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(v, 1)")), 3);
    EXPECT_DOUBLE_EQ(evalScalar("v(1)"), 7.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(3)"), 2.0);
}

TEST_F(WeekdayTest, FullWeekRoundtrip)
{
    // Walk a full week and verify the cycle 7,1,2,3,4,5,6,7
    eval("d0 = datenum(2026, 5, 9);"  // Saturday
         "w = weekday([d0; d0+1; d0+2; d0+3; d0+4; d0+5; d0+6; d0+7]);");
    EXPECT_DOUBLE_EQ(evalScalar("w(1)"), 7.0);
    EXPECT_DOUBLE_EQ(evalScalar("w(2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("w(3)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("w(4)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("w(5)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("w(6)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("w(7)"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("w(8)"), 7.0);
}
