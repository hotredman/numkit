// toolboxes/comm/tests/comm_pulse_scrambler_test.cpp
//
// Coverage for eq/{pulse,scrambler}.cpp (parity-spec only before this):
// rcosdesign / gaussdesign / rectpulse / intdump and scrambler / descrambler.
// Pulse-shape filters are checked by length + symmetry + a parity-validated
// center tap / normalisation; intdump inverts rectpulse; the scrambler pair is
// verified by the descramble(scramble(x)) == x round-trip (scrambler_smoke).

#include "dual_engine_fixture.hpp"

#include <cmath>

using namespace m_test;

class CommEqTest : public DualEngineTest
{};

TEST_P(CommEqTest, RcosdesignSymmetric)
{
    eval("h = rcosdesign(0.25, 6, 4);");
    EXPECT_EQ(eval("h").numel(), 25u);  // span*sps + 1
    EXPECT_NEAR(evalScalar("max(abs(h - flip(h)))"), 0.0, 1e-12);  // linear-phase / symmetric
    EXPECT_NEAR(evalScalar("h(13)"), 0.5169570352, 1e-6);          // center tap
}

TEST_P(CommEqTest, GaussdesignNormalized)
{
    eval("g = gaussdesign(0.3, 4, 2);");
    EXPECT_EQ(eval("g").numel(), 9u);  // span*sps + 1
    EXPECT_NEAR(evalScalar("sum(g)"), 1.0, 1e-9);                 // unit-area
    EXPECT_NEAR(evalScalar("max(abs(g - flip(g)))"), 0.0, 1e-12);
}

TEST_P(CommEqTest, RectpulseRepeatsSamples)
{
    eval("y = rectpulse([1 2 3]', 4);");
    EXPECT_EQ(eval("y").numel(), 12u);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(5)"), 2.0);  // second symbol block
}

TEST_P(CommEqTest, IntdumpInvertsRectpulse)
{
    eval("z = intdump(rectpulse([1 2 3]', 4), 4);");
    EXPECT_EQ(eval("z").numel(), 3u);
    EXPECT_DOUBLE_EQ(evalScalar("z(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("z(3)"), 3.0);
}

TEST_P(CommEqTest, ScramblerDescramblerRoundTrip)
{
    // poly 1 + z^-2 + z^-5, 5-bit register (scrambler_smoke).
    eval("x = [1 0 1 1 0 0 1 0 1 1 0 1]'; poly = [1 0 1 0 0 1]; init = [0 0 0 0 0]; "
         "y = scrambler(x, poly, init); xr = descrambler(y, poly, init); e = sum(abs(x - xr));");
    EXPECT_DOUBLE_EQ(evalScalar("e"), 0.0);
}

INSTANTIATE_DUAL(CommEqTest);
