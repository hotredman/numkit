// libs/signal/tests/conv_complex_test.cpp
//
// Regression guard for the conv part of
// bugs/builtin/complex-input-unsupported.md (umbrella; conv now FIXED).
// conv is BILINEAR, so a complex input needs a genuine complex
// multiply-accumulate (full[n] = sum_k a[k]*b[n-k]), NOT a real/imag split.
// 'full' / 'same' / 'valid' trims all carry through. MATLAB R2025b.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class ConvComplexTest : public ::testing::Test
{
public:
    numkit::StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// conv([1 1i],[1 1]) == [1  1+1i  1i].
TEST_F(ConvComplexTest, Full)
{
    eval("c = conv([1 1i], [1 1]);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(c)")), 3);
    EXPECT_NEAR(evalScalar("real(c(1))"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(c(1))"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("real(c(2))"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(c(2))"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("real(c(3))"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(c(3))"), 1.0, 1e-12);
}

// Complex x complex: cross terms matter (a split would be wrong).
TEST_F(ConvComplexTest, ComplexByComplex)
{
    eval("c = conv([1+1i 2-1i 3i], [2 1i]);");
    EXPECT_NEAR(evalScalar("real(c(2))"), 3.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(c(2))"), -1.0, 1e-12);
    EXPECT_NEAR(evalScalar("real(c(3))"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(c(3))"), 8.0, 1e-12);
}

// Complex x real.
TEST_F(ConvComplexTest, ComplexByReal)
{
    eval("c = conv([1 2 3], [1+1i 1]);");   // c(2) = 1*1 + 2*(1+1i) = 3+2i
    EXPECT_NEAR(evalScalar("real(c(2))"), 3.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(c(2))"), 2.0, 1e-12);
}

// 'same' keeps the central part the size of A.
TEST_F(ConvComplexTest, Same)
{
    eval("c = conv([1+1i 2 3-1i 4], [1 1], 'same');");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(c)")), 4);
    EXPECT_NEAR(evalScalar("real(c(1))"), 3.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(c(1))"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("real(c(2))"), 5.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(c(2))"), -1.0, 1e-12);
}

// 'valid' keeps only the fully-overlapping part.
TEST_F(ConvComplexTest, Valid)
{
    eval("c = conv([1+1i 2 3-1i 4], [1 1], 'valid');");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(c)")), 3);
    EXPECT_NEAR(evalScalar("real(c(1))"), 3.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(c(1))"), 1.0, 1e-12);
}

// Real input must be unaffected.
TEST_F(ConvComplexTest, RealUnchanged)
{
    eval("c = conv([1 2 3], [1 1]);");   // [1 3 5 3]
    EXPECT_DOUBLE_EQ(evalScalar("c(2)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(3)"), 5.0);
    EXPECT_TRUE(eval("isreal(c)").toBool());
}
