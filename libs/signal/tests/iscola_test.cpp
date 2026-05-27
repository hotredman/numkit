// libs/signal/tests/iscola_test.cpp
//
// Regression guard for signal/iscola — COLA-compliance check.
// Fingerprints from MATLAB R2025b.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class IscolaTest : public ::testing::Test
{
public:
    numkit::Engine engine;
    void SetUp() override
    {
        engine.eval("import compat.*;");
        engine.eval("w  = hann(64, 'periodic');");
        engine.eval("w2 = hamming(64, 'periodic');");
        engine.eval("wr = ones(1, 64);");
    }
    double evalScalar(const std::string &c) { return engine.eval(c).toScalar(); }
};

// Hann at 50% overlap (hop=32): COLA under 'ola' with m=1.
TEST_F(IscolaTest, HannOlaFiftyPercent)
{
    engine.eval("[tf, m, dev] = iscola(w, 32, 'ola');");
    EXPECT_DOUBLE_EQ(evalScalar("tf"), 1.0);
    EXPECT_NEAR(evalScalar("m"),   1.0, 1e-14);
    EXPECT_LE(evalScalar("dev"),   1e-14);
}

// Hann at 50% under 'wola' (sum of w²): NOT COLA, m=0.75, dev=0.25.
TEST_F(IscolaTest, HannWolaFiftyPercent)
{
    engine.eval("[tf, m, dev] = iscola(w, 32, 'wola');");
    EXPECT_DOUBLE_EQ(evalScalar("tf"), 0.0);
    EXPECT_NEAR(evalScalar("m"),   0.75, 1e-12);
    EXPECT_NEAR(evalScalar("dev"), 0.25, 1e-12);
}

// Default method is 'wola' (MATLAB R2019a+).
TEST_F(IscolaTest, DefaultMethodIsWola)
{
    engine.eval("[tf1, m1] = iscola(w, 32);");
    engine.eval("[tf2, m2] = iscola(w, 32, 'wola');");
    EXPECT_DOUBLE_EQ(evalScalar("tf1"), evalScalar("tf2"));
    EXPECT_DOUBLE_EQ(evalScalar("m1"),  evalScalar("m2"));
}

// Hamming at 50%: COLA under 'ola' with m=1.08.
TEST_F(IscolaTest, HammingOlaFiftyPercent)
{
    engine.eval("[tf, m, dev] = iscola(w2, 32, 'ola');");
    EXPECT_DOUBLE_EQ(evalScalar("tf"), 1.0);
    EXPECT_NEAR(evalScalar("m"),   1.08, 1e-12);
}

// Rectangular hop=M (no overlap): trivially COLA with m=1.
TEST_F(IscolaTest, RectangularNoOverlap)
{
    engine.eval("[tf, m, dev] = iscola(wr, 0, 'ola');");
    EXPECT_DOUBLE_EQ(evalScalar("tf"), 1.0);
    EXPECT_NEAR(evalScalar("m"),   1.0, 1e-14);
    EXPECT_NEAR(evalScalar("dev"), 0.0, 1e-14);
}

// Hann at hop=33 (overlap=31): NOT COLA, dev ≈ 0.033.
TEST_F(IscolaTest, HannOddOverlapFails)
{
    engine.eval("[tf, m, dev] = iscola(w, 31, 'ola');");
    EXPECT_DOUBLE_EQ(evalScalar("tf"), 0.0);
    EXPECT_GT(evalScalar("dev"), 1e-4);
}

// Hann at 75% overlap (hop=16): COLA under 'ola' with m=2.
TEST_F(IscolaTest, HannOlaSeventyFivePercent)
{
    engine.eval("[tf, m, dev] = iscola(w, 48, 'ola');");
    EXPECT_DOUBLE_EQ(evalScalar("tf"), 1.0);
    EXPECT_NEAR(evalScalar("m"), 2.0, 1e-12);
}

// Single-output form returns just tf.
TEST_F(IscolaTest, SingleOutput)
{
    EXPECT_DOUBLE_EQ(evalScalar("iscola(w, 32, 'ola')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("iscola(w, 32, 'wola')"), 0.0);
}

// Errors: noverlap >= window length.
TEST_F(IscolaTest, OverlapTooLargeThrows)
{
    EXPECT_THROW(engine.eval("iscola(w, 64, 'ola');"), std::exception);
    EXPECT_THROW(engine.eval("iscola(w, 100, 'ola');"), std::exception);
}

// Unknown method throws.
TEST_F(IscolaTest, UnknownMethodThrows)
{
    EXPECT_THROW(engine.eval("iscola(w, 32, 'bogus');"), std::exception);
}
