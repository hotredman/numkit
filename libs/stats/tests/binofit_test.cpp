// libs/stats/tests/binofit_test.cpp
//
// Backfill gtest for binofit (pre-2026-05-04 function). Reference
// values from MATLAB R2025b probe (audit/closed/stats/binofit.md).
// Closes audit/findings/stats/binofit.md (coverage gap).

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class BinofitTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(BinofitTest, ScalarPhat)
{
    EXPECT_NEAR(evalScalar("binofit(7, 10)"), 0.7, 1e-12);
}

TEST_F(BinofitTest, ScalarCI)
{
    eval("[ph, pci] = binofit(7, 10);");
    EXPECT_NEAR(evalScalar("pci(1)"), 0.3475471499, 1e-9);
    EXPECT_NEAR(evalScalar("pci(2)"), 0.9332604888, 1e-9);
}

TEST_F(BinofitTest, VectorInputs)
{
    eval("[ph, pci] = binofit([3 5 7]', [10 10 10]');");
    EXPECT_NEAR(evalScalar("ph(1)"), 0.30, 1e-12);
    EXPECT_NEAR(evalScalar("ph(2)"), 0.50, 1e-12);
    EXPECT_NEAR(evalScalar("ph(3)"), 0.70, 1e-12);
    EXPECT_NEAR(evalScalar("pci(1,1)"), 0.0667395112, 1e-9);
    EXPECT_NEAR(evalScalar("pci(1,2)"), 0.6524528501, 1e-9);
    EXPECT_NEAR(evalScalar("pci(3,1)"), 0.3475471499, 1e-9);
    EXPECT_NEAR(evalScalar("pci(3,2)"), 0.9332604888, 1e-9);
}

TEST_F(BinofitTest, EdgeXIsZero)
{
    eval("[ph, pci] = binofit(0, 10);");
    EXPECT_DOUBLE_EQ(evalScalar("ph"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("pci(1)"), 0.0);
    EXPECT_NEAR(evalScalar("pci(2)"), 0.3084971078, 1e-9);
}

TEST_F(BinofitTest, EdgeXEqualsN)
{
    eval("[ph, pci] = binofit(10, 10);");
    EXPECT_DOUBLE_EQ(evalScalar("ph"), 1.0);
    EXPECT_NEAR(evalScalar("pci(1)"), 0.6915028922, 1e-9);
    EXPECT_DOUBLE_EQ(evalScalar("pci(2)"), 1.0);
}

TEST_F(BinofitTest, NonDefaultAlpha)
{
    // alpha=0.01 → wider CI than the default 0.05.
    eval("[ph, pci] = binofit(7, 10, 0.01);");
    EXPECT_NEAR(evalScalar("ph"), 0.7, 1e-12);
    // MATLAB R2025b: pci = [0.2200629; 0.9747159] (rough).
    EXPECT_LT(evalScalar("pci(1)"), 0.3475471499);  // wider lower
    EXPECT_GT(evalScalar("pci(2)"), 0.9332604888);  // wider upper
}

TEST_F(BinofitTest, NonPositiveNReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("binofit(0, 0)")));
}
