// toolboxes/signal/tests/firls_test.cpp
//
// Regression guard for signal::firls (Type-I least-squares FIR design).
// Hardcoded expected values captured from MATLAB R2025b parity probe.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class FirlsTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Lowpass: passband [0, 0.4], stopband [0.5, 1.0].
TEST_F(FirlsTest, LowpassLength21)
{
    eval("b = firls(20, [0 0.4 0.5 1], [1 1 0 0]);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(b)")), 21);
    // Symmetric Type-I.
    EXPECT_NEAR(evalScalar("max(abs(b - fliplr(b)))"), 0.0, 1e-12);
    // Bit-identical hardcoded values (MATLAB R2025b reference).
    EXPECT_NEAR(evalScalar("b(1)"),  0.0165090, 1e-6);
    EXPECT_NEAR(evalScalar("b(6)"),  0.0391469, 1e-6);
    EXPECT_NEAR(evalScalar("b(11)"), 0.4503900, 1e-6);
    EXPECT_NEAR(evalScalar("b(16)"), 0.0391469, 1e-6);
    EXPECT_NEAR(evalScalar("b(21)"), 0.0165090, 1e-6);
}

// Bandpass: stop [0,0.2], pass [0.3,0.6], stop [0.7,1].
TEST_F(FirlsTest, BandpassLength41)
{
    eval("bb = firls(40, [0 0.2 0.3 0.6 0.7 1], [0 0 1 1 0 0]);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(bb)")), 41);
    EXPECT_NEAR(evalScalar("max(abs(bb - fliplr(bb)))"), 0.0, 1e-12);
    // Center coefficient (hardcoded from MATLAB R2025b parity probe).
    EXPECT_NEAR(evalScalar("bb(21)"), 0.397298, 1e-6);
}

// Differentiator-like linear ramp in the passband (linear interp inside band).
TEST_F(FirlsTest, LinearRampInBand)
{
    eval("b3 = firls(20, [0 0.5 0.6 1], [0 1 0 0]);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(b3)")), 21);
    EXPECT_NEAR(evalScalar("max(abs(b3 - fliplr(b3)))"), 0.0, 1e-12);
}

// Even-N rejection (Type-III/IV not implemented).
TEST_F(FirlsTest, OddOrderRejected)
{
    EXPECT_THROW(eval("firls(21, [0 0.4 0.5 1], [1 1 0 0]);"), std::exception);
}

// Length mismatch rejection.
TEST_F(FirlsTest, FAlengthMismatchRejected)
{
    EXPECT_THROW(eval("firls(20, [0 0.4 0.5 1], [1 1 0]);"), std::exception);
}
