// libs/signal/tests/ctfutils_test.cpp
//
// Regression guard for ctf2zp + scaleFilterSections (Phase 4.11).
// Bit-equal MATLAB R2025b.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class CtfUtilsTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── ctf2zp ────────────────────────────────────────────────────────────
TEST_F(CtfUtilsTest, SingleSectionVectorInput)
{
    eval("[z, p, k] = ctf2zp([1 -1 0.5], [1 -0.6 0.2]);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(z)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(p)")), 2);
    EXPECT_NEAR(evalScalar("real(k)"), 1.0, 1e-9);
}

TEST_F(CtfUtilsTest, MultiSectionGainProduct)
{
    eval("NUM = [1 -1 0.5; 1 0 -1]; DEN = [1 -0.6 0.2; 1 0 -0.25];"
         "[z, p, k] = ctf2zp(NUM, DEN);");
    EXPECT_NEAR(evalScalar("real(k)"), 1.0, 1e-9);
}

TEST_F(CtfUtilsTest, SVScalingAccumulates)
{
    eval("NUM = [1 -1 0.5; 1 0 -1]; DEN = [1 -0.6 0.2; 1 0 -0.25];"
         "[~, ~, k] = ctf2zp(NUM, DEN, [2 3 5]);");
    EXPECT_NEAR(evalScalar("real(k)"), 30.0, 1e-9);  // 2*3*5
}

// ── scaleFilterSections ───────────────────────────────────────────────
TEST_F(CtfUtilsTest, VectorScaleDistributedAcrossSections)
{
    eval("ctf = [1 2 1; 1 -1 0.5]; sv = scaleFilterSections(ctf, [2 3 5]);");
    // Expected per MATLAB (manually computed):
    // sv[1,:] = sqrt(5) * 2 * [1 2 1] = [4.4721, 8.9443, 4.4721]
    // sv[2,:] = sign(5) * sqrt(5) * 3 * [1 -1 0.5] = [6.7082, -6.7082, 3.3541]
    EXPECT_NEAR(evalScalar("sv(1, 1)"),  4.47214, 1e-5);
    EXPECT_NEAR(evalScalar("sv(1, 2)"),  8.94427, 1e-5);
    EXPECT_NEAR(evalScalar("sv(2, 1)"),  6.70820, 1e-5);
    EXPECT_NEAR(evalScalar("sv(2, 2)"), -6.70820, 1e-5);
    EXPECT_NEAR(evalScalar("sv(2, 3)"),  3.35410, 1e-5);
}

TEST_F(CtfUtilsTest, ScalarScaleUniformDistribution)
{
    // Scalar SV → |sv|^(1/K) on each row, sign on last.
    eval("ctf = [1 2 1; 1 -1 0.5]; sv = scaleFilterSections(ctf, 4);");
    // |4|^(1/2) = 2; row 1 = 2*[1 2 1]; row 2 = sign(4)*2*[1 -1 0.5]
    EXPECT_NEAR(evalScalar("sv(1, 1)"),  2.0, 1e-9);
    EXPECT_NEAR(evalScalar("sv(2, 2)"), -2.0, 1e-9);
}

TEST_F(CtfUtilsTest, AllOnesSVReturnsOriginal)
{
    eval("ctf = [1 2 1; 1 -1 0.5]; sv = scaleFilterSections(ctf, [1 1 1]);");
    EXPECT_NEAR(evalScalar("sv(1, 1)"),  1.0, 1e-12);
    EXPECT_NEAR(evalScalar("sv(2, 2)"), -1.0, 1e-12);
}
