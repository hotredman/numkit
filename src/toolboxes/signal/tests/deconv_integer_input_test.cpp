// toolboxes/signal/tests/deconv_integer_input_test.cpp
//
// bugs/signal/deconv-integer-input.md — deconv accepts integer/logical input
// (MATLAB R2025b promotes to double; quotient AND remainder are always double,
// never the integer class). Offline regression guard, hardcoded expecteds.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class DeconvIntegerInputTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Branch: int8 / int8 quotient -> double (was: throw "Not a double array").
TEST_F(DeconvIntegerInputTest, Int8Quotient)
{
    eval("q = deconv(int8([1 3 5 3]), int8([1 1]));");
    EXPECT_TRUE(eval("isa(q, 'double')").toBool());
    EXPECT_EQ(static_cast<int>(evalScalar("numel(q)")), 3);
    EXPECT_DOUBLE_EQ(evalScalar("q(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("q(2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("q(3)"), 3.0);
}

// Branch: two-output [q, r] -> both double, remainder zero on exact division.
TEST_F(DeconvIntegerInputTest, Int8QuotientRemainder)
{
    eval("[q, r] = deconv(int8([1 3 5 3]), int8([1 1]));");
    EXPECT_TRUE(eval("isa(q, 'double')").toBool());
    EXPECT_TRUE(eval("isa(r, 'double')").toBool());
    EXPECT_DOUBLE_EQ(evalScalar("q(2)"), 2.0);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(r)")), 4);
    EXPECT_DOUBLE_EQ(evalScalar("max(abs(r))"), 0.0);
}

// Branch: int16 input.
TEST_F(DeconvIntegerInputTest, Int16Quotient)
{
    eval("q = deconv(int16([2 7 7 2]), int16([1 2]));");
    EXPECT_TRUE(eval("isa(q, 'double')").toBool());
    EXPECT_DOUBLE_EQ(evalScalar("q(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("q(2)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("q(3)"), 1.0);
}

// Branch: mixed int+double and logical -> double.
TEST_F(DeconvIntegerInputTest, MixedAndLogical)
{
    EXPECT_TRUE(eval("isa(deconv([1 3 5 3], int8([1 1])), 'double')").toBool());
    eval("ql = deconv(logical([1 0 1 0]), [1 1]);");
    EXPECT_TRUE(eval("isa(ql, 'double')").toBool());
    EXPECT_DOUBLE_EQ(evalScalar("ql(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("ql(2)"), -1.0);
    EXPECT_DOUBLE_EQ(evalScalar("ql(3)"), 2.0);
}

// Branch: divisor longer than dividend (na > nb), DOUBLE input — q is the
// scalar 0, remainder is the numerator unchanged (matches MATLAB).
TEST_F(DeconvIntegerInputTest, DivisorLongerDouble)
{
    eval("[q, r] = deconv([1 1], [1 2 3]);");
    EXPECT_DOUBLE_EQ(evalScalar("q(1)"), 0.0);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(r)")), 2);
    EXPECT_DOUBLE_EQ(evalScalar("r(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(2)"), 1.0);
}

// numkit-lenient extension: MATLAB ERRORS on the na>nb branch with INTEGER
// input ("Inputs must be floats" — superiorfloat quirk). numkit promotes
// before the branch, so it stays lenient and returns double. This guards
// that deliberate lenient choice (it is NOT MATLAB parity — see the md).
TEST_F(DeconvIntegerInputTest, DivisorLongerIntegerIsLenient)
{
    eval("[q, r] = deconv(int8([1 1]), int8([1 2 3]));");
    EXPECT_TRUE(eval("isa(q, 'double')").toBool());
    EXPECT_TRUE(eval("isa(r, 'double')").toBool());
    EXPECT_DOUBLE_EQ(evalScalar("q(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(2)"), 1.0);
}

// Regression: plain double/double unchanged.
TEST_F(DeconvIntegerInputTest, DoubleUnchanged)
{
    eval("q = deconv([2 7 7 2], [1 2]);");
    EXPECT_TRUE(eval("isa(q, 'double')").toBool());
    EXPECT_DOUBLE_EQ(evalScalar("q(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("q(3)"), 1.0);
}
