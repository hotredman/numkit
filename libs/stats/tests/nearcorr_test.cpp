// libs/stats/tests/nearcorr_test.cpp
//
// Regression guard for nearcorr() — nearest correlation matrix
// (Higham 2002 alternating projections + Dykstra correction).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class NearcorrTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Already-correlation input -> output equals input (no work to do).
TEST_F(NearcorrTest, IdentityCaseUnchanged)
{
    eval("C0 = [1 0.3 -0.2; 0.3 1 0.1; -0.2 0.1 1];"
         "Y0 = nearcorr(C0);");
    EXPECT_NEAR(evalScalar("Y0(1, 1)"),  1.0, 1e-12);
    EXPECT_NEAR(evalScalar("Y0(1, 2)"),  0.3, 1e-9);
    EXPECT_NEAR(evalScalar("Y0(1, 3)"), -0.2, 1e-9);
    EXPECT_NEAR(evalScalar("Y0(2, 3)"),  0.1, 1e-9);
}

// Higham's classic 3x3 textbook example.
// Input has min eigval ~ -0.20; nearest correlation matrix has
// off-diag entries [-0.4041, 0.4988, 0.5912] (matches MATLAB nearcorr).
TEST_F(NearcorrTest, HighamTextbook3x3)
{
    eval("A1 = [1 -0.5 0.6; -0.5 1 0.7; 0.6 0.7 1];"
         "Y1 = nearcorr(A1);");
    EXPECT_NEAR(evalScalar("Y1(1, 1)"),  1.0, 1e-12);
    EXPECT_NEAR(evalScalar("Y1(1, 2)"), -0.4041, 1e-3);
    EXPECT_NEAR(evalScalar("Y1(1, 3)"),  0.4988, 1e-3);
    EXPECT_NEAR(evalScalar("Y1(2, 3)"),  0.5912, 1e-3);
}

// Output is symmetric.
TEST_F(NearcorrTest, OutputIsSymmetric)
{
    eval("A1 = [1 -0.5 0.6; -0.5 1 0.7; 0.6 0.7 1];"
         "Y1 = nearcorr(A1);"
         "asym = max(max(abs(Y1 - Y1')));");
    EXPECT_LE(evalScalar("asym"), 1e-14);
}

// Output has unit diagonal.
TEST_F(NearcorrTest, UnitDiagonal)
{
    eval("A1 = [1 -0.5 0.6; -0.5 1 0.7; 0.6 0.7 1];"
         "Y1 = nearcorr(A1);");
    EXPECT_DOUBLE_EQ(evalScalar("Y1(1, 1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("Y1(2, 2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("Y1(3, 3)"), 1.0);
}

// Output is PSD: smallest eigenvalue is non-negative (within tol).
TEST_F(NearcorrTest, OutputIsPSD)
{
    eval("A1 = [1 -0.5 0.6; -0.5 1 0.7; 0.6 0.7 1];"
         "Y1 = nearcorr(A1);"
         "minEv = min(eig(Y1));");
    EXPECT_GE(evalScalar("minEv"), -1e-9);
}

// Larger 4x4 indefinite case stays bit-equal with MATLAB on
// off-diag entries.
TEST_F(NearcorrTest, FourByFourIndefinite)
{
    eval("A2 = [1.0 0.9 0.7 0.6;"
         "      0.9 1.0 0.8 0.95;"
         "      0.7 0.8 1.0 0.9;"
         "      0.6 0.95 0.9 1.0];"
         "Y2 = nearcorr(A2);");
    // MATLAB-derived expected values (parity-validated).
    EXPECT_NEAR(evalScalar("Y2(1, 2)"), 0.872544, 1e-4);
    EXPECT_NEAR(evalScalar("Y2(3, 4)"), 0.886723, 1e-4);
    EXPECT_DOUBLE_EQ(evalScalar("Y2(1, 1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("Y2(4, 4)"), 1.0);
}

// 1x1 trivial case.
TEST_F(NearcorrTest, OneByOneScalar)
{
    eval("Y = nearcorr([1.0]);");
    EXPECT_DOUBLE_EQ(evalScalar("Y(1, 1)"), 1.0);
}

// Empty input passes through.
TEST_F(NearcorrTest, EmptyInput)
{
    eval("Y = nearcorr(zeros(0, 0));");
    EXPECT_EQ(static_cast<int>(evalScalar("size(Y, 1)")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("size(Y, 2)")), 0);
}

// Non-square input throws.
TEST_F(NearcorrTest, NonSquareThrows)
{
    bool threw = false;
    try { eval("nearcorr([1 0.3; 0.3 1; 0 0]);"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}
