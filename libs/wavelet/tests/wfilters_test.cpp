// libs/wavelet/tests/wfilters_test.cpp
// wfilters.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class WfiltersTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// 2026-05-08 — gap closure: previously numkit's Lo_D / Lo_R labels
// were swapped relative to MATLAB R2025b. Root cause of the
// dwt / wavedec value mismatches. Now bit-identical.

TEST_F(WfiltersTest, Db2LabelsMatchMATLAB)
{
    eval("[Lo_D, Hi_D, Lo_R, Hi_R] = wfilters('db2');");
    // MATLAB R2025b: Lo_D = [-0.1294, 0.2241, 0.8365, 0.4830]
    EXPECT_NEAR(evalScalar("Lo_D(1)"), -0.12940952255092145, 1e-12);
    EXPECT_NEAR(evalScalar("Lo_D(4)"),  0.48296291314469025, 1e-12);
    // Lo_R = wrev(Lo_D) = [0.4830, 0.8365, 0.2241, -0.1294]
    EXPECT_NEAR(evalScalar("Lo_R(1)"),  0.48296291314469025, 1e-12);
    EXPECT_NEAR(evalScalar("Lo_R(4)"), -0.12940952255092145, 1e-12);
    // Hi_R = QMF(Lo_R)
    EXPECT_NEAR(evalScalar("Hi_R(1)"), -0.12940952255092145, 1e-12);
    EXPECT_NEAR(evalScalar("Hi_R(4)"), -0.48296291314469025, 1e-12);
    // Hi_D = wrev(Hi_R)
    EXPECT_NEAR(evalScalar("Hi_D(1)"), -0.48296291314469025, 1e-12);
    EXPECT_NEAR(evalScalar("Hi_D(4)"), -0.12940952255092145, 1e-12);
}

// gap closure: 1-output 'd' / 'r' / 'l' / 'h' form returns a 2×Lf
// matrix (was returning two separate row vectors).
TEST_F(WfiltersTest, OneOutputFormReturns2xLf)
{
    eval("Fd = wfilters('db2', 'd');");
    EXPECT_DOUBLE_EQ(evalScalar("size(Fd, 1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(Fd, 2)"), 4.0);
    // Row 1 = Lo_D, row 2 = Hi_D.
    EXPECT_NEAR(evalScalar("Fd(1, 1)"), -0.12940952255092145, 1e-12);
    EXPECT_NEAR(evalScalar("Fd(2, 1)"), -0.48296291314469025, 1e-12);
}

TEST_F(WfiltersTest, OneOutputRForm)
{
    eval("Fr = wfilters('db2', 'r');");
    EXPECT_DOUBLE_EQ(evalScalar("size(Fr, 1)"), 2.0);
    // Row 1 = Lo_R, row 2 = Hi_R.
    EXPECT_NEAR(evalScalar("Fr(1, 1)"),  0.48296291314469025, 1e-12);
    EXPECT_NEAR(evalScalar("Fr(2, 1)"), -0.12940952255092145, 1e-12);
}

// Cascade verification: after the wfilters fix, dwt now matches MATLAB
// without any change to dwt itself (just the downsample-offset tweak
// that landed in the same commit).
TEST_F(WfiltersTest, DwtCascadeMatchesMATLABOnDb2)
{
    eval("x = (1:8)';");
    eval("[cA, cD] = dwt(x, 'db2');");
    EXPECT_NEAR(evalScalar("cA(1)"),  1.7677669530, 1e-9);
    EXPECT_NEAR(evalScalar("cA(5)"), 10.9601551084, 1e-9);
    EXPECT_NEAR(evalScalar("cD(1)"), -0.6123724357, 1e-9);
    EXPECT_NEAR(evalScalar("cD(5)"),  0.6123724357, 1e-9);
}

TEST_F(WfiltersTest, RoundTripPreservedAfterFix)
{
    eval("x = (1:16)';");
    eval("[cA, cD] = dwt(x, 'db4');");
    eval("xr = idwt(cA, cD, 'db4');");
    EXPECT_LT(evalScalar("max(abs(xr(:) - x(:)))"), 1e-10);
}
