// toolboxes/builtin/tests/diff_complex_test.cpp
//
// Regression guard for bugs/builtin/diff-complex.md (FIXED): diff() now
// differences the real AND imaginary parts of a complex array (honouring n
// and dim) instead of silently dropping the imaginary part. MATLAB R2025b.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class DiffComplexTest : public ::testing::Test
{
public:
    numkit::StandardEngine engine;
    void SetUp() override {}
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// diff([1+2i 4+6i 9+12i]) == [3+4i 5+6i].
TEST_F(DiffComplexTest, FirstOrderVector)
{
    eval("d = diff([1+2i 4+6i 9+12i]);");
    EXPECT_NEAR(evalScalar("real(d(1))"), 3.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(d(1))"), 4.0, 1e-12);
    EXPECT_NEAR(evalScalar("real(d(2))"), 5.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(d(2))"), 6.0, 1e-12);
}

// 2nd-order diff: diff([1+2i 4+6i 9+12i 16+20i], 2) == [2+2i 2+2i].
TEST_F(DiffComplexTest, SecondOrder)
{
    eval("d = diff([1+2i 4+6i 9+12i 16+20i], 2);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(d)")), 2);
    EXPECT_NEAR(evalScalar("real(d(1))"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(d(1))"), 2.0, 1e-12);
}

// Along dim 2: diff([1+1i 2+2i; 5+1i 6+3i], 1, 2) == [1+1i; 1+2i].
TEST_F(DiffComplexTest, AlongDim2)
{
    eval("d = diff([1+1i 2+2i; 5+1i 6+3i], 1, 2);");
    EXPECT_NEAR(evalScalar("real(d(1))"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(d(1))"), 1.0, 1e-12);  // row 1: (2+2i)-(1+1i)
    EXPECT_NEAR(evalScalar("real(d(2))"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(d(2))"), 2.0, 1e-12);  // row 2: (6+3i)-(5+1i)
}

// NOTE: diff(X,0) is intentionally NOT asserted here — MATLAB rejects n=0
// ("N must be a positive integer scalar"); numkit's acceptance of it is a
// separate pre-existing divergence (see bugs/builtin/diff-zero-order.md).
// The n=0 code path was only updated to preserve the complex parts.

// Real input must be unaffected.
TEST_F(DiffComplexTest, RealUnchanged)
{
    eval("d = diff([1 2 4 7]);");   // [1 2 3]
    EXPECT_DOUBLE_EQ(evalScalar("d(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("d(3)"), 3.0);
}
