// libs/image/tests/adapthisteq_test.cpp
//
// gtest coverage for adapthisteq — CLAHE. Pins output class / size /
// corner saturation behaviour; the interior is exercised by smoke +
// parity (parity uses a wide tol because MATLAB's "single tile in
// outer-half corner regions" rule isn't replicated yet — known gap).

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class AdaptHistEqTest : public ::testing::Test
{
public:
    numkit::Engine engine;
    void   SetUp() override { engine.eval("import compat.*;"); }
    double eval_scalar(const std::string &c) { return engine.eval(c).toScalar(); }
};

// Output preserves input class + size.
TEST_F(AdaptHistEqTest, PreservesClassAndSize)
{
    engine.eval("I = uint8(reshape(0:255, 16, 16));");
    engine.eval("J = adapthisteq(I);");
    EXPECT_EQ(eval_scalar("strcmp(class(J), 'uint8')"), 1.0);
    EXPECT_DOUBLE_EQ(eval_scalar("size(J, 1)"), 16.0);
    EXPECT_DOUBLE_EQ(eval_scalar("size(J, 2)"), 16.0);
}

// Smooth gradient: top-left corner saturates near 0 of remapped range,
// bottom-right saturates at 255. (Tile-corner pixels use a single-tile
// transform, so corners reach the extremes of the per-tile CDF.)
TEST_F(AdaptHistEqTest, GradientCornerSaturates)
{
    engine.eval("[X, Y] = meshgrid(linspace(0, 1, 64), linspace(0, 1, 64));");
    engine.eval("I = uint8(255 * sqrt(X.*Y));");
    engine.eval("J = adapthisteq(I);");
    EXPECT_DOUBLE_EQ(eval_scalar("double(J(end, end))"), 255.0);  // bottom-right max
    EXPECT_LT(eval_scalar("double(J(1, 1))"), 30.0);              // top-left near 0
}

// NumTiles must be >= 2.
TEST_F(AdaptHistEqTest, BadNumTilesThrows)
{
    engine.eval("I = uint8(reshape(0:255, 16, 16));");
    EXPECT_THROW(engine.eval("adapthisteq(I, 'NumTiles', [1 1]);"), std::exception);
}

// ClipLimit out of [0, 1] throws.
TEST_F(AdaptHistEqTest, BadClipLimitThrows)
{
    engine.eval("I = uint8(reshape(0:255, 16, 16));");
    EXPECT_THROW(engine.eval("adapthisteq(I, 'ClipLimit', 1.5);"), std::exception);
}

// Distribution other than 'uniform' deferred — must throw.
TEST_F(AdaptHistEqTest, NonUniformDistributionThrows)
{
    engine.eval("I = uint8(reshape(0:255, 16, 16));");
    EXPECT_THROW(engine.eval("adapthisteq(I, 'Distribution', 'rayleigh');"),
                 std::exception);
    EXPECT_THROW(engine.eval("adapthisteq(I, 'Distribution', 'exponential');"),
                 std::exception);
}

// Unknown name-value key throws.
TEST_F(AdaptHistEqTest, UnknownNVKeyThrows)
{
    engine.eval("I = uint8(reshape(0:255, 16, 16));");
    EXPECT_THROW(engine.eval("adapthisteq(I, 'BogusKey', 5);"), std::exception);
}

// Double-precision input also works and stays in [0, 1].
TEST_F(AdaptHistEqTest, DoubleInputStaysInUnitRange)
{
    engine.eval("[X, Y] = meshgrid(linspace(0, 1, 64), linspace(0, 1, 64));");
    engine.eval("I = sqrt(X.*Y);");
    engine.eval("J = adapthisteq(I);");
    EXPECT_GE(eval_scalar("min(J(:))"), 0.0);
    EXPECT_LE(eval_scalar("max(J(:))"), 1.0);
    EXPECT_EQ(eval_scalar("strcmp(class(J), 'double')"), 1.0);
}
