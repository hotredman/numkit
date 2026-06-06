// libs/signal/tests/fwht_test.cpp
//
// Regression guard for signal/fwht + ifwht. Fingerprints from MATLAB R2025b.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class FwhtTest : public ::testing::Test
{
public:
    numkit::StandardEngine engine;
    void SetUp() override
    {
        engine.eval("import compat.*;");
        engine.eval("x = [1 2 3 4 5 6 7 8];");
    }
    double evalScalar(const std::string &c) { return engine.eval(c).toScalar(); }
};

// Default ordering is 'sequency': y = [4.5 -2 0 -1 0 0 0 -0.5].
TEST_F(FwhtTest, DefaultSequencyOrder)
{
    engine.eval("y = fwht(x);");
    EXPECT_NEAR(evalScalar("y(1)"),  4.5,  1e-14);
    EXPECT_NEAR(evalScalar("y(2)"), -2.0,  1e-14);
    EXPECT_NEAR(evalScalar("y(3)"),  0.0,  1e-14);
    EXPECT_NEAR(evalScalar("y(4)"), -1.0,  1e-14);
    EXPECT_NEAR(evalScalar("y(8)"), -0.5,  1e-14);
}

// Hadamard (natural) order: y = [4.5 -0.5 -1 0 -2 0 0 0].
TEST_F(FwhtTest, HadamardOrder)
{
    engine.eval("y = fwht(x, 8, 'hadamard');");
    EXPECT_NEAR(evalScalar("y(1)"),  4.5,  1e-14);
    EXPECT_NEAR(evalScalar("y(2)"), -0.5,  1e-14);
    EXPECT_NEAR(evalScalar("y(3)"), -1.0,  1e-14);
    EXPECT_NEAR(evalScalar("y(5)"), -2.0,  1e-14);
}

// Dyadic (Paley/bit-reversed) order: y = [4.5 -2 -1 0 -0.5 0 0 0].
TEST_F(FwhtTest, DyadicOrder)
{
    engine.eval("y = fwht(x, 8, 'dyadic');");
    EXPECT_NEAR(evalScalar("y(1)"),  4.5,  1e-14);
    EXPECT_NEAR(evalScalar("y(2)"), -2.0,  1e-14);
    EXPECT_NEAR(evalScalar("y(3)"), -1.0,  1e-14);
    EXPECT_NEAR(evalScalar("y(5)"), -0.5,  1e-14);
}

// y(1) is the mean of the input (1/N normalisation).
TEST_F(FwhtTest, FirstCoefficientIsMean)
{
    engine.eval("y = fwht(1:16);");
    EXPECT_NEAR(evalScalar("y(1)"), 8.5, 1e-14);  // mean(1:16)
}

// Zero-pad to length 16: y(1) = sum/16 = 36/16 = 2.25.
TEST_F(FwhtTest, ZeroPadDoublesLength)
{
    engine.eval("y = fwht(x, 16);");
    EXPECT_DOUBLE_EQ(evalScalar("length(y)"), 16.0);
    EXPECT_NEAR(evalScalar("y(1)"),  2.25, 1e-14);
    EXPECT_NEAR(evalScalar("y(8)"), -0.5,  1e-14);
    EXPECT_NEAR(evalScalar("y(16)"), -0.25, 1e-14);
}

// Truncate to length 4: y(1) = mean(1:4) = 2.5.
TEST_F(FwhtTest, TruncateToShorter)
{
    engine.eval("y = fwht(x, 4);");
    EXPECT_DOUBLE_EQ(evalScalar("length(y)"), 4.0);
    EXPECT_NEAR(evalScalar("y(1)"),  2.5, 1e-14);
    EXPECT_NEAR(evalScalar("y(4)"), -0.5, 1e-14);
}

// Round-trip ifwht(fwht(x)) = x — exact in integer arithmetic.
TEST_F(FwhtTest, RoundTripSequencyExact)
{
    engine.eval("y = fwht(x); xr = ifwht(y); err = max(abs(xr - x));");
    EXPECT_DOUBLE_EQ(evalScalar("err"), 0.0);
}

TEST_F(FwhtTest, RoundTripHadamardExact)
{
    engine.eval("y = fwht(x, 8, 'hadamard'); "
                "xr = ifwht(y, 8, 'hadamard'); "
                "err = max(abs(xr - x));");
    EXPECT_DOUBLE_EQ(evalScalar("err"), 0.0);
}

TEST_F(FwhtTest, RoundTripDyadicExact)
{
    engine.eval("y = fwht(x, 8, 'dyadic'); "
                "xr = ifwht(y, 8, 'dyadic'); "
                "err = max(abs(xr - x));");
    EXPECT_DOUBLE_EQ(evalScalar("err"), 0.0);
}

// Impulse delta into Hadamard basis: fwht([1 0 0 0]) = [1/4, 1/4, 1/4, 1/4].
TEST_F(FwhtTest, HadamardImpulseRow0)
{
    engine.eval("y = fwht([1 0 0 0], 4, 'hadamard');");
    for (int k = 1; k <= 4; ++k)
        EXPECT_NEAR(evalScalar("y(" + std::to_string(k) + ")"), 0.25, 1e-14);
}

// fwht([0 1 0 0], 4, 'hadamard') = [0.25 -0.25 0.25 -0.25] (col 2 of H4).
TEST_F(FwhtTest, HadamardImpulseRow1)
{
    engine.eval("y = fwht([0 1 0 0], 4, 'hadamard');");
    EXPECT_NEAR(evalScalar("y(1)"),  0.25, 1e-14);
    EXPECT_NEAR(evalScalar("y(2)"), -0.25, 1e-14);
    EXPECT_NEAR(evalScalar("y(3)"),  0.25, 1e-14);
    EXPECT_NEAR(evalScalar("y(4)"), -0.25, 1e-14);
}

// Non-power-of-2 length: auto-promotes to next pow-2.
TEST_F(FwhtTest, NonPowerOfTwoAutoPad)
{
    engine.eval("y = fwht([1 2 3 4 5 6 7]);");
    EXPECT_DOUBLE_EQ(evalScalar("length(y)"), 8.0);
    EXPECT_NEAR(evalScalar("y(1)"), 28.0 / 8.0, 1e-14);  // sum/N
}

// Explicit non-pow-2 n throws.
TEST_F(FwhtTest, ExplicitNonPow2Throws)
{
    EXPECT_THROW(engine.eval("fwht(x, 6);"), std::exception);
}

// Unknown ordering throws.
TEST_F(FwhtTest, UnknownOrderingThrows)
{
    EXPECT_THROW(engine.eval("fwht(x, 8, 'bogus');"), std::exception);
}
