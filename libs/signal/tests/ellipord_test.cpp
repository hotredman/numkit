// libs/signal/tests/ellipord_test.cpp
//
// Regression guard for ellipord (Phase 4.6). Bit-equal MATLAB R2025b.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class EllipordTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(EllipordTest, LowpassDigital)
{
    eval("[n, Wn] = ellipord(0.2, 0.3, 1, 40);");
    EXPECT_EQ(static_cast<int>(evalScalar("n")), 4);
    EXPECT_NEAR(evalScalar("Wn"), 0.2, 1e-9);
}

TEST_F(EllipordTest, HighpassDigital)
{
    eval("[n, Wn] = ellipord(0.6, 0.4, 3, 60);");
    EXPECT_EQ(static_cast<int>(evalScalar("n")), 5);
    EXPECT_NEAR(evalScalar("Wn"), 0.6, 1e-9);
}

TEST_F(EllipordTest, BandpassDigital)
{
    eval("[n, Wn] = ellipord([0.2 0.4], [0.1 0.5], 1, 40);");
    EXPECT_EQ(static_cast<int>(evalScalar("n")), 4);
    EXPECT_NEAR(evalScalar("Wn(1)"), 0.2, 1e-9);
    EXPECT_NEAR(evalScalar("Wn(2)"), 0.4, 1e-9);
}

TEST_F(EllipordTest, BandstopDigital)
{
    // bugs/signal/ellipord-bandstop.md — passband edges straddle the stopband.
    eval("[n, Wn] = ellipord([0.1 0.6], [0.2 0.5], 3, 40);");
    EXPECT_EQ(static_cast<int>(evalScalar("n")), 4);     // MATLAB n=4
    EXPECT_NEAR(evalScalar("Wn(1)"), 0.1, 1e-12);
    EXPECT_NEAR(evalScalar("Wn(2)"), 0.6, 1e-12);
    eval("[n2, W2] = ellipord([0.15 0.55], [0.25 0.45], 1, 60);");
    EXPECT_EQ(static_cast<int>(evalScalar("n2")), 5);    // tighter spec
    EXPECT_NEAR(evalScalar("W2(2)"), 0.55, 1e-12);
}

TEST_F(EllipordTest, BandstopAnalog)
{
    eval("[n, Wn] = ellipord([100 600], [200 500], 3, 40, 's');");
    EXPECT_EQ(static_cast<int>(evalScalar("n")), 5);     // MATLAB n=5
    EXPECT_NEAR(evalScalar("Wn(1)"), 100.0, 1e-9);
    EXPECT_NEAR(evalScalar("Wn(2)"), 600.0, 1e-9);
}

TEST_F(EllipordTest, AnalogLowpass)
{
    // 2π·1000 → 2π·1500 transition, Rp=1dB, Rs=40dB, analog 's' mode.
    eval("[n, Wn] = ellipord(2*pi*1000, 2*pi*1500, 1, 40, 's');");
    EXPECT_EQ(static_cast<int>(evalScalar("n")), 5);
    EXPECT_NEAR(evalScalar("Wn"), 6283.185307, 1e-3);  // 2π·1000
}

TEST_F(EllipordTest, BadRpRsRejected)
{
    bool threw = false;
    try { eval("ellipord(0.2, 0.3, -1, 40);"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}
