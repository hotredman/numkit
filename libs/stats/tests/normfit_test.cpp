// libs/stats/tests/normfit_test.cpp
// normfit.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class NormfitTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override {
        engine.eval("import compat.*;");
        engine.eval("x = [1.2 2.4 3.1 4.5 5.0 6.2 7.1]';");
        engine.eval("cens = [0 0 0 0 0 1 1]';");
        engine.eval("freq = [2 2 2 1 1 1 1]';");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Reference values from MATLAB R2025b probe.

TEST_F(NormfitTest, BasicNoCensNoFreq)
{
    eval("[mu, sd, muci, sdci] = normfit(x);");
    EXPECT_NEAR(evalScalar("mu"), 4.2142857143, 1e-7);
    EXPECT_NEAR(evalScalar("sd"), 2.1050958580, 1e-7);
    EXPECT_NEAR(evalScalar("muci(1)"), 2.2673967602, 1e-7);
    EXPECT_NEAR(evalScalar("muci(2)"), 6.1611746684, 1e-7);
    EXPECT_NEAR(evalScalar("sdci(1)"), 1.3565098940, 1e-7);
    EXPECT_NEAR(evalScalar("sdci(2)"), 4.6355602532, 1e-7);
}

// 2026-05-08 — gap closure: censored MLE via EM iteration on x.
TEST_F(NormfitTest, CensoredOnly)
{
    eval("[mu, sd, muci, sdci] = normfit(x, 0.05, cens);");
    EXPECT_NEAR(evalScalar("mu"), 4.6418203244, 1e-7);
    EXPECT_NEAR(evalScalar("sd"), 2.5994452841, 1e-7);
    EXPECT_NEAR(evalScalar("muci(1)"), 2.6101081063, 1e-6);
    EXPECT_NEAR(evalScalar("muci(2)"), 6.6735325426, 1e-6);
    EXPECT_NEAR(evalScalar("sdci(1)"), 1.3275144566, 1e-6);
    EXPECT_NEAR(evalScalar("sdci(2)"), 5.0900506217, 1e-6);
}

// gap closure: freq weighting via closed-form weighted moments.
TEST_F(NormfitTest, FrequencyOnly)
{
    eval("[mu, sd, muci, sdci] = normfit(x, 0.05, [], freq);");
    EXPECT_NEAR(evalScalar("mu"), 3.6200000000, 1e-7);
    EXPECT_NEAR(evalScalar("sd"), 2.0186904446, 1e-7);
    EXPECT_NEAR(evalScalar("muci(1)"), 2.1759158494, 1e-7);
    EXPECT_NEAR(evalScalar("muci(2)"), 5.0640841506, 1e-7);
    EXPECT_NEAR(evalScalar("sdci(1)"), 1.3885263593, 1e-7);
    EXPECT_NEAR(evalScalar("sdci(2)"), 3.6853418310, 1e-7);
}

// gap closure: combined cens + freq via weighted EM iteration.
TEST_F(NormfitTest, CensoredAndFreq)
{
    eval("[mu, sd, muci, sdci] = normfit(x, 0.05, cens, freq);");
    EXPECT_NEAR(evalScalar("mu"), 3.8482097044, 1e-6);
    EXPECT_NEAR(evalScalar("sd"), 2.3323500304, 1e-6);
    EXPECT_NEAR(evalScalar("muci(1)"), 2.3655855351, 1e-5);
    EXPECT_NEAR(evalScalar("muci(2)"), 5.3308338737, 1e-5);
    EXPECT_NEAR(evalScalar("sdci(1)"), 1.3845490290, 1e-5);
    EXPECT_NEAR(evalScalar("sdci(2)"), 3.9289736588, 1e-5);
}
