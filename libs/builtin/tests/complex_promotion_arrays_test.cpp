// libs/builtin/tests/complex_promotion_arrays_test.cpp
//
// Regression guard for bugs/builtin/complex-promotion-arrays.md (FIXED):
// sqrt/acosh/atanh now promote a whole real ARRAY to complex when any element
// is out of domain (previously only the scalar case promoted; array elements
// became NaN). atanh additionally uses the MATLAB branch for x<-1 (the
// std::atanh complex branch flips the imaginary sign), which also fixes the
// scalar atanh(-2). MATLAB R2025b reference values.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class ComplexPromotionArraysTest : public ::testing::Test
{
public:
    numkit::StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// sqrt([-1 4 -9]) == [0+1i 2 0+3i] (any negative promotes the whole array).
TEST_F(ComplexPromotionArraysTest, SqrtArray)
{
    eval("s = sqrt([-1 4 -9]);");
    EXPECT_TRUE(eval("~isreal(s)").toBool());
    EXPECT_NEAR(evalScalar("imag(s(1))"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("real(s(2))"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(s(2))"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(s(3))"), 3.0, 1e-12);
}

// acosh([0.5 2]) == [0+1.0472i 1.31696]; acosh(-2) == 1.31696 + pi*i.
TEST_F(ComplexPromotionArraysTest, AcoshArray)
{
    eval("h = acosh([0.5 2]);");
    EXPECT_TRUE(eval("~isreal(h)").toBool());
    EXPECT_NEAR(evalScalar("imag(h(1))"), 1.0471975511965979, 1e-12);
    EXPECT_NEAR(evalScalar("real(h(2))"), 1.3169578969248166, 1e-12);
    EXPECT_NEAR(evalScalar("imag(h(2))"), 0.0, 1e-12);
    eval("hn = acosh(-2);");
    EXPECT_NEAR(evalScalar("real(hn)"), 1.3169578969248166, 1e-12);
    EXPECT_NEAR(evalScalar("imag(hn)"), 3.141592653589793,  1e-12);
}

// atanh([2 -2 0.5]) == [0.5493+1.5708i, -0.5493-1.5708i, 0.5493] (MATLAB branch).
TEST_F(ComplexPromotionArraysTest, AtanhArray)
{
    eval("t = atanh([2 -2 0.5]);");
    EXPECT_TRUE(eval("~isreal(t)").toBool());
    EXPECT_NEAR(evalScalar("real(t(1))"),  0.5493061443340549, 1e-12);
    EXPECT_NEAR(evalScalar("imag(t(1))"),  1.5707963267948966, 1e-12);
    EXPECT_NEAR(evalScalar("real(t(2))"), -0.5493061443340549, 1e-12);
    EXPECT_NEAR(evalScalar("imag(t(2))"), -1.5707963267948966, 1e-12);
    EXPECT_NEAR(evalScalar("real(t(3))"),  0.5493061443340549, 1e-12);
    EXPECT_NEAR(evalScalar("imag(t(3))"),  0.0,                1e-12);
}

// Scalar atanh(-2): the imaginary sign must match MATLAB (-pi/2, not +pi/2).
TEST_F(ComplexPromotionArraysTest, AtanhScalarNegativeSign)
{
    eval("a = atanh(-2);");
    EXPECT_NEAR(evalScalar("real(a)"), -0.5493061443340549, 1e-12);
    EXPECT_NEAR(evalScalar("imag(a)"), -1.5707963267948966, 1e-12);
}

// In-domain input must stay REAL (no spurious promotion).
TEST_F(ComplexPromotionArraysTest, InDomainStaysReal)
{
    EXPECT_TRUE(eval("isreal(sqrt([1 4 9]))").toBool());
    EXPECT_TRUE(eval("isreal(acosh([1 2 5]))").toBool());
    EXPECT_TRUE(eval("isreal(atanh([0 0.5 -0.5]))").toBool());
    EXPECT_NEAR(evalScalar("max(sqrt([1 4 9]))"), 3.0, 1e-12);
}
