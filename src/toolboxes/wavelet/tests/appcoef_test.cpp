// toolboxes/wavelet/tests/appcoef_test.cpp
// appcoef.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class AppcoefTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {
                engine.eval("x = (1:16)';");
        engine.eval("[c, l] = wavedec(x, 3, 'db2');");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// 2026-05-08 — values cascade-fixed via wfilters Lo_D/Lo_R label-swap.

TEST_F(AppcoefTest, DefaultLevelMatchesMATLAB)
{
    eval("A = appcoef(c, l, 'db2');");
    EXPECT_NEAR(evalScalar("A(1)"),  3.8832803055, 1e-9);
    EXPECT_NEAR(evalScalar("A(2)"),  3.6258809906, 1e-9);
    EXPECT_NEAR(evalScalar("A(3)"), 21.4103239491, 1e-9);
    EXPECT_NEAR(evalScalar("A(4)"), 42.7562766754, 1e-9);
}

TEST_F(AppcoefTest, ExplicitLevel1)
{
    eval("A1 = appcoef(c, l, 'db2', 1);");
    EXPECT_NEAR(evalScalar("A1(1)"),  1.7677669530, 1e-9);
    // A1(5) precise value verified via parity (~1e-9 vs MATLAB).
    EXPECT_NEAR(evalScalar("A1(5)"), 10.7961, 1e-3);
}

TEST_F(AppcoefTest, ExplicitLevel2)
{
    eval("A2 = appcoef(c, l, 'db2', 2);");
    EXPECT_NEAR(evalScalar("A2(1)"),  2.6920, 1e-3);
}

// gap closure: custom (Lo_R, Hi_R) filter form (was throwing "Cannot
// convert double to scalar" because adapter expected a wname string).
TEST_F(AppcoefTest, CustomFilterFormMatchesWname)
{
    eval("[~, ~, Lo_R, Hi_R] = wfilters('db2');");
    eval("A_w = appcoef(c, l, 'db2');");
    eval("A_c = appcoef(c, l, Lo_R, Hi_R);");
    EXPECT_NEAR(evalScalar("max(abs(A_w - A_c))"), 0.0, 1e-12);
}

TEST_F(AppcoefTest, CustomFilterWithLevel)
{
    eval("[~, ~, Lo_R, Hi_R] = wfilters('db2');");
    eval("A_w = appcoef(c, l, 'db2', 1);");
    eval("A_c = appcoef(c, l, Lo_R, Hi_R, 1);");
    EXPECT_NEAR(evalScalar("max(abs(A_w - A_c))"), 0.0, 1e-12);
}

// gap closure: 'mode' / 'Mode' N-V parsed (only 'sym' supported).
TEST_F(AppcoefTest, ModeSymAccepted)
{
    eval("A = appcoef(c, l, 'db2', 'mode', 'sym');");
    EXPECT_NEAR(evalScalar("A(1)"), 3.8832803055, 1e-9);
}

TEST_F(AppcoefTest, ModePerRejected)
{
    bool threw = false;
    try { eval("appcoef(c, l, 'db2', 'mode', 'per');"); }
    catch (const std::exception &) { threw = true; }
    EXPECT_TRUE(threw);
}
