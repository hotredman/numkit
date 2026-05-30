// libs/builtin/tests/datenum_test.cpp
//
// Regression guard for datenum() — MATLAB serial date number from
// date components. Exact bit-for-bit MATLAB match expected since the
// algorithm is integer-arithmetic civil-to-serial-day conversion.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class DatenumTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(DatenumTest, ThreeArgScalar)
{
    EXPECT_DOUBLE_EQ(evalScalar("datenum(2026, 5, 9)"), 740111.0);
    EXPECT_DOUBLE_EQ(evalScalar("datenum(1970, 1, 1)"), 719529.0);
    EXPECT_DOUBLE_EQ(evalScalar("datenum(0, 1, 1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("datenum(0, 1, 0)"), 0.0);
}

TEST_F(DatenumTest, SixArgScalarIncludesTime)
{
    EXPECT_NEAR(evalScalar("datenum(2026, 5, 9, 12, 30, 45)"),
                740111.521354166666, 1e-9);
    EXPECT_NEAR(evalScalar("datenum(2026, 5, 9, 12, 0, 0)"),
                740111.5, 1e-12);
}

TEST_F(DatenumTest, RowVecForm)
{
    EXPECT_DOUBLE_EQ(evalScalar("datenum([2026 5 9])"), 740111.0);
    EXPECT_NEAR(evalScalar("datenum([2026 5 9 12 30 45])"),
                740111.521354166666, 1e-9);
}

TEST_F(DatenumTest, MatrixForm)
{
    eval("y = datenum([2026 1 1; 2026 2 1; 2026 3 1]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 2)")), 1);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 739983.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 740014.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 740042.0);
}

TEST_F(DatenumTest, BroadcastVectorArgs)
{
    eval("y = datenum([2026; 2027; 2028], [1; 2; 3], [1; 15; 28]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 1)")), 3);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 739983.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 740393.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 740800.0);
}

TEST_F(DatenumTest, MonthOverflow)
{
    // Month 13 = January of next year; day 30 of Feb = March 2.
    EXPECT_DOUBLE_EQ(evalScalar("datenum(2026, 13, 9)"), 740356.0);
    EXPECT_DOUBLE_EQ(evalScalar("datenum(2026, 2, 30)"), 740043.0);
}

TEST_F(DatenumTest, NowAndDatenumConsistent)
{
    // datenum of today's components ≈ now (within a day).
    eval("n = now; "
         "yvec = datenum(2025, 1, 1);"
         "dvec = datenum(2030, 1, 1);");
    EXPECT_GT(evalScalar("n"), evalScalar("yvec"));
    EXPECT_LT(evalScalar("n"), evalScalar("dvec"));
}

// datenum(str [, fmt]): parse a date string. vs MATLAB R2025b. 2026-05-30:
// previously threw "string parsing not yet supported".
TEST_F(DatenumTest, StringParse)
{
    // Auto-detected ISO and dd-mmm-yyyy forms.
    EXPECT_DOUBLE_EQ(evalScalar("datenum('2022-12-30')"), 738885.0);
    EXPECT_DOUBLE_EQ(evalScalar("datenum('30-Dec-2022')"), 738885.0);
    EXPECT_NEAR(evalScalar("datenum('2022-12-30 12:34:56')"), 738885.5242592593, 1e-9);
    EXPECT_NEAR(evalScalar("datenum('30-Dec-2022 06:05:09')"), 738885.2535763889, 1e-9);
    // Explicit format string.
    EXPECT_DOUBLE_EQ(evalScalar("datenum('2022-12-30','yyyy-mm-dd')"), 738885.0);
    EXPECT_DOUBLE_EQ(evalScalar("datenum('30/12/2022','dd/mm/yyyy')"), 738885.0);
    // Round-trips against datestr.
    EXPECT_DOUBLE_EQ(
        evalScalar("datenum(datestr(738885.5,'yyyy-mm-dd HH:MM:SS'),"
                   "'yyyy-mm-dd HH:MM:SS')"),
        738885.5);
    // Numeric form is unchanged.
    EXPECT_DOUBLE_EQ(evalScalar("datenum(2022,12,30)"), 738885.0);
    // Unparseable string throws.
    EXPECT_THROW(eval("datenum('not a date')"), std::exception);
}
