// toolboxes/signal/tests/filter_complex_test.cpp
//
// Regression guard for the filter part of
// bugs/builtin/complex-input-unsupported.md (umbrella; filter now FIXED — this
// closes the umbrella). filter is BILINEAR (the recursive a-part mixes terms),
// so the Direct Form II transposed recurrence runs over Complex — NOT a
// real/imag split. Covers FIR/IIR, complex b/a taps, zi, [y,zf], matrix.
// MATLAB R2025b reference values.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class FilterComplexTest : public ::testing::Test
{
public:
    numkit::StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// FIR filter([1 1],1,[1i 1i 1i]) == [1i 2i 2i].
TEST_F(FilterComplexTest, FIR)
{
    eval("y = filter([1 1], 1, [1i 1i 1i]);");
    EXPECT_NEAR(evalScalar("imag(y(1))"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(y(2))"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(y(3))"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("real(y(2))"), 0.0, 1e-12);
}

// IIR filter(1,[1 -0.5],[1+1i 2 4-1i]): y(2)=2.5+0.5i, y(3)=5.25-0.75i.
TEST_F(FilterComplexTest, IIR)
{
    eval("y = filter(1, [1 -0.5], [1+1i 2 4-1i]);");
    EXPECT_NEAR(evalScalar("real(y(2))"),  2.5,  1e-12);
    EXPECT_NEAR(evalScalar("imag(y(2))"),  0.5,  1e-12);
    EXPECT_NEAR(evalScalar("real(y(3))"),  5.25, 1e-12);
    EXPECT_NEAR(evalScalar("imag(y(3))"), -0.75, 1e-12);
}

// Complex b AND a taps: filter([1+1i 0.5],[1 0.2i],[1 2 3]): y(2)=2.7+1.8i.
TEST_F(FilterComplexTest, ComplexTaps)
{
    eval("y = filter([1+1i 0.5], [1 0.2i], [1 2 3]);");
    EXPECT_NEAR(evalScalar("real(y(2))"), 2.7, 1e-12);
    EXPECT_NEAR(evalScalar("imag(y(2))"), 1.8, 1e-12);
}

// [y, zf]: the complex final state is returned.
TEST_F(FilterComplexTest, FinalStateZf)
{
    eval("[y, zf] = filter([1 1], 1, [1i 2i 3i]);");
    EXPECT_NEAR(evalScalar("real(zf(1))"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(zf(1))"), 3.0, 1e-12);
}

// Initial conditions zi seed the state: y(1) = zi + b0*x(1) = 5 + 1i.
TEST_F(FilterComplexTest, InitialConditionsZi)
{
    eval("y = filter([1 1], 1, [1i 2i 3i], 5+0i);");
    EXPECT_NEAR(evalScalar("real(y(1))"), 5.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(y(1))"), 1.0, 1e-12);
}

// Matrix: filter down each column.
TEST_F(FilterComplexTest, MatrixColumns)
{
    eval("y = filter([1 1], 1, [1i 2; 3i 4-1i]);");   // MATLAB: y(2,1)=4i, y(2,2)=6-1i
    EXPECT_NEAR(evalScalar("imag(y(2,1))"), 4.0, 1e-12);
    EXPECT_NEAR(evalScalar("real(y(2,2))"), 6.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(y(2,2))"), -1.0, 1e-12);
}

// Real input must be unaffected.
TEST_F(FilterComplexTest, RealUnchanged)
{
    eval("y = filter([1 1], 1, [1 2 3]);");   // [1 3 5]
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 5.0);
    EXPECT_TRUE(eval("isreal(y)").toBool());
}
