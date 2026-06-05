// libs/stats/tests/movmean_complex_test.cpp
//
// Regression guard for the movmean part of
// bugs/builtin/complex-input-unsupported.md (umbrella; movmean now FIXED).
// movmean of a complex array moving-means the real and imaginary parts
// separately, then recombines (window / asymmetric / dim all carry through).
// MATLAB R2025b reference values.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class MovmeanComplexTest : public ::testing::Test
{
public:
    numkit::Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Even window k=2 (leans backward): [1+1i 1.5+1.5i 2.5+2.5i 3.5+3.5i].
TEST_F(MovmeanComplexTest, EvenWindow)
{
    eval("m = movmean([1+1i 2+2i 3+3i 4+4i], 2);");
    EXPECT_NEAR(evalScalar("real(m(1))"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(m(1))"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("real(m(2))"), 1.5, 1e-12);
    EXPECT_NEAR(evalScalar("imag(m(2))"), 1.5, 1e-12);
    EXPECT_NEAR(evalScalar("real(m(4))"), 3.5, 1e-12);
    EXPECT_NEAR(evalScalar("imag(m(4))"), 3.5, 1e-12);
}

// Centered odd window k=3.
TEST_F(MovmeanComplexTest, OddWindow)
{
    eval("m = movmean([1+1i 5 3-2i 8 2+4i], 3);");   // m(3) = (5+(3-2i)+8)/3
    EXPECT_NEAR(evalScalar("real(m(3))"), 16.0 / 3.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(m(3))"), -2.0 / 3.0, 1e-12);
}

// Asymmetric window [kb kf] = [1 0].
TEST_F(MovmeanComplexTest, Asymmetric)
{
    eval("m = movmean([1+1i 5 3-2i 8], [1 0]);");   // m(3) = (5+(3-2i))/2 = 4-1i
    EXPECT_NEAR(evalScalar("real(m(3))"),  4.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(m(3))"), -1.0, 1e-12);
}

// Matrix: per-column.
TEST_F(MovmeanComplexTest, MatrixColumns)
{
    eval("m = movmean([1+1i 2; 3 4i; 5+5i 6], 2);");  // m(2,:) = [2+0.5i 1+2i]
    EXPECT_NEAR(evalScalar("real(m(2,1))"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(m(2,1))"), 0.5, 1e-12);
    EXPECT_NEAR(evalScalar("real(m(2,2))"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(m(2,2))"), 2.0, 1e-12);
}

// Real input must be unaffected.
TEST_F(MovmeanComplexTest, RealUnchanged)
{
    eval("m = movmean([1 2 3 4], 2);");
    EXPECT_DOUBLE_EQ(evalScalar("m(2)"), 1.5);
    EXPECT_TRUE(eval("isreal(m)").toBool());
}
