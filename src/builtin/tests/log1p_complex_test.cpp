// toolboxes/builtin/tests/log1p_complex_test.cpp
//
// Regression guard for the log1p part of bugs/builtin/log-complex-promotion-
// arrays.md (log family): log1p(x) = log(1+x) goes complex for x < -1
// (log1p(-2) = i*pi), any array element < -1 promotes the whole real array,
// and complex input uses log(1+z). The real (x >= -1) path keeps the accurate
// log1p (log1p(1e-15) == 1e-15, not the lossy log(1+1e-15)). MATLAB R2025b.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class Log1pComplexTest : public ::testing::Test
{
public:
    numkit::StandardEngine engine;
    void SetUp() override {}
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Scalar x < -1 promotes to complex: log1p(-2) = log(-1) = i*pi.
TEST_F(Log1pComplexTest, ScalarBelowMinusOne)
{
    eval("s = log1p(-2);");
    EXPECT_TRUE(eval("~isreal(s)").toBool());
    EXPECT_NEAR(evalScalar("real(s)"), 0.0,               1e-12);
    EXPECT_NEAR(evalScalar("imag(s)"), 3.141592653589793, 1e-12);
    // log1p(-3) = log(-2) = log(2) + i*pi
    eval("t = log1p(-3);");
    EXPECT_NEAR(evalScalar("real(t)"), 0.6931471805599453, 1e-12);
    EXPECT_NEAR(evalScalar("imag(t)"), 3.141592653589793,  1e-12);
}

// Array with any element < -1 promotes the whole array; per-element accurate.
TEST_F(Log1pComplexTest, ArrayPromotes)
{
    eval("a = log1p([-2 -0.5 0 3]);");
    EXPECT_TRUE(eval("~isreal(a)").toBool());
    EXPECT_NEAR(evalScalar("real(a(1))"), 0.0,                1e-12);  // log(-1)
    EXPECT_NEAR(evalScalar("imag(a(1))"), 3.141592653589793, 1e-12);
    EXPECT_NEAR(evalScalar("real(a(2))"), -0.6931471805599453, 1e-12); // log(0.5)
    EXPECT_NEAR(evalScalar("imag(a(2))"), 0.0,                1e-12);
    EXPECT_NEAR(evalScalar("real(a(3))"), 0.0,                1e-12);  // log(1)
    EXPECT_NEAR(evalScalar("real(a(4))"), 1.3862943611198906, 1e-12); // log(4)
}

// The real (x >= -1) path keeps accurate log1p inside a promoted array.
TEST_F(Log1pComplexTest, AccuracyPreservedInPromotedArray)
{
    eval("b = log1p([-2, 1e-15]);");
    // accurate log1p -> 1e-15 (NOT the lossy log(1+1e-15) = 1.11e-15)
    EXPECT_NEAR(evalScalar("real(b(2))"), 1e-15, 1e-27);
    EXPECT_NEAR(evalScalar("imag(b(2))"), 0.0,   1e-12);
}

// Complex input: log1p(z) = log(1+z).
TEST_F(Log1pComplexTest, ComplexInput)
{
    eval("z = log1p(3+4i);");
    EXPECT_NEAR(evalScalar("real(z)"), 1.7328679513998633, 1e-12);
    EXPECT_NEAR(evalScalar("imag(z)"), 0.7853981633974483, 1e-12);
}

// In-domain (x >= -1) input stays real and accurate; x == -1 -> -Inf.
TEST_F(Log1pComplexTest, InDomainStaysReal)
{
    EXPECT_TRUE(eval("isreal(log1p([0 1 5]))").toBool());
    EXPECT_NEAR(evalScalar("log1p(1e-15)"), 1e-15, 1e-27);
    EXPECT_NEAR(evalScalar("log1p(1)"),     0.6931471805599453, 1e-12);
    EXPECT_TRUE(eval("isinf(log1p(-1))").toBool());        // log(0) = -Inf
    EXPECT_LT(evalScalar("log1p(-1)"), 0.0);               // -Inf
}
