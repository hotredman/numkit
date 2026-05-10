// libs/audio/tests/spectral_shape_test.cpp
//
// Regression guard for Audio Toolbox spectral shape descriptors
// (cycle B). Tests use the deterministic (X, F) direct form for
// per-metric value checks. Time-domain (x, fs) form is exercised
// in the smoke .m and the parity spec.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class SpectralShapeTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override {
        engine.eval("import compat.*;");
        engine.eval("X = [4; 3; 2; 1]; F = [100; 200; 300; 400];");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── direct (X, F) form ────────────────────────────────────────────────
TEST_F(SpectralShapeTest, CentroidFirstMoment)
{
    EXPECT_DOUBLE_EQ(evalScalar("spectralCentroid(X, F)"), 200.0);
}

TEST_F(SpectralShapeTest, SpreadSecondMoment)
{
    EXPECT_DOUBLE_EQ(evalScalar("spectralSpread(X, F)"), 100.0);
}

TEST_F(SpectralShapeTest, RolloffPointDefault95)
{
    EXPECT_DOUBLE_EQ(evalScalar("spectralRolloffPoint(X, F)"), 400.0);
}

TEST_F(SpectralShapeTest, RolloffPointCustomThreshold)
{
    // Threshold 0.5: cumsum [4 7 9 10], 50% of 10 = 5 → idx 2 → F=200.
    EXPECT_DOUBLE_EQ(evalScalar("spectralRolloffPoint(X, F, 0.5)"), 200.0);
}

TEST_F(SpectralShapeTest, DecreaseSlope)
{
    EXPECT_NEAR(evalScalar("spectralDecrease(X, F)"), -0.5, 1e-12);
}

TEST_F(SpectralShapeTest, Slope)
{
    EXPECT_NEAR(evalScalar("spectralSlope(X, F)"), -0.01, 1e-12);
}

// ── two-column matrix form ────────────────────────────────────────────
TEST_F(SpectralShapeTest, CentroidPerColumn)
{
    eval("X2 = [4 1; 3 2; 2 3; 1 4]; F2 = [100; 200; 300; 400];");
    eval("sc = spectralCentroid(X2, F2);");
    EXPECT_DOUBLE_EQ(evalScalar("sc(1)"), 200.0);
    EXPECT_DOUBLE_EQ(evalScalar("sc(2)"), 300.0);
}

TEST_F(SpectralShapeTest, FluxFirstFrameZero)
{
    eval("X2 = [4 1; 3 2; 2 3; 1 4]; F2 = [100; 200; 300; 400];");
    eval("sf = spectralFlux(X2, F2);");
    EXPECT_DOUBLE_EQ(evalScalar("sf(1)"), 0.0);  // MATLAB convention
    EXPECT_NEAR(evalScalar("sf(2)"), 4.47213595, 1e-6);
}

TEST_F(SpectralShapeTest, FluxCustomP)
{
    eval("X2 = [4 1; 3 2; 2 3; 1 4]; F2 = [100; 200; 300; 400];");
    eval("sf = spectralFlux(X2, F2, 1);");  // L1 norm
    EXPECT_NEAR(evalScalar("sf(2)"), 8.0, 1e-12);  // |3|+|1|+|-1|+|-3| = 8
}

// ── time-domain (x, fs) form ──────────────────────────────────────────
// Uses internal STFT (rectwin 30ms, overlap 20ms). For a pure tone we
// expect the centroid to be near the fundamental frequency.
TEST_F(SpectralShapeTest, TimeDomainSineFundamental)
{
    eval("fs = 8000; t = (0:1/fs:0.05)'; x = sin(2*pi*440*t);"
         "sc = spectralCentroid(x, fs);");
    EXPECT_GE(static_cast<int>(evalScalar("numel(sc)")), 1);
    // Within DFT bin granularity (fs/winLen = 8000/240 ≈ 33 Hz).
    EXPECT_NEAR(evalScalar("sc(1)"), 440.0, 50.0);
}

TEST_F(SpectralShapeTest, EmptyTooShortReturnsEmpty)
{
    eval("fs = 8000; x = ones(10, 1); sc = spectralCentroid(x, fs);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(sc)")), 0);
}
