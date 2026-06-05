// libs/stats/tests/detrend_complex_test.cpp
//
// Regression guard for the detrend part of
// bugs/builtin/complex-input-unsupported.md (umbrella; detrend now FIXED).
// detrend of a complex array removes the trend from the real and imaginary
// parts separately, then recombines. MATLAB R2025b reference values.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class DetrendComplexTest : public ::testing::Test
{
public:
    numkit::Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Linear (default): detrend([1+2i 5+1i 3-2i 8+0i]).
TEST_F(DetrendComplexTest, Linear)
{
    eval("d = detrend([1+2i 5+1i 3-2i 8+0i]);");
    EXPECT_NEAR(evalScalar("real(d(1))"), -0.4, 1e-12);
    EXPECT_NEAR(evalScalar("imag(d(1))"),  0.4, 1e-12);
    EXPECT_NEAR(evalScalar("real(d(2))"),  1.7, 1e-12);
    EXPECT_NEAR(evalScalar("imag(d(2))"),  0.3, 1e-12);
    EXPECT_NEAR(evalScalar("real(d(4))"),  0.9, 1e-12);
    EXPECT_NEAR(evalScalar("imag(d(4))"),  1.1, 1e-12);
}

// 'constant' subtracts the complex mean: mean = 4.25+0.25i.
TEST_F(DetrendComplexTest, Constant)
{
    eval("d = detrend([1+2i 5+1i 3-2i 8+0i], 'constant');");
    EXPECT_NEAR(evalScalar("real(d(1))"), -3.25, 1e-12);
    EXPECT_NEAR(evalScalar("imag(d(1))"),  1.75, 1e-12);
}

// A perfect complex linear ramp detrends to ~0.
TEST_F(DetrendComplexTest, RampToZero)
{
    eval("d = detrend([1+2i 2+4i 3+6i]);");
    EXPECT_LT(evalScalar("abs(d(1))"), 1e-10);
    EXPECT_LT(evalScalar("abs(d(2))"), 1e-10);
    EXPECT_LT(evalScalar("abs(d(3))"), 1e-10);
}

// Matrix: detrend each column.
TEST_F(DetrendComplexTest, MatrixColumns)
{
    eval("d = detrend([1+1i 10; 2 8+2i; 5+1i 6]);");
    EXPECT_NEAR(evalScalar("real(d(1,1))"), 1.0 / 3.0, 1e-10);
    EXPECT_NEAR(evalScalar("imag(d(1,1))"), 1.0 / 3.0, 1e-10);
    EXPECT_NEAR(evalScalar("real(d(2,2))"), 0.0,       1e-10);
    EXPECT_NEAR(evalScalar("imag(d(2,2))"), 4.0 / 3.0, 1e-10);
}

// Real input must be unaffected.
TEST_F(DetrendComplexTest, RealUnchanged)
{
    eval("d = detrend(2 * (1:10)' + 5);");   // linear -> ~0
    EXPECT_LT(evalScalar("max(abs(d))"), 1e-10);
    EXPECT_TRUE(eval("isreal(d)").toBool());
}
