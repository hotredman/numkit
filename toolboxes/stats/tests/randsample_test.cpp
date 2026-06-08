// toolboxes/stats/tests/randsample_test.cpp
//
// Regression guard for randsample's population-vector + weighted forms.
// Weights that put all mass on one element make the result deterministic
// regardless of RNG state, so these are stable. vs MATLAB R2025b.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class RandsampleTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// randsample(populationVector, k, true, weights): a row-vector population
// used to route to datasample(dim=1) and collapse N to 1 — weighted form
// erred ("weights length must equal sample-axis size"). Now samples along
// the vector's length.
TEST_F(RandsampleTest, WeightedRowPopulation)
{
    eval("y = randsample([10 20 30], 4, true, [0 0 1]);");  // all mass on 30
    EXPECT_DOUBLE_EQ(evalScalar("numel(y)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 30.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), 30.0);
    eval("y2 = randsample([10 20 30], 3, true, [1 0 0]);"); // all mass on 10
    EXPECT_DOUBLE_EQ(evalScalar("y2(2)"), 10.0);
}

TEST_F(RandsampleTest, WeightedColumnPopulationOrientation)
{
    eval("y = randsample([10;20;30], 2, true, [0 1 0]);");  // all mass on 20
    EXPECT_DOUBLE_EQ(evalScalar("size(y,1)"), 2.0);          // column in -> column out
    EXPECT_DOUBLE_EQ(evalScalar("size(y,2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 20.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 20.0);
}

TEST_F(RandsampleTest, UnweightedRowPopulationNoError)
{
    // Used to fail (N collapsed to 1 row, K=2 without replacement impossible).
    eval("y = randsample([10 20 30], 2);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(y)"), 2.0);
}

TEST_F(RandsampleTest, ScalarNFormWeighted)
{
    eval("s = randsample(5, 5, true, [1 0 0 0 0]);");  // all mass on index 1
    EXPECT_DOUBLE_EQ(evalScalar("s(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("s(5)"), 1.0);
}
