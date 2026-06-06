// libs/builtin/tests/testmatrices2_test.cpp
//
// Regression guard for the test-matrix demo group:
//   pascal, hilb, invhilb, wilkinson, hadamard, rosser

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class TestMatrices2Test : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── pascal ──────────────────────────────────────────────────

TEST_F(TestMatrices2Test, PascalSymmetric)
{
    eval("P = pascal(5);");
    EXPECT_DOUBLE_EQ(evalScalar("P(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("P(2,2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("P(3,3)"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("P(4,4)"), 20.0);
    EXPECT_DOUBLE_EQ(evalScalar("P(5,5)"), 70.0);
    EXPECT_DOUBLE_EQ(evalScalar("max(max(abs(P - P')))"), 0.0);  // symmetric
    // First column / first row are all ones.
    EXPECT_DOUBLE_EQ(evalScalar("sum(P(:,1))"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(P(1,:))"), 5.0);
}

// ── hilb ────────────────────────────────────────────────────

TEST_F(TestMatrices2Test, HilbExactValues)
{
    eval("H = hilb(4);");
    EXPECT_DOUBLE_EQ(evalScalar("H(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("H(2,2)"), 1.0/3.0);
    EXPECT_DOUBLE_EQ(evalScalar("H(4,4)"), 1.0/7.0);
    EXPECT_DOUBLE_EQ(evalScalar("H(2,3)"), 0.25);
    EXPECT_DOUBLE_EQ(evalScalar("max(max(abs(H - H')))"), 0.0);  // symmetric
}

// ── invhilb ─────────────────────────────────────────────────

TEST_F(TestMatrices2Test, InvHilbInverseProperty)
{
    // hilb(n) * invhilb(n) ≈ I. Loose tolerance for n=8 because
    // both engines lose ~1e-3 ULPs through the binomial overflow.
    eval("H = hilb(4); IH = invhilb(4); P = H * IH;");
    EXPECT_NEAR(evalScalar("max(max(abs(P - eye(4))))"), 0.0, 1e-9);
}

TEST_F(TestMatrices2Test, InvHilb4ExactIntegers)
{
    eval("IH = invhilb(4);");
    // Diagonal entries of invhilb(4) are exact small integers.
    EXPECT_DOUBLE_EQ(evalScalar("IH(1,1)"),    16.0);
    EXPECT_DOUBLE_EQ(evalScalar("IH(2,2)"),  1200.0);
    EXPECT_DOUBLE_EQ(evalScalar("IH(3,3)"),  6480.0);
    EXPECT_DOUBLE_EQ(evalScalar("IH(4,4)"),  2800.0);  // MATLAB invhilb(4)(end,end)
    EXPECT_DOUBLE_EQ(evalScalar("IH(2,3)"), -2700.0);
}

// ── wilkinson ───────────────────────────────────────────────

TEST_F(TestMatrices2Test, WilkinsonOddOrder)
{
    eval("W = wilkinson(7);");
    // Diagonal: |1..7 - 4| = [3 2 1 0 1 2 3].
    EXPECT_DOUBLE_EQ(evalScalar("W(1,1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("W(4,4)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("W(7,7)"), 3.0);
    // Subdiagonal/superdiagonal: ones.
    EXPECT_DOUBLE_EQ(evalScalar("W(2,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("W(2,3)"), 1.0);
    // Tridiagonal: zeros elsewhere.
    EXPECT_DOUBLE_EQ(evalScalar("W(1,3)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("W(1,7)"), 0.0);
}

TEST_F(TestMatrices2Test, WilkinsonEvenOrderHalfIntegers)
{
    eval("W = wilkinson(8);");
    // Diagonal: |(1:8) - 4.5| = [3.5 2.5 1.5 0.5 0.5 1.5 2.5 3.5].
    EXPECT_DOUBLE_EQ(evalScalar("W(1,1)"), 3.5);
    EXPECT_DOUBLE_EQ(evalScalar("W(4,4)"), 0.5);
    EXPECT_DOUBLE_EQ(evalScalar("W(5,5)"), 0.5);
}

// ── hadamard ────────────────────────────────────────────────

TEST_F(TestMatrices2Test, HadamardOrthogonalRows)
{
    // H * H' = N * I for a Hadamard matrix.
    for (int N : {1, 2, 4, 8, 16, 32}) {
        const std::string sn = std::to_string(N);
        eval("H = hadamard(" + sn + "); D = H * H';");
        EXPECT_DOUBLE_EQ(evalScalar("max(max(abs(D - " + sn + "*eye(" + sn + "))))"), 0.0);
    }
}

TEST_F(TestMatrices2Test, HadamardEntriesArePlusMinusOne)
{
    eval("H = hadamard(8);");
    EXPECT_DOUBLE_EQ(evalScalar("max(abs(H(:)))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("min(abs(H(:)))"), 1.0);
}

TEST_F(TestMatrices2Test, HadamardNonPow2Rejected)
{
    EXPECT_THROW(eval("hadamard(3);"), std::exception);
    EXPECT_THROW(eval("hadamard(6);"), std::exception);
    // 12 and 20 are valid in MATLAB but deferred here -- should throw too.
    EXPECT_THROW(eval("hadamard(12);"), std::exception);
}

// ── rosser ──────────────────────────────────────────────────

TEST_F(TestMatrices2Test, RosserHardcodedConstants)
{
    eval("R = rosser();");
    EXPECT_DOUBLE_EQ(evalScalar("R(1,1)"),  611.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(2,2)"),  899.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(8,8)"),   99.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(7,8)"), -911.0);
    EXPECT_DOUBLE_EQ(evalScalar("max(max(abs(R - R')))"), 0.0);  // symmetric
}
