// libs/stats/tests/anova2_test.cpp
//
// Regression guard for anova2 (two-way ANOVA without replication).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class Anova2Test : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(Anova2Test, ClassicFertilizerExample)
{
    // 4 fertilizers x 3 fields, no replication.
    eval("Y = [55 26 78; 60 24 73; 70 28 75; 65 27 80]; p = anova2(Y);");
    EXPECT_NEAR(evalScalar("p(1)"), 4.72675e-06, 1e-10);   // strong column effect
    EXPECT_NEAR(evalScalar("p(2)"), 0.297202,    1e-5);    // no row effect
    EXPECT_TRUE(std::isnan(evalScalar("p(3)")));            // no interaction (reps=1)
}

TEST_F(Anova2Test, StrongRowEffect)
{
    // Rows differ massively, columns identical -> p_rows ~0, p_cols ~1.
    eval("Y = [1 1 1; 5 5 5; 10 10 10]; p = anova2(Y);");
    EXPECT_NEAR(evalScalar("p(2)"), 0.0, 1e-10);
    EXPECT_GT(evalScalar("p(1)"), 0.5);
}

TEST_F(Anova2Test, OutputShape)
{
    eval("Y = [1 2; 3 4; 5 6]; p = anova2(Y);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(p, 1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(p, 2)")), 3);
}

TEST_F(Anova2Test, TableOutput)
{
    eval("Y = [55 26 78; 60 24 73; 70 28 75; 65 27 80]; [p, tbl] = anova2(Y);");
    // Table is 5x6 cell with header row.
    EXPECT_EQ(static_cast<int>(evalScalar("size(tbl, 1)")), 5);
    EXPECT_EQ(static_cast<int>(evalScalar("size(tbl, 2)")), 6);
}

TEST_F(Anova2Test, RepsGreaterThanOneRejected)
{
    EXPECT_THROW(eval("anova2([1 2; 3 4], 2);"), std::exception);
}

TEST_F(Anova2Test, TooSmallRejected)
{
    EXPECT_THROW(eval("anova2([1; 2]);"), std::exception);
    EXPECT_THROW(eval("anova2([1 2 3]);"), std::exception);
}
