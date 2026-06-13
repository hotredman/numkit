// toolboxes/comm/tests/comm_fading_test.cpp
//
// Coverage for channel/fading.cpp (parity-spec only before this): rayleighchan
// and ricianchan. These draw on the Engine RNG, so the tests seed with rng(0)
// and assert statistical / structural invariants (unit average power, shape,
// and that a high Rician K-factor collapses the amplitude variance) plus
// seed-reproducibility, rather than exact random values.

#include "dual_engine_fixture.hpp"

#include <cmath>

using namespace m_test;

class CommFadingTest : public DualEngineTest
{};

TEST_P(CommFadingTest, RayleighUnitAveragePower)
{
    eval("rng(0); y = rayleighchan(ones(20000, 1)); pwr = mean(abs(y).^2);");
    EXPECT_EQ(eval("y").numel(), 20000u);
    EXPECT_NEAR(evalScalar("pwr"), 1.0, 0.05);  // normalised to unit average power
}

TEST_P(CommFadingTest, RicianHighKLowVariance)
{
    eval("rng(0); y = ricianchan(ones(20000, 1), 1000); "
         "pwr = mean(abs(y).^2); v = std(abs(y).^2);");
    EXPECT_NEAR(evalScalar("pwr"), 1.0, 0.05);  // unit total power
    EXPECT_LT(evalScalar("v"), 0.2);            // K=1000 → near-deterministic amplitude
}

TEST_P(CommFadingTest, RicianK0MatchesRayleighPower)
{
    eval("rng(0); y = ricianchan(ones(20000, 1), 0); pwr = mean(abs(y).^2);");
    EXPECT_NEAR(evalScalar("pwr"), 1.0, 0.05);  // K=0 → Rayleigh
}

TEST_P(CommFadingTest, RayleighSeedReproducible)
{
    eval("rng(0); a = rayleighchan(ones(100, 1)); "
         "rng(0); b = rayleighchan(ones(100, 1)); e = max(abs(a - b));");
    EXPECT_NEAR(evalScalar("e"), 0.0, 1e-12);  // same seed → identical realisation
}

INSTANTIATE_DUAL(CommFadingTest);
