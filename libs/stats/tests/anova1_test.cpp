// libs/stats/tests/anova1_test.cpp
//
// Regression guard for bugs/stats/anova1-matrix-input.md (fixed): anova1(X)
// accepts a data matrix whose COLUMNS are the groups. Expected values from
// MATLAB R2025b.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class Anova1Test : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(Anova1Test, MatrixColumnsAreGroups)
{
    // 3 columns -> 3 groups of 3; MATLAB p = 0.125.
    eval("p = anova1([1 2 3; 2 3 4; 3 4 5]);");
    EXPECT_NEAR(evalScalar("p"), 0.125, 1e-6);
}

TEST_F(Anova1Test, MatrixColumnsAreGroups2)
{
    // 3 columns x 4 rows; MATLAB p = 0.6999700518.
    eval("p = anova1([1 5 2; 7 3 8; 4 9 6; 2 1 5]);");
    EXPECT_NEAR(evalScalar("p"), 0.6999700518, 1e-6);
}

TEST_F(Anova1Test, MatrixTableSecondOutput)
{
    // [p,tbl] = anova1(X): MATLAB SSb=6, SSw=6, F=3 for this matrix.
    eval("[p, tbl] = anova1([1 2 3; 2 3 4; 3 4 5]);");
    EXPECT_NEAR(evalScalar("tbl{2,2}"), 6.0, 1e-9);  // SS Groups (between)
    EXPECT_NEAR(evalScalar("tbl{3,2}"), 6.0, 1e-9);  // SS Error  (within)
    EXPECT_NEAR(evalScalar("tbl{2,5}"), 3.0, 1e-9);  // F
}

TEST_F(Anova1Test, VectorGroupFormUnchanged)
{
    // Same data as the (y, group) form -> identical p (unchanged path).
    eval("p = anova1([1 2 3 2 3 4 3 4 5],[1 1 1 2 2 2 3 3 3]);");
    EXPECT_NEAR(evalScalar("p"), 0.125, 1e-6);
}
