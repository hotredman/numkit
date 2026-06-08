// toolboxes/stats/tests/median_complex_test.cpp
//
// Regression guard for the median part of
// bugs/builtin/complex-input-unsupported.md (umbrella; median now FIXED).
// median of a complex array orders by abs (ties by angle), same comparator
// sort/max use; even n → mean of the two middle. MATLAB R2025b reference.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class MedianComplexTest : public ::testing::Test
{
public:
    numkit::StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Odd count: middle element by abs ordering.
TEST_F(MedianComplexTest, OddVector)
{
    eval("m = median([1+1i 2+2i 3+3i]);");   // MATLAB: 2+2i
    EXPECT_NEAR(evalScalar("real(m)"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(m)"), 2.0, 1e-12);
}

// Even count: mean of the two middle (by abs).
TEST_F(MedianComplexTest, EvenVector)
{
    eval("m = median([1+1i 2+2i 3+3i 10+10i]);");   // MATLAB: 2.5+2.5i
    EXPECT_NEAR(evalScalar("real(m)"), 2.5, 1e-12);
    EXPECT_NEAR(evalScalar("imag(m)"), 2.5, 1e-12);
}

// Matrix: median down each column.
TEST_F(MedianComplexTest, MatrixColumns)
{
    eval("m = median([1+1i 2; 3 4i]);");   // MATLAB: [2+0.5i 1+2i]
    EXPECT_NEAR(evalScalar("real(m(1))"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(m(1))"), 0.5, 1e-12);
    EXPECT_NEAR(evalScalar("real(m(2))"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(m(2))"), 2.0, 1e-12);
}

// dim 2 along a row; ordering is by magnitude.
TEST_F(MedianComplexTest, Dim2)
{
    eval("m = median([1+1i 5 2-3i], 2);");   // abs [1.41 5 3.61] → middle 2-3i
    EXPECT_NEAR(evalScalar("real(m)"),  2.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(m)"), -3.0, 1e-12);
}

// Equal magnitudes: ties broken by angle. |1|=|1i|=|-1|=1; angle order
// 1 (0) < 1i (pi/2) < -1 (pi) → middle 1i.
TEST_F(MedianComplexTest, AbsTieByAngle)
{
    eval("m = median([1 1i -1]);");   // MATLAB: 0+1i
    EXPECT_NEAR(evalScalar("real(m)"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(m)"), 1.0, 1e-12);
}

// 'all' flattens then medians (even → mean of two middle).
TEST_F(MedianComplexTest, All)
{
    eval("m = median([1+1i 2; 3 4i], 'all');");   // MATLAB: 2.5+0i
    EXPECT_NEAR(evalScalar("real(m)"), 2.5, 1e-12);
    EXPECT_NEAR(evalScalar("imag(m)"), 0.0, 1e-12);
}

// Real input must be unaffected.
TEST_F(MedianComplexTest, RealUnchanged)
{
    EXPECT_DOUBLE_EQ(evalScalar("median([3 1 2])"), 2.0);
    EXPECT_TRUE(eval("isreal(median([3 1 2]))").toBool());
}
