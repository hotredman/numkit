// toolboxes/stats/tests/friedman_test.cpp
//
// Regression guard for friedman() — Friedman nonparametric two-way ANOVA by
// ranks. Expected p-values from MATLAB R2025b. bugs/stats/friedman.
// numkit returns [p, Q, df] (statistic + df), not MATLAB's (tbl, stats); the
// primary p matches MATLAB exactly.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class FriedmanTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(FriedmanTest, NoTies)
{
    // MATLAB: friedman([1 2 3;2 3 4;3 4 5;1 3 5], 1) = 0.0183156388887, Q=8, df=2.
    EXPECT_NEAR(evalScalar("friedman([1 2 3;2 3 4;3 4 5;1 3 5], 1)"),
                0.0183156388887, 1e-10);
    eval("[p, Q, df] = friedman([1 2 3;2 3 4;3 4 5;1 3 5], 1);");
    EXPECT_NEAR(evalScalar("Q"), 8.0, 1e-10);
    EXPECT_DOUBLE_EQ(evalScalar("df"), 2.0);
}

TEST_F(FriedmanTest, WithTies)
{
    // MATLAB: friedman([7 9 8;6 5 7;9 7 6;8 8 9;5 6 5], 1) = 0.846481724891.
    // Exercises the tie correction C = 1 - sum(t^3-t)/(n*(k^3-k)).
    EXPECT_NEAR(evalScalar("friedman([7 9 8;6 5 7;9 7 6;8 8 9;5 6 5], 1)"),
                0.846481724891, 1e-10);
}

TEST_F(FriedmanTest, DefaultRepsOmitted)
{
    // reps defaults to 1 when omitted.
    EXPECT_NEAR(evalScalar("friedman([1 2 3;2 3 4;3 4 5;1 3 5])"),
                0.0183156388887, 1e-10);
}
