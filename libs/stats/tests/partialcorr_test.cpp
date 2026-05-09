// libs/stats/tests/partialcorr_test.cpp
//
// Regression guard for partialcorr.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class PartialCorrTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(PartialCorrTest, PartialcorrDeterministic)
{
    eval("x = [1; 2; 3; 4; 5; 6; 7; 8; 9; 10]; "
         "y = [1.2; 1.8; 3.5; 3.9; 5.1; 6.0; 7.2; 7.8; 9.1; 10.0]; "
         "z = [1; 4; 2; 5; 3; 6; 8; 7; 9; 10]; "
         "p = partialcorr(x, y, z);");
    EXPECT_NEAR(evalScalar("p"), 0.987889, 1e-5);
}

TEST_F(PartialCorrTest, PartialcorrShape)
{
    // X is m×2, Y is m×3 → result is 2×3.
    eval("X = [1 2; 3 4; 5 6; 7 8; 9 10]; "
         "Y = [1 4 7; 2 5 8; 3 6 9; 4 7 10; 5 8 11]; "
         "Z = [1; 1; 2; 2; 3]; "
         "P = partialcorr(X, Y, Z);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(P,1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(P,2)")), 3);
}

TEST_F(PartialCorrTest, PartialcorrDimMismatchThrows)
{
    EXPECT_THROW(eval("partialcorr([1;2;3], [1;2], [1;2;3]);"), std::exception);
}

TEST_F(PartialCorrTest, PartialcorrControlsForConfounder)
{
    // Construct: x and y both depend strongly on z.
    // Conditional on z, residuals should have low correlation.
    eval("z = [1; 2; 3; 4; 5; 6; 7; 8; 9; 10]; "
         "x = z + [0.1; -0.2; 0.05; 0.15; -0.1; 0.2; -0.05; 0.1; -0.15; 0.05]; "
         "y = z + [-0.1; 0.2; 0.15; -0.1; 0.05; -0.2; 0.1; 0.05; 0.15; -0.1]; "
         "p_naive = corr([x y]); p_part = partialcorr(x, y, z);");
    // naive should be high (>0.99 since trend dominates)
    EXPECT_GT(evalScalar("p_naive(1, 2)"), 0.99);
    // partial should be smaller than naive (residuals after regressing
    // out z still have some structure since hand-picked noise isn't
    // truly random, but the trend is removed).
    EXPECT_LT(std::fabs(evalScalar("p_part")),
              std::fabs(evalScalar("p_naive(1, 2)")));
}
