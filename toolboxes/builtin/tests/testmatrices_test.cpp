// toolboxes/builtin/tests/testmatrices_test.cpp
//
// Regression guard for builtin test-matrix family:
//   toeplitz, hankel, vander, compan
//
// Hardcoded expected values captured from MATLAB R2025b parity probes.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class TestMatricesTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── toeplitz ────────────────────────────────────────────────

TEST_F(TestMatricesTest, ToeplitzSymmetricSingleArg)
{
    eval("T = toeplitz([1 2 3 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("T(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("T(1,4)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("T(4,1)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("T(2,3)"), 2.0);
    // Symmetric.
    EXPECT_DOUBLE_EQ(evalScalar("max(max(abs(T - T')))"), 0.0);
}

TEST_F(TestMatricesTest, ToeplitzRectangularTwoArg)
{
    eval("T = toeplitz([1 2 3], [1 4 5 6]);");
    EXPECT_DOUBLE_EQ(evalScalar("T(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("T(1,4)"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("T(3,1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("T(3,4)"), 4.0);
}

// ── hankel ──────────────────────────────────────────────────

TEST_F(TestMatricesTest, HankelSingleArg)
{
    // Anti-triangular: trailing zeros below the anti-diagonal.
    eval("H = hankel([1 2 3 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("H(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("H(1,4)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("H(4,1)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("H(4,4)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("H(2,3)"), 4.0);
}

TEST_F(TestMatricesTest, HankelTwoArgRectangular)
{
    eval("H = hankel([1 2 3], [3 5 6 7]);");
    EXPECT_DOUBLE_EQ(evalScalar("H(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("H(3,1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("H(3,4)"), 7.0);
    EXPECT_DOUBLE_EQ(evalScalar("H(1,4)"), 5.0);
}

// DEEP-PROBE 2026-05-31: toeplitz/hankel were DOUBLE-only — they dropped
// the imaginary part of COMPLEX inputs and down-converted SINGLE.
TEST_F(TestMatricesTest, ToeplitzComplexAndSingle)
{
    // Single-arg COMPLEX -> Hermitian Toeplitz (lower triangle conjugated).
    eval("T = toeplitz([1+1i 2+2i 3+3i]);");
    EXPECT_DOUBLE_EQ(evalScalar("real(T(1,1))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("imag(T(1,1))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("real(T(2,1))"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("imag(T(2,1))"), -2.0);  // conj(2+2i)
    EXPECT_DOUBLE_EQ(evalScalar("imag(T(1,2))"), 2.0);   // c(2), not conj

    // Two-arg COMPLEX -> plain gather (no conjugation).
    eval("T2 = toeplitz([1+1i 2 3],[1-9i 8 7]);");
    EXPECT_DOUBLE_EQ(evalScalar("imag(T2(1,1))"), 1.0);  // column wins
    EXPECT_DOUBLE_EQ(evalScalar("real(T2(2,1))"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("imag(T2(2,1))"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("real(T2(1,2))"), 8.0);  // r(2)

    // SINGLE preserved.
    eval("Ts = toeplitz(single([1 2 3]));");
    EXPECT_DOUBLE_EQ(evalScalar("double(strcmp(class(Ts),'single'))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(Ts(2,1))"), 2.0);
}

TEST_F(TestMatricesTest, HankelComplexAndSingle)
{
    // hankel NEVER conjugates.
    eval("H = hankel([1+1i 2+2i 3+3i]);");
    EXPECT_DOUBLE_EQ(evalScalar("real(H(1,1))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("imag(H(1,1))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("real(H(2,2))"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("imag(H(2,2))"), 3.0);   // c(3)=3+3i

    // Two-arg complex keeps r's imaginary part.
    eval("H2 = hankel([1+1i 2 3],[3 4 5+5i]);");
    EXPECT_DOUBLE_EQ(evalScalar("real(H2(3,3))"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("imag(H2(3,3))"), 5.0);

    // SINGLE preserved.
    eval("Hs = hankel(single([1 2 3]));");
    EXPECT_DOUBLE_EQ(evalScalar("double(strcmp(class(Hs),'single'))"), 1.0);
}

// ── vander ──────────────────────────────────────────────────

TEST_F(TestMatricesTest, VanderHighestPowerLeft)
{
    eval("V = vander([1 2 3 4]);");
    // Last column is all ones (v^0).
    EXPECT_DOUBLE_EQ(evalScalar("V(1,4)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("V(2,4)"), 1.0);
    // First column is v^(n-1).
    EXPECT_DOUBLE_EQ(evalScalar("V(2,1)"),  8.0);   // 2^3
    EXPECT_DOUBLE_EQ(evalScalar("V(3,1)"), 27.0);   // 3^3
    EXPECT_DOUBLE_EQ(evalScalar("V(4,1)"), 64.0);   // 4^3
    // V(i,j) = v(i)^(n-j); spot-check column 2 (= v^2).
    EXPECT_DOUBLE_EQ(evalScalar("V(3,2)"), 9.0);
    EXPECT_DOUBLE_EQ(evalScalar("V(4,2)"), 16.0);
}

TEST_F(TestMatricesTest, VanderNonInteger)
{
    eval("V = vander([0.5 -1 2]);");
    // n = 3; powers (n-1=2, 1, 0) = [v^2, v, 1].
    EXPECT_DOUBLE_EQ(evalScalar("V(1,1)"), 0.25);
    EXPECT_DOUBLE_EQ(evalScalar("V(2,2)"), -1.0);
    EXPECT_DOUBLE_EQ(evalScalar("V(3,1)"), 4.0);
}

// ── compan ──────────────────────────────────────────────────

TEST_F(TestMatricesTest, CompanRootsRecovered)
{
    // p(x) = x^3 - 7x + 6 has roots 1, 2, -3.
    eval("C = compan([1 0 -7 6]);");
    EXPECT_DOUBLE_EQ(evalScalar("C(1,1)"),  0.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(1,2)"),  7.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(1,3)"), -6.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(2,1)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(3,2)"),  1.0);
}

TEST_F(TestMatricesTest, CompanLeadingNonOne)
{
    // p(x) = 2x^2 - 3x + 1 -- top row gets divided by leading 2.
    eval("C = compan([2 -3 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("C(1,1)"), 1.5);
    EXPECT_DOUBLE_EQ(evalScalar("C(1,2)"), -0.5);
    EXPECT_DOUBLE_EQ(evalScalar("C(2,1)"), 1.0);
}

TEST_F(TestMatricesTest, CompanZeroLeadingRejected)
{
    EXPECT_THROW(eval("compan([0 1 2]);"), std::exception);
}
