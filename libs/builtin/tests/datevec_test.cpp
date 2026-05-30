// libs/builtin/tests/datevec_test.cpp
//
// Regression guard for datevec() — inverse of datenum.
// Round-trip exactness expected since the algorithm is the inverse
// of the same days_from_civil core used in datenum.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class DatevecTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(DatevecTest, RoundTripScalar)
{
    eval("v = datevec(datenum(2026, 5, 9));");
    EXPECT_DOUBLE_EQ(evalScalar("v(1)"), 2026.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(2)"),    5.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(3)"),    9.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(4)"),    0.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(5)"),    0.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(6)"),    0.0);
}

TEST_F(DatevecTest, RoundTripWithTime)
{
    eval("v = datevec(datenum(2026, 5, 9, 12, 30, 45));");
    EXPECT_DOUBLE_EQ(evalScalar("v(4)"), 12.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(5)"), 30.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(6)"), 45.0);
}

TEST_F(DatevecTest, FractionalSecondPreserved)
{
    eval("v = datevec(datenum(2026, 5, 9, 12, 30, 45.5));");
    EXPECT_NEAR(evalScalar("v(6)"), 45.5, 1e-6);
}

TEST_F(DatevecTest, UnixEpoch)
{
    eval("v = datevec(719529);");
    EXPECT_DOUBLE_EQ(evalScalar("v(1)"), 1970.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(2)"),    1.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(3)"),    1.0);
}

TEST_F(DatevecTest, YearZero)
{
    eval("v = datevec(1);");
    EXPECT_DOUBLE_EQ(evalScalar("v(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(3)"), 1.0);
}

TEST_F(DatevecTest, EdgeZero)
{
    eval("v = datevec(0);");
    EXPECT_DOUBLE_EQ(evalScalar("v(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(2)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(3)"), 0.0);
}

TEST_F(DatevecTest, FractionalDayHalf)
{
    eval("v = datevec(datenum(2026, 5, 9) + 0.5);");
    EXPECT_DOUBLE_EQ(evalScalar("v(4)"), 12.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(5)"),  0.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(6)"),  0.0);
}

TEST_F(DatevecTest, FractionalDayQuarter)
{
    eval("v = datevec(datenum(2026, 5, 9) + 0.25);");
    EXPECT_DOUBLE_EQ(evalScalar("v(4)"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(5)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(6)"), 0.0);
}

TEST_F(DatevecTest, ColumnVectorInput)
{
    eval("M = datevec([datenum(2026,5,9); datenum(2026,5,10); "
         "             datenum(2026,5,11)]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(M, 1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(M, 2)")), 6);
    EXPECT_DOUBLE_EQ(evalScalar("M(1,3)"),  9.0);
    EXPECT_DOUBLE_EQ(evalScalar("M(2,3)"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("M(3,3)"), 11.0);
}

TEST_F(DatevecTest, MultiOutputForm)
{
    eval("[Y, Mo, D, H, MI, S] = datevec(datenum(2026, 5, 9, 12, 30, 45.5));");
    EXPECT_DOUBLE_EQ(evalScalar("Y"),  2026.0);
    EXPECT_DOUBLE_EQ(evalScalar("Mo"),    5.0);
    EXPECT_DOUBLE_EQ(evalScalar("D"),     9.0);
    EXPECT_DOUBLE_EQ(evalScalar("H"),    12.0);
    EXPECT_DOUBLE_EQ(evalScalar("MI"),   30.0);
    EXPECT_NEAR(evalScalar("S"), 45.5, 1e-6);
}

TEST_F(DatevecTest, MultiOutputVector)
{
    eval("[Y, Mo, D] = datevec([datenum(2026,5,9); datenum(2027,6,10)]);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(Y)")), 2);
    EXPECT_DOUBLE_EQ(evalScalar("Y(1)"), 2026.0);
    EXPECT_DOUBLE_EQ(evalScalar("Y(2)"), 2027.0);
    EXPECT_DOUBLE_EQ(evalScalar("Mo(1)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("Mo(2)"), 6.0);
}

// datestr: format a serial date number / date vector as text. vs MATLAB
// R2025b. Implemented 2026-05-30 (was an undefined function). 738885.5 =
// 30-Dec-2022 12:00:00 (a Friday).
TEST_F(DatevecTest, DatestrDefaultAndTokens)
{
    // Default format: date-only when no time, date+time otherwise.
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(datestr(738885), '30-Dec-2022')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(datestr(738885.5), '30-Dec-2022 12:00:00')"), 1.0);
    // Format-string tokens.
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(datestr(738885.5,'yyyy-mm-dd'), '2022-12-30')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(datestr(738885.523,'yyyy/mm/dd HH:MM:SS'), '2022/12/30 12:33:07')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(datestr(738885,'mmmm dd, yyyy'), 'December 30, 2022')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(datestr(738885,'ddd'), 'Fri')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(datestr(738885,'dddd'), 'Friday')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(datestr(738885,'yy'), '22')"), 1.0);
}

TEST_F(DatevecTest, DatestrDatevecInput)
{
    // A 1x6 date vector is interpreted as [Y mo D H MI S].
    EXPECT_DOUBLE_EQ(
        evalScalar("strcmp(datestr([2022 12 30 6 5 9],'yyyy-mm-dd HH:MM:SS'), "
                   "'2022-12-30 06:05:09')"),
        1.0);
}

// datevec(str [, fmt]): parse a date string into [Y M D H MI S]. vs MATLAB
// R2025b. 2026-05-30: previously threw "string parsing not yet supported".
TEST_F(DatevecTest, StringParse)
{
    eval("v = datevec('2022-12-30 12:34:56');");
    EXPECT_DOUBLE_EQ(evalScalar("v(1)"), 2022.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(2)"),   12.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(3)"),   30.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(4)"),   12.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(5)"),   34.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(6)"),   56.0);
    // ISO date-only and dd-mmm-yyyy auto-detect.
    eval("v2 = datevec('30-Dec-2022');");
    EXPECT_DOUBLE_EQ(evalScalar("v2(2)"), 12.0);
    EXPECT_DOUBLE_EQ(evalScalar("v2(3)"), 30.0);
    EXPECT_DOUBLE_EQ(evalScalar("v2(4)"),  0.0);
    // Explicit format string.
    eval("v3 = datevec('30/12/2022','dd/mm/yyyy');");
    EXPECT_DOUBLE_EQ(evalScalar("v3(1)"), 2022.0);
    EXPECT_DOUBLE_EQ(evalScalar("v3(3)"),   30.0);
    // Multi-output form.
    eval("[yy, mm, dd] = datevec('2022-12-30');");
    EXPECT_DOUBLE_EQ(evalScalar("yy"), 2022.0);
    EXPECT_DOUBLE_EQ(evalScalar("mm"),   12.0);
    EXPECT_DOUBLE_EQ(evalScalar("dd"),   30.0);
    // Unparseable string throws.
    EXPECT_THROW(eval("datevec('not a date')"), std::exception);
}
