// toolboxes/signal/tests/risetime_falltime_test.cpp
//
// Regression guard for bugs/signal/risetime-falltime-outputs.md:
//   (1) VALUE fix — a sharp single-sample edge crosses both the 10% and 90%
//       reference levels in one interval; findTransitions used to pin the
//       upper crossing to the following (flat) interval, giving R = 0.224
//       instead of MATLAB's 0.198.
//   (2) the [R, LT, UT, LL, UL] outputs (duration, lower/upper crossing
//       times, lower/upper reference levels).
// Expected values from MATLAB R2025b.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class RisetimeFalltimeTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(RisetimeFalltimeTest, SharpRiseValueAndOutputs)
{
    eval("[R, LT, UT, LL, UL] = risetime([0 0 0 1 1 1 1], 4);");
    EXPECT_NEAR(evalScalar("R"),  0.198, 1e-6);   // was 0.224 (sharp-edge bug)
    EXPECT_NEAR(evalScalar("LT"), 0.526, 1e-6);   // 10% crossing time
    EXPECT_NEAR(evalScalar("UT"), 0.724, 1e-6);   // 90% crossing time
    EXPECT_NEAR(evalScalar("LL"), 0.104, 1e-6);   // lower reference level
    EXPECT_NEAR(evalScalar("UL"), 0.896, 1e-6);   // upper reference level
}

TEST_F(RisetimeFalltimeTest, SharpFallValueAndOutputs)
{
    eval("[F, LT, UT, LL, UL] = falltime([1 1 1 0 0 0 0], 4);");
    EXPECT_NEAR(evalScalar("F"),  0.198, 1e-6);
    EXPECT_NEAR(evalScalar("LT"), 0.724, 1e-6);   // lower crossing comes LAST
    EXPECT_NEAR(evalScalar("UT"), 0.526, 1e-6);   // upper crossing comes FIRST
    EXPECT_NEAR(evalScalar("LL"), 0.104, 1e-6);
    EXPECT_NEAR(evalScalar("UL"), 0.896, 1e-6);
}

TEST_F(RisetimeFalltimeTest, MultiSampleRampUnchanged)
{
    // Gradual ramp (crossings in different intervals) was already correct.
    EXPECT_NEAR(evalScalar("risetime([0 0 0.25 0.5 0.75 1 1], 1)"), 3.168, 1e-6);
}

TEST_F(RisetimeFalltimeTest, TwoRisingTransitions)
{
    eval("[R, LT, UT] = risetime([0 0 1 1 1 0 0 1 1 1], 2);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(R)")), 2);
    EXPECT_NEAR(evalScalar("R(1)"),  0.396, 1e-6);
    EXPECT_NEAR(evalScalar("R(2)"),  0.396, 1e-6);
    EXPECT_NEAR(evalScalar("LT(1)"), 0.552, 1e-6);
    EXPECT_NEAR(evalScalar("LT(2)"), 3.052, 1e-6);
    EXPECT_NEAR(evalScalar("UT(2)"), 3.448, 1e-6);
}

TEST_F(RisetimeFalltimeTest, SingleOutputStillWorks)
{
    EXPECT_NEAR(evalScalar("risetime([0 0 0 1 1 1 1], 4)"), 0.198, 1e-6);
    EXPECT_NEAR(evalScalar("falltime([1 1 1 0 0 0 0], 4)"), 0.198, 1e-6);
}
