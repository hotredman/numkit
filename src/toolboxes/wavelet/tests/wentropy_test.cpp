// toolboxes/wavelet/tests/wentropy_test.cpp
//
// wentropy(X, T[, P]) — closed-form coefficient entropy. bugs/wavelet/
// wentropy.md. Reference values from MATLAB R2025b.

#include <numkit/bundle/standard_engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class WentropyTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Shannon entropy -Σ s²·log(s²); zeros contribute 0.
TEST_F(WentropyTest, Shannon)
{
    EXPECT_NEAR(evalScalar("wentropy([1 2 3 4], 'shannon')"), -69.6816181963, 1e-7);
    EXPECT_NEAR(evalScalar("wentropy([0.5 -0.3 0.8 0 -0.1 0.2], 'shannon')"),
                1.023719175595, 1e-9);
}

// Log-energy Σ log(s²) over nonzero coefficients.
TEST_F(WentropyTest, LogEnergy)
{
    EXPECT_NEAR(evalScalar("wentropy([0.5 -0.3 0.8 0 -0.1 0.2], 'log energy')"),
                -12.064573083256, 1e-9);
}

// Threshold: count of |coefficients| > P.
TEST_F(WentropyTest, Threshold)
{
    EXPECT_DOUBLE_EQ(evalScalar("wentropy([0.5 -0.3 0.8 0 -0.1 0.2], 'threshold', 0.2)"), 3.0);
}

// SURE entropy: n - 2·#{|x|<=P} + Σ min(x², P²).
TEST_F(WentropyTest, Sure)
{
    EXPECT_NEAR(evalScalar("wentropy([0.5 -0.3 0.8 0 -0.1 0.2], 'sure', 0.2)"),
                0.17, 1e-10);
}

// Norm: Σ |x|^P (P >= 1).
TEST_F(WentropyTest, Norm)
{
    EXPECT_NEAR(evalScalar("wentropy([0.5 -0.3 0.8 0 -0.1 0.2], 'norm', 1.5)"),
                1.354477406346, 1e-9);
}

// Unknown type / bad norm exponent throw.
TEST_F(WentropyTest, ErrorsThrow)
{
    EXPECT_THROW(eval("wentropy([1 2 3], 'bogus');"), std::exception);
    EXPECT_THROW(eval("wentropy([1 2 3], 'norm', 0.5);"), std::exception);
}
