// toolboxes/comm/tests/comm_modulation_test.cpp
//
// Coverage for modulation/{psk,fsk_ofdm}.cpp (parity-spec only before this):
// pskmod/pskdemod, dpskmod/dpskdemod, fskmod/fskdemod, ofdmmod/ofdmdemod.
// Modulators are verified by the demod(mod(x)) == x round-trip (exact and
// independent of constellation/ordering conventions), matching psk_smoke /
// fsk_ofdm_smoke. Outputs are compared via sum(abs(a(:)-b(:))) so row/column
// orientation does not matter.

#include "dual_engine_fixture.hpp"

#include <cmath>

using namespace m_test;

class CommModulationTest : public DualEngineTest
{};

TEST_P(CommModulationTest, PskRoundTripUnitMagnitude)
{
    eval("data = [0 1 2 3 0 2 1 3]; s = pskmod(data, 4); out = pskdemod(s, 4); "
         "e = sum(abs(out(:) - data(:)));");
    EXPECT_DOUBLE_EQ(evalScalar("e"), 0.0);
    EXPECT_NEAR(evalScalar("max(abs(abs(s) - 1))"), 0.0, 1e-12);  // QPSK points on unit circle
}

TEST_P(CommModulationTest, PskBinarySymbolOrder)
{
    eval("data = [0 1 2 3 0 2 1 3]; s = pskmod(data, 4, 0, 'bin'); "
         "out = pskdemod(s, 4, 0, 'bin'); e = sum(abs(out(:) - data(:)));");
    EXPECT_DOUBLE_EQ(evalScalar("e"), 0.0);
}

TEST_P(CommModulationTest, DpskRoundTrip)
{
    eval("data = [0 1 2 3 0 2 1 3]; out = dpskdemod(dpskmod(data, 4), 4); "
         "e = sum(abs(out(:) - data(:)));");
    EXPECT_DOUBLE_EQ(evalScalar("e"), 0.0);
}

TEST_P(CommModulationTest, FskmodOutputLength)
{
    eval("y = fskmod([0 1 2 3 0 2 1 3], 4, 100, 10, 2000);");
    EXPECT_EQ(eval("y").numel(), 80u);  // 8 symbols x 10 samples/symbol
}

// fskmod -> fskdemod round-trip. Also guards bugs/comm/fsk_tw_divergence.md: a
// TreeWalker x(:) row->column bug used to break this on TW (fskdemod returns a
// column, the input is a row, so out(:) - data(:) needs both column-ified).
TEST_P(CommModulationTest, FskRoundTrip)
{
    eval("data = [0 1 2 3 0 2 1 3]; out = fskdemod(fskmod(data, 4, 100, 10, 2000), "
         "4, 100, 10, 2000); e = sum(abs(out(:) - data(:)));");
    EXPECT_DOUBLE_EQ(evalScalar("e"), 0.0);
}

TEST_P(CommModulationTest, OfdmRoundTrip)
{
    eval("in = reshape(1:16, 8, 2); y = ofdmmod(in, 8, 2); "
         "out = ofdmdemod(y, 8, 2); e = max(max(abs(out - in)));");
    EXPECT_EQ(eval("y").numel(), 20u);  // (nfft + cplen) x Nsym = 10 x 2
    EXPECT_NEAR(evalScalar("e"), 0.0, 1e-9);
}

INSTANTIATE_DUAL(CommModulationTest);
