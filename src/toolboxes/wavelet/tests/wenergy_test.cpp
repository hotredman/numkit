// toolboxes/wavelet/tests/wenergy_test.cpp
//
// wenergy(C, L) — percentage of energy in the approximation (Ea) and each
// detail band (Ed). bugs/wavelet/wenergy.md. Reference values from MATLAB
// R2025b. Ed is ordered finest-first (level 1 … level N), the reverse of
// the C-vector packing.

#include <numkit/bundle/standard_engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class WenergyTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Ramp [1..8], 2 levels, db1: Ea + the two detail percentages (finest-first).
TEST_F(WenergyTest, RampDb1Level2)
{
    eval("[c, l] = wavedec([1 2 3 4 5 6 7 8], 2, 'db1'); [Ea, Ed] = wenergy(c, l);");
    EXPECT_NEAR(evalScalar("Ea"),    95.0980392157, 1e-7);
    EXPECT_NEAR(evalScalar("Ed(1)"),  0.98039216,   1e-6);   // level 1 (finest)
    EXPECT_NEAR(evalScalar("Ed(2)"),  3.92156863,   1e-6);   // level 2 (coarsest)
    EXPECT_EQ(static_cast<int>(evalScalar("numel(Ed)")), 2);
}

// Percentages always sum to 100.
TEST_F(WenergyTest, SumsTo100)
{
    eval("[c, l] = wavedec(1:64, 4, 'db2'); [Ea, Ed] = wenergy(c, l);");
    EXPECT_NEAR(evalScalar("Ea + sum(Ed)"), 100.0, 1e-9);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(Ed)")), 4);
}

// Ramp [1..16], 3 levels, db1 — Ed finest-first.
TEST_F(WenergyTest, RampDb1Level3)
{
    eval("[c, l] = wavedec(1:16, 3, 'db1'); [Ea, Ed] = wenergy(c, l);");
    EXPECT_NEAR(evalScalar("Ea"),    94.38502674, 1e-7);
    EXPECT_NEAR(evalScalar("Ed(1)"),  0.267380,   1e-5);   // level 1 (finest, least for a ramp)
    EXPECT_NEAR(evalScalar("Ed(2)"),  1.069519,   1e-5);   // level 2
    EXPECT_NEAR(evalScalar("Ed(3)"),  4.278075,   1e-5);   // level 3 (coarsest, most for a ramp)
}

// db2 wavelet on a sine.
TEST_F(WenergyTest, SineDb2Level2)
{
    eval("[c, l] = wavedec(sin(1:32), 2, 'db2'); [Ea, Ed] = wenergy(c, l);");
    EXPECT_NEAR(evalScalar("Ea"),    34.187599, 1e-5);
    EXPECT_NEAR(evalScalar("Ed(1)"),  9.833376, 1e-5);
    EXPECT_NEAR(evalScalar("Ed(2)"), 55.979025, 1e-5);
}
