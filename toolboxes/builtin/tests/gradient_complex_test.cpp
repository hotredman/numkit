// toolboxes/builtin/tests/gradient_complex_test.cpp
//
// Regression guard for the gradient part of
// bugs/builtin/complex-input-unsupported.md (umbrella; gradient now FIXED).
// gradient of a complex array gradients the real and imaginary parts
// separately, then recombines (vector + matrix single/2-output + N-D as of
// 2026-06-05, bugs/builtin/gradient-3d.md). MATLAB R2025b reference values.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class GradientComplexTest : public ::testing::Test
{
public:
    numkit::StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// gradient([1+1i 3+3i 5+5i]) == [2+2i 2+2i 2+2i].
TEST_F(GradientComplexTest, Vector)
{
    eval("g = gradient([1+1i 3+3i 5+5i]);");
    EXPECT_NEAR(evalScalar("real(g(1))"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(g(1))"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("real(g(2))"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(g(2))"), 2.0, 1e-12);
}

// Vector with mixed parts: gradient([1+2i 3 5-1i]) = [2-2i 2-1.5i 2-1i].
TEST_F(GradientComplexTest, VectorMixed)
{
    eval("g = gradient([1+2i 3 5-1i]);");
    EXPECT_NEAR(evalScalar("real(g(2))"),  2.0,  1e-12);
    EXPECT_NEAR(evalScalar("imag(g(2))"), -1.5,  1e-12);
    EXPECT_NEAR(evalScalar("imag(g(1))"), -2.0,  1e-12);
    EXPECT_NEAR(evalScalar("imag(g(3))"), -1.0,  1e-12);
}

// Single-output matrix gradient is the dim-2 (x) gradient.
TEST_F(GradientComplexTest, MatrixSingle)
{
    eval("gx = gradient([1+1i 2 4; 3+1i 4i 6]);");
    EXPECT_NEAR(evalScalar("real(gx(1,1))"),  1.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(gx(1,1))"), -1.0, 1e-12);
    EXPECT_NEAR(evalScalar("real(gx(1,2))"),  1.5, 1e-12);
    EXPECT_NEAR(evalScalar("imag(gx(1,2))"), -0.5, 1e-12);
}

// 2-output: [FX,FY] = gradient(M). FY is the dim-1 (y) gradient.
TEST_F(GradientComplexTest, MatrixTwoOutput)
{
    eval("[fx, fy] = gradient([1+1i 2 4; 3+1i 4i 6]);");
    EXPECT_NEAR(evalScalar("real(fy(1,1))"), 2.0, 1e-12);   // (3+1i)-(1+1i)
    EXPECT_NEAR(evalScalar("imag(fy(1,1))"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("real(fx(1,2))"), 1.5, 1e-12);
    EXPECT_NEAR(evalScalar("imag(fx(1,2))"), -0.5, 1e-12);
}

// Real input must be unaffected.
TEST_F(GradientComplexTest, RealUnchanged)
{
    eval("g = gradient([1 4 9]);");
    EXPECT_DOUBLE_EQ(evalScalar("g(2)"), 4.0);
    EXPECT_TRUE(eval("isreal(g)").toBool());
}

// N-D complex now works (was: threw) — real + imaginary parts gradiented
// separately and recombined. bugs/builtin/gradient-3d.md FIXED 2026-06-05.
TEST_F(GradientComplexTest, NDComplexOk)
{
    eval("Z = reshape(1:8,2,2,2) + 1i*reshape(8:-1:1,2,2,2); [zx,zy,zz] = gradient(Z);");
    EXPECT_TRUE(eval("~isreal(zx)").toBool());
    EXPECT_NEAR(evalScalar("real(zx(1,1,1))"), 2.0,  1e-12);   // d/dx of real part
    EXPECT_NEAR(evalScalar("imag(zx(1,1,1))"), -2.0, 1e-12);   // d/dx of imag part
}
