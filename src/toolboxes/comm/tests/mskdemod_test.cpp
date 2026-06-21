// mskdemod_test.cpp — MSK demodulator (differential variant).
//
// Coherent inverse of mskmod: bit_k = sign of the symbol's accumulated
// phase increment. Robust to a constant phase rotation and to noise.
// Verified vs MATLAB R2025b. Fixes bugs/comm/analog-demodulators.md (the
// last of the five demodulators).

#include <numkit/bundle/standard_engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class MskdemodTest : public ::testing::Test {
public:
    numkit::StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// mskmod -> mskdemod is the identity on the bit stream.
TEST_F(MskdemodTest, RoundTrip) {
    eval("d = [1 0 1 1 0 0 1 0]; z = mskdemod(mskmod(d, 8), 8);");
    EXPECT_EQ((int)evalScalar("numel(z)"), 8);
    EXPECT_EQ((int)evalScalar("sum(z(:) == d(:))"), 8);
}

// Longer pattern, different samples-per-symbol.
TEST_F(MskdemodTest, RoundTripLong) {
    eval("d = [1 0 0 1 1 1 0 1 0 0 1 0 1 1 0 0]; z = mskdemod(mskmod(d, 4), 4);");
    EXPECT_EQ((int)evalScalar("numel(z)"), 16);
    EXPECT_EQ((int)evalScalar("sum(z(:) == d(:))"), 16);
}

// Differential decision is invariant to a constant phase rotation.
TEST_F(MskdemodTest, PhaseRotationInvariant) {
    eval("d = [1 0 1 1 0 0 1 0]; y = mskmod(d, 8); z = mskdemod(y .* exp(1i*0.7), 8);");
    EXPECT_EQ((int)evalScalar("sum(z(:) == d(:))"), 8);
}

// ...and robust to moderate additive noise (deterministic perturbation).
TEST_F(MskdemodTest, NoiseRobust) {
    eval("d = [1 0 1 1 0 0 1 0]; y = mskmod(d, 8); n = 0.05*cos((1:numel(y))).';");
    eval("z = mskdemod(y(:) + n + 1i*0.05*sin((1:numel(y))).', 8);");
    EXPECT_EQ((int)evalScalar("sum(z(:) == d(:))"), 8);
}

// Second output is the final phase state (0 here: the bit sum is balanced).
TEST_F(MskdemodTest, PhaseOut) {
    eval("d = [1 0 1 1 0 0 1 0]; [z, ph] = mskdemod(mskmod(d, 8), 8);");
    EXPECT_NEAR(evalScalar("ph"), 0.0, 1e-12);
}

// Output keeps the input orientation: a column waveform -> a column bit stream.
TEST_F(MskdemodTest, ColumnOrientation) {
    eval("d = [1 0 1 1 0]; y = mskmod(d, 8); z = mskdemod(y(:), 8);");
    EXPECT_EQ((int)evalScalar("size(z,1)"), 5);
    EXPECT_EQ((int)evalScalar("size(z,2)"), 1);
}

// The non-differential path is deferred (matching the mskmod gap).
TEST_F(MskdemodTest, NonDiffRejected) {
    EXPECT_THROW(eval("mskdemod(mskmod([1 0 1],8), 8, 'nondiff');"), std::exception);
}
