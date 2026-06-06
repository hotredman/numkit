// libs/builtin/tests/log_complex_test.cpp
//
// Regression guard for bugs/builtin/log-complex-promotion-arrays.md (FIXED):
// log / log10 / log2 promote a whole real ARRAY to complex when any element is
// negative (previously only the scalar case promoted; array elements became
// NaN). std::log's branch matches MATLAB. MATLAB R2025b reference values.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class LogComplexTest : public ::testing::Test
{
public:
    numkit::StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// log([-1 4]) == [0+pi*i  log(4)] (any negative promotes the whole array).
TEST_F(LogComplexTest, LogArray)
{
    eval("y = log([-1 4]);");
    EXPECT_TRUE(eval("~isreal(y)").toBool());
    EXPECT_NEAR(evalScalar("real(y(1))"), 0.0,               1e-12);
    EXPECT_NEAR(evalScalar("imag(y(1))"), 3.141592653589793, 1e-12);
    EXPECT_NEAR(evalScalar("real(y(2))"), 1.3862943611198906, 1e-12);
    EXPECT_NEAR(evalScalar("imag(y(2))"), 0.0,               1e-12);
}

// log10([-100 100]) == [2 + (pi/ln10)i  2].
TEST_F(LogComplexTest, Log10Array)
{
    eval("y = log10([-100 100]);");
    EXPECT_NEAR(evalScalar("real(y(1))"), 2.0,                1e-12);
    EXPECT_NEAR(evalScalar("imag(y(1))"), 1.3643763538418412, 1e-12);
    EXPECT_NEAR(evalScalar("real(y(2))"), 2.0,                1e-12);
    EXPECT_NEAR(evalScalar("imag(y(2))"), 0.0,                1e-12);
}

// log2([-8 8]) == [3 + (pi/ln2)i  3].
TEST_F(LogComplexTest, Log2Array)
{
    eval("y = log2([-8 8]);");
    EXPECT_NEAR(evalScalar("real(y(1))"), 3.0,               1e-12);
    EXPECT_NEAR(evalScalar("imag(y(1))"), 4.532360141827194, 1e-12);
    EXPECT_NEAR(evalScalar("real(y(2))"), 3.0,               1e-12);
}

// In-domain (all-positive) input stays REAL.
TEST_F(LogComplexTest, InDomainStaysReal)
{
    EXPECT_TRUE(eval("isreal(log([1 2 4]))").toBool());
    EXPECT_TRUE(eval("isreal(log10([1 10 100]))").toBool());
    EXPECT_TRUE(eval("isreal(log2([1 2 4]))").toBool());
    EXPECT_NEAR(evalScalar("max(log([1 2 4]))"), 1.3862943611198906, 1e-12);
    // scalar negative still promotes
    EXPECT_NEAR(evalScalar("imag(log(-1))"), 3.141592653589793, 1e-12);
}
