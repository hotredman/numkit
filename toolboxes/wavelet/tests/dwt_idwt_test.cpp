// toolboxes/wavelet/tests/dwt_idwt_test.cpp
// dwt + idwt + wavedec + waverec.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class DwtIdwtTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// 2026-05-08 — values cascade-fixed via wfilters Lo_D/Lo_R label-swap.

TEST_F(DwtIdwtTest, Db2WnameMatchesMATLAB)
{
    eval("[cA, cD] = dwt((1:8)', 'db2');");
    EXPECT_NEAR(evalScalar("cA(1)"),  1.7677669530, 1e-9);
    EXPECT_NEAR(evalScalar("cA(5)"), 10.9601551084, 1e-9);
    EXPECT_NEAR(evalScalar("cD(1)"), -0.6123724357, 1e-9);
    EXPECT_NEAR(evalScalar("cD(5)"),  0.6123724357, 1e-9);
}

// gap closure: custom (Lo_D, Hi_D) filter form — adapter previously
// threw "expected a string" on numeric 2nd arg.
TEST_F(DwtIdwtTest, CustomFilterFormMatchesWnameForm)
{
    eval("[Lo_D, Hi_D] = wfilters('db2');");
    eval("[cA1, cD1] = dwt((1:8)', Lo_D, Hi_D);");
    eval("[cA2, cD2] = dwt((1:8)', 'db2');");
    EXPECT_NEAR(evalScalar("max(abs(cA1 - cA2))"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("max(abs(cD1 - cD2))"), 0.0, 1e-12);
}

// gap closure: 'mode' N-V is parsed (only 'sym' supported, others
// throw a clear error instead of silently using 'sym').
TEST_F(DwtIdwtTest, ModeSymAccepted)
{
    eval("[cA, cD] = dwt((1:8)', 'db2', 'mode', 'sym');");
    EXPECT_NEAR(evalScalar("cA(1)"), 1.7677669530, 1e-9);
}

TEST_F(DwtIdwtTest, ModePerRejected)
{
    bool threw = false;
    try { eval("dwt((1:8)', 'db2', 'mode', 'per');"); }
    catch (const std::exception &) { threw = true; }
    EXPECT_TRUE(threw);
}

// idwt round-trip preserved at ~1e-12.
TEST_F(DwtIdwtTest, IdwtRoundTripDb4)
{
    eval("x = (1:16)';");
    eval("[cA, cD] = dwt(x, 'db4');");
    eval("xr = idwt(cA, cD, 'db4');");
    EXPECT_LT(evalScalar("max(abs(xr(:) - x(:)))"), 1e-10);
}

// gap closure: idwt custom (Lo_R, Hi_R) form.
TEST_F(DwtIdwtTest, IdwtCustomFilterForm)
{
    eval("[~, ~, Lo_R, Hi_R] = wfilters('db2');");
    eval("[cA, cD] = dwt((1:8)', 'db2');");
    eval("xr = idwt(cA, cD, Lo_R, Hi_R);");
    EXPECT_LT(evalScalar("max(abs(xr(:) - (1:8)'))"), 1e-10);
}

// wavedec multi-level cascade.
TEST_F(DwtIdwtTest, WavedecMultilevelMatchesMATLAB)
{
    eval("[c, l] = wavedec((1:16)', 3, 'db2');");
    EXPECT_NEAR(evalScalar("c(1)"),  3.8832803055, 1e-9);
    EXPECT_NEAR(evalScalar("c(2)"),  3.6258809906, 1e-9);
    EXPECT_NEAR(evalScalar("c(3)"), 21.4103239491, 1e-9);
    EXPECT_NEAR(evalScalar("c(4)"), 42.7562766754, 1e-9);
    EXPECT_DOUBLE_EQ(evalScalar("l(1)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("l(2)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("l(3)"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("l(4)"), 9.0);
    EXPECT_DOUBLE_EQ(evalScalar("l(5)"), 16.0);
}

TEST_F(DwtIdwtTest, WaverecRoundTrip)
{
    eval("x = (1:32)';");
    eval("[c, l] = wavedec(x, 4, 'sym4');");
    eval("xr = waverec(c, l, 'sym4');");
    EXPECT_LT(evalScalar("max(abs(xr(:) - x(:)))"), 1e-9);
}
