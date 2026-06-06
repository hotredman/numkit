// libs/image/tests/imdiffusefilt_test.cpp
//
// Regression guard for imdiffusefilt — Perona-Malik anisotropic
// diffusion. Bit-equal MATLAB R2025b at 1e-9 tolerance.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ImdiffusefiltTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {
        engine.eval(
            "import compat.*;"
            "I = double([1 2 3 4 5; 6 7 8 9 10; 11 12 13 14 15;"
            " 16 17 18 19 20; 21 22 23 24 25]) / 25;");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── Default (5 iter, maximal 8-conn, exponential, K=0.1) ─────────

TEST_F(ImdiffusefiltTest, DefaultArgs)
{
    eval("B = imdiffusefilt(I);");
    EXPECT_NEAR(evalScalar("B(3,3)"), 0.52,          1e-9);
    EXPECT_NEAR(evalScalar("B(1,1)"), 0.07067272445, 1e-8);
    EXPECT_NEAR(evalScalar("B(5,5)"), 0.9693272756,  1e-8);
}

// ── NumberOfIterations override ────────────────────────────────────

TEST_F(ImdiffusefiltTest, NumIterations)
{
    eval("B = imdiffusefilt(I, 'NumberOfIterations', 3);");
    EXPECT_NEAR(evalScalar("B(3,3)"), 0.52, 1e-9);
}

// ── Connectivity = minimal (4-conn) ────────────────────────────────

TEST_F(ImdiffusefiltTest, MinimalConnectivity)
{
    eval("B = imdiffusefilt(I, 'Connectivity', 'minimal');");
    EXPECT_NEAR(evalScalar("B(3,3)"), 0.52, 1e-9);
}

// ── ConductionMethod = quadratic ──────────────────────────────────

TEST_F(ImdiffusefiltTest, QuadraticConduction)
{
    eval("B = imdiffusefilt(I, 'ConductionMethod', 'quadratic');");
    EXPECT_NEAR(evalScalar("B(3,3)"), 0.52, 1e-9);
}

// ── Scalar GradientThreshold ──────────────────────────────────────

TEST_F(ImdiffusefiltTest, ScalarThreshold)
{
    eval("B = imdiffusefilt(I, 'GradientThreshold', 0.5);");
    EXPECT_NEAR(evalScalar("B(3,3)"), 0.52, 1e-9);
}

// ── Vector GradientThreshold (N inferred from length) ────────────

TEST_F(ImdiffusefiltTest, VectorThreshold)
{
    eval("B = imdiffusefilt(I, 'GradientThreshold', [0.1 0.2 0.3]);");
    EXPECT_NEAR(evalScalar("B(3,3)"), 0.52, 1e-9);
}

// ── uint8 input class → uint8 output ──────────────────────────────

TEST_F(ImdiffusefiltTest, Uint8Class)
{
    eval("Iu = uint8(I*255); B = imdiffusefilt(Iu);");
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(3,3))")), 133);
    EXPECT_EQ(eval("class(B)").toString(), "uint8");
}

// ── Shape preserved ────────────────────────────────────────────────

TEST_F(ImdiffusefiltTest, ShapePreserved)
{
    eval("B = imdiffusefilt(I);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B, 1)")), 5);
    EXPECT_EQ(static_cast<int>(evalScalar("size(B, 2)")), 5);
}

// ── Errors ─────────────────────────────────────────────────────────

TEST_F(ImdiffusefiltTest, BadConnectivityThrows)
{
    EXPECT_THROW(eval("imdiffusefilt(I, 'Connectivity', 'wat');"),
                 std::exception);
}

TEST_F(ImdiffusefiltTest, BadConductionThrows)
{
    EXPECT_THROW(eval("imdiffusefilt(I, 'ConductionMethod', 'bad');"),
                 std::exception);
}

TEST_F(ImdiffusefiltTest, NegativeThreshThrows)
{
    EXPECT_THROW(eval("imdiffusefilt(I, 'GradientThreshold', -1);"),
                 std::exception);
}
