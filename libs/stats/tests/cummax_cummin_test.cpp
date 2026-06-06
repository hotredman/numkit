// libs/stats/tests/cummax_cummin_test.cpp

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class CummaxCumminTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override
    {
        engine.eval("import compat.*;");
        engine.eval("A = [1 3 2 5 4 6 NaN 8 7 10]';");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── cummax ────────────────────────────────────────────────────────────

TEST_F(CummaxCumminTest, CummaxDefault)
{
    eval("y = cummax(A);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"),  1);
    EXPECT_DOUBLE_EQ(evalScalar("y(6)"),  6);
    EXPECT_DOUBLE_EQ(evalScalar("y(10)"), 10);
}

TEST_F(CummaxCumminTest, CummaxReverse)
{
    eval("y = cummax(A, 'reverse');");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"),  10);
    EXPECT_DOUBLE_EQ(evalScalar("y(10)"), 10);
}

TEST_F(CummaxCumminTest, CummaxIncludenan)
{
    eval("y = cummax(A, 'includenan');");
    EXPECT_DOUBLE_EQ(evalScalar("y(6)"),  6);
    EXPECT_TRUE(std::isnan(evalScalar("y(7)")));
    EXPECT_TRUE(std::isnan(evalScalar("y(8)")));
    EXPECT_TRUE(std::isnan(evalScalar("y(10)")));
}

TEST_F(CummaxCumminTest, CummaxReverseIncludenan)
{
    eval("y = cummax(A, 'reverse', 'includenan');");
    EXPECT_TRUE(std::isnan(evalScalar("y(1)")));
    EXPECT_TRUE(std::isnan(evalScalar("y(7)")));
    EXPECT_DOUBLE_EQ(evalScalar("y(8)"),  10);
    EXPECT_DOUBLE_EQ(evalScalar("y(10)"), 10);
}

TEST_F(CummaxCumminTest, CummaxOmitnanExplicit)
{
    eval("y_def = cummax(A); y_om = cummax(A, 'omitnan');");
    EXPECT_DOUBLE_EQ(evalScalar("max(abs(y_def - y_om))"), 0.0);
}

// ── cummin (mirrors cummax) ────────────────────────────────────────────

TEST_F(CummaxCumminTest, CumminDefault)
{
    eval("y = cummin(A);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 1);
    EXPECT_DOUBLE_EQ(evalScalar("y(10)"), 1);
}

TEST_F(CummaxCumminTest, CumminReverseIncludenan)
{
    eval("y = cummin(A, 'reverse', 'includenan');");
    EXPECT_TRUE(std::isnan(evalScalar("y(1)")));
    EXPECT_TRUE(std::isnan(evalScalar("y(7)")));
    EXPECT_DOUBLE_EQ(evalScalar("y(8)"),  7);
}
