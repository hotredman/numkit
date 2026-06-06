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
    numkit::StdEngine engine;
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

// 'GrayLimits', [] (empty) = MATLAB-documented auto limits [min(I) max(I)]
// over the actual data — for any class. For this uint8 image the data range
// is [32 128], so the empty form must equal the explicit [32 128] GLCM and
// must differ from the class-range default [0 255]. numkit previously threw
// "GrayLimits must be 2-element" on the empty form (DEEP-PROBE c168).
TEST_F(GraycomatrixTest, GrayLimitsEmptyAutoDataRange)
{
    engine.eval("Ge = graycomatrix(I, 'NumLevels', 4, 'GrayLimits', []);");
    engine.eval("Gx = graycomatrix(I, 'NumLevels', 4, 'GrayLimits', [32 128]);");
    engine.eval("Gd = graycomatrix(I, 'NumLevels', 4);");   // default [0 255]
    EXPECT_DOUBLE_EQ(eval_scalar("size(Ge, 1)"), 4.0);
    EXPECT_DOUBLE_EQ(eval_scalar("sum(Ge(:))"), 12.0);
    EXPECT_DOUBLE_EQ(eval_scalar("Ge(1, 2)"), 3.0);
    EXPECT_DOUBLE_EQ(eval_scalar("Ge(3, 4)"), 3.0);
    EXPECT_DOUBLE_EQ(eval_scalar("double(isequal(Ge, Gx))"), 1.0);   // = data range
    EXPECT_DOUBLE_EQ(eval_scalar("double(isequal(Ge, Gd))"), 0.0);   // != class range
}

// A scalar (non-empty, <2-element) GrayLimits must still error.
TEST_F(GraycomatrixTest, GrayLimitsScalarThrows)
{
    EXPECT_THROW(engine.eval("graycomatrix(I, 'NumLevels', 4, 'GrayLimits', 5);"),
                 std::exception);
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
