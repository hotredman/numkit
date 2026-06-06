// libs/builtin/tests/trapz_complex_test.cpp
//
// Regression guard for the trapz part of bugs/builtin/complex-input-unsupported.md
// (umbrella; trapz now FIXED). trapz integrates complex y by trapezoidal sum
// over Complex storage; the integration variable x stays real. dim + x-spacing
// + matrix forms all honoured. MATLAB R2025b reference values.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class TrapzComplexTest : public ::testing::Test
{
public:
    numkit::StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// trapz([1+1i 2+2i 3+3i]) == 4+4i (unit spacing).
TEST_F(TrapzComplexTest, Vector)
{
    eval("t = trapz([1+1i 2+2i 3+3i]);");
    EXPECT_NEAR(evalScalar("real(t)"), 4.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(t)"), 4.0, 1e-12);
}

// trapz(x, y) with a real x-spacing: trapz([0 1 2],[1+1i 2+2i 5+5i]) == 5+5i.
TEST_F(TrapzComplexTest, WithXSpacing)
{
    eval("t = trapz([0 1 2], [1+1i 2+2i 5+5i]);");
    EXPECT_NEAR(evalScalar("real(t)"), 5.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(t)"), 5.0, 1e-12);
}

// Matrix integrates each column: trapz([1+1i 2; 3 4i]) == [2+0.5i 1+2i].
TEST_F(TrapzComplexTest, MatrixColumns)
{
    eval("t = trapz([1+1i 2; 3 4i]);");
    EXPECT_NEAR(evalScalar("real(t(1))"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(t(1))"), 0.5, 1e-12);
    EXPECT_NEAR(evalScalar("real(t(2))"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(t(2))"), 2.0, 1e-12);
}

// Explicit dim 2 on a row vector integrates along the row.
TEST_F(TrapzComplexTest, Dim2)
{
    eval("t = trapz([1+1i 2+2i 3+3i], 2);");
    EXPECT_NEAR(evalScalar("real(t)"), 4.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(t)"), 4.0, 1e-12);
}

// Real input must be unaffected.
TEST_F(TrapzComplexTest, RealUnchanged)
{
    EXPECT_DOUBLE_EQ(evalScalar("trapz([1 2 3 4])"), 7.5);
    EXPECT_TRUE(eval("isreal(trapz([1 2 3 4]))").toBool());
}
