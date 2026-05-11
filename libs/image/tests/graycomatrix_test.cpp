// libs/image/tests/graycomatrix_test.cpp
//
// gtest coverage for graycomatrix + graycoprops. The pair is the
// classical CLCM-based texture-analysis toolchain; fingerprints
// captured from MATLAB R2025b on the rotational 4x4 demo image.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class GraycomatrixTest : public ::testing::Test
{
public:
    numkit::Engine engine;
    void   SetUp() override {
        engine.eval("import compat.*;");
        engine.eval("I = uint8([1 2 3 4; 2 3 4 1; 3 4 1 2; 4 1 2 3] * 32);");
    }
    double eval_scalar(const std::string &c) { return engine.eval(c).toScalar(); }
};

TEST_F(GraycomatrixTest, DefaultsMatchMatlab)
{
    engine.eval("G = graycomatrix(I);");
    EXPECT_DOUBLE_EQ(eval_scalar("size(G, 1)"), 8.0);
    EXPECT_DOUBLE_EQ(eval_scalar("size(G, 2)"), 8.0);
    EXPECT_DOUBLE_EQ(eval_scalar("sum(G(:))"), 12.0);
    EXPECT_DOUBLE_EQ(eval_scalar("G(2, 3)"), 3.0);
    EXPECT_DOUBLE_EQ(eval_scalar("G(3, 4)"), 3.0);
    EXPECT_DOUBLE_EQ(eval_scalar("G(4, 5)"), 3.0);
    EXPECT_DOUBLE_EQ(eval_scalar("G(5, 2)"), 3.0);
}

TEST_F(GraycomatrixTest, PropsMatchMatlab)
{
    engine.eval("G = graycomatrix(I); s = graycoprops(G);");
    EXPECT_NEAR(eval_scalar("s.Contrast"),     3.0,    1e-12);
    EXPECT_NEAR(eval_scalar("s.Correlation"), -0.2,    1e-12);
    EXPECT_NEAR(eval_scalar("s.Energy"),       0.25,   1e-12);
    EXPECT_NEAR(eval_scalar("s.Homogeneity"),  0.4375, 1e-12);
}

TEST_F(GraycomatrixTest, NumLevelsAndOffsetAndSymmetric)
{
    engine.eval("G = graycomatrix(I, 'NumLevels', 4, 'Offset', [0 1], "
                "'Symmetric', true);");
    EXPECT_DOUBLE_EQ(eval_scalar("size(G, 1)"), 4.0);
    EXPECT_DOUBLE_EQ(eval_scalar("size(G, 2)"), 4.0);
    EXPECT_DOUBLE_EQ(eval_scalar("sum(G(:))"), 24.0);   // 2× non-symmetric
    EXPECT_DOUBLE_EQ(eval_scalar("G(2, 3)"), 3.0);
    EXPECT_DOUBLE_EQ(eval_scalar("G(3, 2)"), 3.0);      // symmetry pair
}

TEST_F(GraycomatrixTest, GrayLimitsCustom)
{
    // With GrayLimits == image range, the 4 levels become 1..4.
    engine.eval("G = graycomatrix(I, 'NumLevels', 4, "
                "'GrayLimits', [32 128]);");
    EXPECT_DOUBLE_EQ(eval_scalar("size(G, 1)"), 4.0);
    EXPECT_DOUBLE_EQ(eval_scalar("sum(G(:))"), 12.0);
}

TEST_F(GraycomatrixTest, BadNumLevelsThrows)
{
    EXPECT_THROW(engine.eval("graycomatrix(I, 'NumLevels', 1);"),
                 std::exception);
}

TEST_F(GraycomatrixTest, UnknownNVKeyThrows)
{
    EXPECT_THROW(engine.eval("graycomatrix(I, 'BogusKey', 5);"),
                 std::exception);
}

TEST_F(GraycomatrixTest, GraycopropsRejectsNonSquare)
{
    engine.eval("G = zeros(3, 4);");
    EXPECT_THROW(engine.eval("graycoprops(G);"), std::exception);
}
