// toolboxes/builtin/tests/cumtrapz_complex_test.cpp
//
// Regression guard for the cumtrapz part of
// bugs/builtin/complex-input-unsupported.md (umbrella; cumtrapz now FIXED).
// cumtrapz integrates complex y by cumulative trapezoid over Complex storage;
// the integration variable x stays real. dim / x-spacing / matrix forms all
// honoured. MATLAB R2025b reference values.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class CumtrapzComplexTest : public ::testing::Test
{
public:
    numkit::StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// cumtrapz([1+1i 2+2i 3+3i]) == [0, 1.5+1.5i, 4+4i].
TEST_F(CumtrapzComplexTest, Vector)
{
    eval("c = cumtrapz([1+1i 2+2i 3+3i]);");
    EXPECT_NEAR(evalScalar("real(c(1))"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(c(1))"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("real(c(2))"), 1.5, 1e-12);
    EXPECT_NEAR(evalScalar("imag(c(2))"), 1.5, 1e-12);
    EXPECT_NEAR(evalScalar("real(c(3))"), 4.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(c(3))"), 4.0, 1e-12);
}

// cumtrapz(x, y) with a real x-spacing.
TEST_F(CumtrapzComplexTest, WithXSpacing)
{
    eval("c = cumtrapz([0 1 3], [1+1i 2+2i 4+4i]);");   // MATLAB c(3) = 7.5+7.5i
    EXPECT_NEAR(evalScalar("real(c(3))"), 7.5, 1e-12);
    EXPECT_NEAR(evalScalar("imag(c(3))"), 7.5, 1e-12);
}

// Matrix cumtrapz defaults to dim 1 (down columns): cumtrapz([1+1i 2; 3 4i]).
TEST_F(CumtrapzComplexTest, MatrixColumns)
{
    eval("c = cumtrapz([1+1i 2; 3 4i]);");
    EXPECT_NEAR(evalScalar("real(c(2,1))"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(c(2,1))"), 0.5, 1e-12);
    EXPECT_NEAR(evalScalar("real(c(2,2))"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(c(2,2))"), 2.0, 1e-12);
}

// dim 2 on a row vector integrates along the row.
TEST_F(CumtrapzComplexTest, Dim2)
{
    eval("c = cumtrapz([1+1i 2+2i 3+3i], 2);");   // MATLAB c(3) = 4+4i
    EXPECT_NEAR(evalScalar("real(c(3))"), 4.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(c(3))"), 4.0, 1e-12);
}

// Real input must be unaffected.
TEST_F(CumtrapzComplexTest, RealUnchanged)
{
    eval("r = cumtrapz([1 2 3 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("r(4)"), 7.5);
    EXPECT_TRUE(eval("isreal(r)").toBool());
}
