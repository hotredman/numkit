// toolboxes/builtin/tests/cumsum_cumprod_complex_test.cpp
//
// Regression guard for bugs/builtin/cumsum-complex.md (FIXED): cumsum/cumprod
// now accumulate COMPLEX values element-wise (dim + 'reverse' honoured)
// instead of throwing "Not a double array". MATLAB R2025b reference values.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class CumComplexTest : public ::testing::Test
{
public:
    numkit::StandardEngine engine;
    void SetUp() override {}
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// cumsum([1+1i 2+2i 3+3i]) == [1+1i 3+3i 6+6i].
TEST_F(CumComplexTest, CumsumVector)
{
    eval("s = cumsum([1+1i 2+2i 3+3i]);");
    EXPECT_NEAR(evalScalar("real(s(2))"), 3.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(s(2))"), 3.0, 1e-12);
    EXPECT_NEAR(evalScalar("real(s(3))"), 6.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(s(3))"), 6.0, 1e-12);
}

// cumprod([1+1i 1-1i 2i]) == [1+1i 2+0i 0+4i].
TEST_F(CumComplexTest, CumprodVector)
{
    eval("p = cumprod([1+1i 1-1i 2i]);");
    EXPECT_NEAR(evalScalar("real(p(2))"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(p(2))"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("real(p(3))"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(p(3))"), 4.0, 1e-12);
}

// Matrix cumsum defaults to dim 1 (down columns).
TEST_F(CumComplexTest, CumsumMatrixDefaultDim)
{
    eval("cs = cumsum([1+1i 2; 3 4i]);");
    EXPECT_NEAR(evalScalar("real(cs(2,1))"), 4.0, 1e-12);   // (1+1i)+3
    EXPECT_NEAR(evalScalar("imag(cs(2,1))"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("real(cs(2,2))"), 2.0, 1e-12);   // 2+4i
    EXPECT_NEAR(evalScalar("imag(cs(2,2))"), 4.0, 1e-12);
}

// dim + 'reverse' (flip-scan-flip) on a complex row.
TEST_F(CumComplexTest, CumsumReverse)
{
    eval("sr = cumsum([1+1i 2+2i 3+3i], 2, 'reverse');");   // [6+6i 5+5i 3+3i]
    EXPECT_NEAR(evalScalar("real(sr(1))"), 6.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(sr(1))"), 6.0, 1e-12);
    EXPECT_NEAR(evalScalar("real(sr(3))"), 3.0, 1e-12);
}

// Real input must be unaffected.
TEST_F(CumComplexTest, RealUnchanged)
{
    eval("r = cumsum([1 2 3 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("r(4)"), 10.0);
    eval("q = cumprod([1 2 3 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("q(4)"), 24.0);
}
