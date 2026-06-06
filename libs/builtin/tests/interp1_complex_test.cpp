// libs/builtin/tests/interp1_complex_test.cpp
//
// Regression guard for the interp1 part of
// bugs/builtin/complex-input-unsupported.md (umbrella; interp1 now FIXED).
// interp1 interpolates the real and imaginary parts of a complex y separately
// (every method), then recombines. MATLAB R2025b reference values.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class Interp1ComplexTest : public ::testing::Test
{
public:
    numkit::StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Linear (default): interp1([1 2 3],[1+1i 2+2i 3+3i],2.5) == 2.5+2.5i.
TEST_F(Interp1ComplexTest, Linear)
{
    eval("y = interp1([1 2 3],[1+1i 2+2i 3+3i],2.5);");
    EXPECT_NEAR(evalScalar("real(y)"), 2.5, 1e-12);
    EXPECT_NEAR(evalScalar("imag(y)"), 2.5, 1e-12);
}

// Vector of queries.
TEST_F(Interp1ComplexTest, VectorQuery)
{
    eval("y = interp1([1 2 3],[1+1i 4 3-2i],[1.5 2.5]);");
    EXPECT_NEAR(evalScalar("real(y(1))"), 2.5, 1e-12);
    EXPECT_NEAR(evalScalar("imag(y(1))"), 0.5, 1e-12);
    EXPECT_NEAR(evalScalar("real(y(2))"), 3.5, 1e-12);
    EXPECT_NEAR(evalScalar("imag(y(2))"), -1.0, 1e-12);
}

// 'nearest' picks a whole sample (here y(2)=5).
TEST_F(Interp1ComplexTest, Nearest)
{
    eval("y = interp1([1 2 3],[1+1i 5 3-2i],2.4,'nearest');");
    EXPECT_NEAR(evalScalar("real(y)"), 5.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(y)"), 0.0, 1e-12);
}

// 'spline' interpolates each part with the cubic spline.
TEST_F(Interp1ComplexTest, Spline)
{
    eval("y = interp1([1 2 3 4],[1+1i 0 -1+2i 3],2.5,'spline');");
    EXPECT_NEAR(evalScalar("real(y)"), -0.8125, 1e-10);
    EXPECT_NEAR(evalScalar("imag(y)"),  1.0625, 1e-10);
}

// Out-of-range default extrapolation → NaN + NaN*i.
TEST_F(Interp1ComplexTest, ExtrapNaN)
{
    eval("y = interp1([1 2 3],[1+1i 2+2i 3+3i],5);");
    EXPECT_TRUE(std::isnan(evalScalar("real(y)")));
    EXPECT_TRUE(std::isnan(evalScalar("imag(y)")));
}

// Matrix y: interpolate down each column.
TEST_F(Interp1ComplexTest, MatrixColumns)
{
    eval("y = interp1([1 2 3],[1+1i 2; 3 4-1i; 5+5i 6],1.5);");  // MATLAB: [2+0.5i 3-0.5i]
    EXPECT_NEAR(evalScalar("real(y(1))"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(y(1))"), 0.5, 1e-12);
    EXPECT_NEAR(evalScalar("real(y(2))"), 3.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(y(2))"), -0.5, 1e-12);
}

// Real input must be unaffected.
TEST_F(Interp1ComplexTest, RealUnchanged)
{
    EXPECT_DOUBLE_EQ(evalScalar("interp1([1 2 3],[10 20 30],2.5)"), 25.0);
    EXPECT_TRUE(eval("isreal(interp1([1 2 3],[10 20 30],2.5))").toBool());
}
