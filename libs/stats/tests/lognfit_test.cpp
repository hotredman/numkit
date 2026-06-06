// libs/stats/tests/lognfit_test.cpp
// lognfit.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class LognfitTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override {
        engine.eval("import compat.*;");
        engine.eval("x = [1 2 3 4 5 6 7 8 9 10]';");
        engine.eval("cens = [0 0 0 0 0 0 0 1 1 1]';");
        engine.eval("freq = [2 2 2 1 1 1 1 1 1 1]';");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Reference values from MATLAB R2025b probe.

TEST_F(LognfitTest, BasicNoCensNoFreq)
{
    eval("[p, c] = lognfit(x);");
    EXPECT_NEAR(evalScalar("p(1)"), 1.5104412573, 1e-7);
    EXPECT_NEAR(evalScalar("p(2)"), 0.7330238657, 1e-7);
    EXPECT_NEAR(evalScalar("c(1,1)"), 0.9860675727, 1e-7);
    EXPECT_NEAR(evalScalar("c(2,1)"), 2.0348149419, 1e-7);
    EXPECT_NEAR(evalScalar("c(1,2)"), 0.5041996222, 1e-7);
    EXPECT_NEAR(evalScalar("c(2,2)"), 1.3382158333, 1e-7);
}

// 2026-05-08 — gap closure: censored MLE via EM iteration on log(x).
TEST_F(LognfitTest, CensoredOnly)
{
    eval("[p, c] = lognfit(x, 0.05, cens);");
    EXPECT_NEAR(evalScalar("p(1)"), 1.6856246465, 1e-7);
    EXPECT_NEAR(evalScalar("p(2)"), 0.9277676177, 1e-7);
    EXPECT_NEAR(evalScalar("c(1,1)"), 1.0724981385, 1e-6);
    EXPECT_NEAR(evalScalar("c(2,1)"), 2.2987511544, 1e-6);
    EXPECT_NEAR(evalScalar("c(1,2)"), 0.5288251852, 1e-6);
    EXPECT_NEAR(evalScalar("c(2,2)"), 1.6276697414, 1e-6);
}

// gap closure: freq weighting via closed-form weighted moments.
TEST_F(LognfitTest, FrequencyOnly)
{
    eval("[p, c] = lognfit(x, 0.05, [], freq);");
    EXPECT_NEAR(evalScalar("p(1)"), 1.2997055417, 1e-7);
    EXPECT_NEAR(evalScalar("p(2)"), 0.7840916903, 1e-7);
    EXPECT_NEAR(evalScalar("c(1,1)"), 0.8258836754, 1e-7);
    EXPECT_NEAR(evalScalar("c(2,1)"), 1.7735274080, 1e-7);
    EXPECT_NEAR(evalScalar("c(1,2)"), 0.5622611625, 1e-7);
    EXPECT_NEAR(evalScalar("c(2,2)"), 1.2943277053, 1e-7);
}

// gap closure: combined cens + freq via weighted EM iteration.
TEST_F(LognfitTest, CensoredAndFreq)
{
    eval("[p, c] = lognfit(x, 0.05, cens, freq);");
    EXPECT_NEAR(evalScalar("p(1)"), 1.4214576479, 1e-6);
    EXPECT_NEAR(evalScalar("p(2)"), 0.9373415041, 1e-6);
    EXPECT_NEAR(evalScalar("c(1,1)"), 0.8924682721, 1e-5);
    EXPECT_NEAR(evalScalar("c(2,1)"), 1.9504470238, 1e-5);
    EXPECT_NEAR(evalScalar("c(1,2)"), 0.5887466083, 1e-5);
    EXPECT_NEAR(evalScalar("c(2,2)"), 1.4923382706, 1e-5);
}
