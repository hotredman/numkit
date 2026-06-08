// toolboxes/builtin/tests/acos_asin_complex_test.cpp
//
// Regression guard for bugs/builtin/acos-asin-complex.md (FIXED): acos/asin of
// a REAL argument outside [-1,1] now return the MATLAB-correct complex value
// (computed via acosh so the imaginary sign matches MATLAB's branch cut), and
// promote the whole array to complex if any element is out of range.
// MATLAB R2025b reference values. acosh(2) = 1.3169578969248166.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class AcosAsinComplexTest : public ::testing::Test
{
public:
    numkit::StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// acos(x>1) = 0 + i*acosh(x); acos(x<-1) = pi - i*acosh(|x|).
TEST_F(AcosAsinComplexTest, AcosOutOfDomain)
{
    eval("a = acos(2);");
    EXPECT_NEAR(evalScalar("real(a)"), 0.0,                1e-12);
    EXPECT_NEAR(evalScalar("imag(a)"), 1.3169578969248166, 1e-12);
    eval("b = acos(-2);");
    EXPECT_NEAR(evalScalar("real(b)"), 3.141592653589793,  1e-12);
    EXPECT_NEAR(evalScalar("imag(b)"), -1.3169578969248166, 1e-12);
}

// asin(x>1) = pi/2 - i*acosh(x); asin(x<-1) = -pi/2 + i*acosh(|x|).
TEST_F(AcosAsinComplexTest, AsinOutOfDomain)
{
    eval("a = asin(2);");
    EXPECT_NEAR(evalScalar("real(a)"),  1.5707963267948966, 1e-12);
    EXPECT_NEAR(evalScalar("imag(a)"), -1.3169578969248166, 1e-12);
    eval("b = asin(-2);");
    EXPECT_NEAR(evalScalar("real(b)"), -1.5707963267948966, 1e-12);
    EXPECT_NEAR(evalScalar("imag(b)"),  1.3169578969248166, 1e-12);
}

// Any out-of-range element promotes the whole array; in-range elements keep
// their real value (imag 0).
TEST_F(AcosAsinComplexTest, ArrayPromotion)
{
    eval("v = acos([0.5 2]);");
    EXPECT_TRUE(eval("~isreal(v)").toBool());
    EXPECT_NEAR(evalScalar("real(v(1))"), 1.0471975511965979, 1e-12);  // acos(0.5)
    EXPECT_NEAR(evalScalar("imag(v(1))"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("real(v(2))"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(v(2))"), 1.3169578969248166, 1e-12);
}

// In-domain input stays REAL (no spurious complex promotion).
TEST_F(AcosAsinComplexTest, InDomainStaysReal)
{
    EXPECT_TRUE(eval("isreal(acos(0.5))").toBool());
    EXPECT_TRUE(eval("isreal(asin([0 0.5 1 -1]))").toBool());
    EXPECT_NEAR(evalScalar("acos(0.5)"), 1.0471975511965979, 1e-12);
    // NaN stays real (NaN is not 'out of [-1,1]' for the promotion test).
    EXPECT_TRUE(eval("isreal(acos(nan))").toBool());
}
