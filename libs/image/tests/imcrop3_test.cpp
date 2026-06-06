// libs/image/tests/imcrop3_test.cpp
//
// Regression guard for imcrop3 — 3-D / 4-D volume cropping.
// Reference values from MATLAB R2025b, bit-equal on all probed cases.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class Imcrop3Test : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {
        engine.eval("import compat.*;"
                    "V = reshape(1:60, 3, 4, 5);"
                    "V4 = uint8(reshape(1:96, 4, 4, 3, 2));");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── Shape ───────────────────────────────────────────────────────────

TEST_F(Imcrop3Test, BasicShape3D)
{
    eval("W = imcrop3(V, [1 1 1 2 1 2]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(W,1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(W,2)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(W,3)")), 3);
}

TEST_F(Imcrop3Test, InteriorCropShape)
{
    eval("W = imcrop3(V, [2 2 1 2 1 2]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(W,1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(W,2)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(W,3)")), 3);
}

// ── Values ──────────────────────────────────────────────────────────

TEST_F(Imcrop3Test, BasicValues)
{
    eval("W = imcrop3(V, [1 1 1 2 1 2]);");
    EXPECT_DOUBLE_EQ(evalScalar("W(1,1,1)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("W(1,2,1)"),  4.0);
    EXPECT_DOUBLE_EQ(evalScalar("W(1,3,1)"),  7.0);
    EXPECT_DOUBLE_EQ(evalScalar("W(2,1,1)"),  2.0);
    EXPECT_DOUBLE_EQ(evalScalar("W(2,3,1)"),  8.0);
    EXPECT_DOUBLE_EQ(evalScalar("W(1,1,2)"), 13.0);
    EXPECT_DOUBLE_EQ(evalScalar("W(2,3,3)"), 32.0);
}

TEST_F(Imcrop3Test, RoundingNonInteger)
{
    eval("W = imcrop3(V, [1.6 1.6 1.6 1.4 1.4 1.4]);");
    // After rounding all limits become [2, 3] → 2×2×2 block.
    EXPECT_EQ(static_cast<int>(evalScalar("size(W,1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(W,2)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(W,3)")), 2);
    // V(2,2,2) = (page=2 start at 13, col 2 → +3, row 2 → +1)
    //          = 13 + 3 + 1 = 17.
    EXPECT_DOUBLE_EQ(evalScalar("W(1,1,1)"), 17.0);
}

// ── 4-D pass-through ────────────────────────────────────────────────

TEST_F(Imcrop3Test, FourDClassPreservedPassThrough)
{
    eval("W = imcrop3(V4, [1 1 1 2 2 0]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(W,1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(W,2)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(W,3)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(W,4)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("W(1,1,1,1)")),  1);
    EXPECT_EQ(static_cast<int>(evalScalar("W(3,3,1,1)")), 11);
    // 4th dim pass-through: page 1 of slice 2 starts at 49 (= 4*4*3 + 1).
    EXPECT_EQ(static_cast<int>(evalScalar("W(1,1,1,2)")), 49);
}

// ── Validation ───────────────────────────────────────────────────────

TEST_F(Imcrop3Test, OutOfBoundsThrows)
{
    EXPECT_THROW(eval("imcrop3(V, [10 10 10 1 1 1]);"), std::exception);
    EXPECT_THROW(eval("imcrop3(V, [-1 1 1 1 1 1]);"),  std::exception);
    EXPECT_THROW(eval("imcrop3(V, [1 1 1 100 1 1]);"), std::exception);
}

TEST_F(Imcrop3Test, BadCuboidLengthThrows)
{
    EXPECT_THROW(eval("imcrop3(V, [1 2 3 4 5]);"),    std::exception);
    EXPECT_THROW(eval("imcrop3(V, [1 2 3 4 5 6 7]);"), std::exception);
}

TEST_F(Imcrop3Test, LowDimVolumeThrows)
{
    EXPECT_THROW(eval("imcrop3(zeros(3,3), [1 1 1 1 1 0]);"), std::exception);
}

TEST_F(Imcrop3Test, ClassPreservedDoubleAndUint8)
{
    EXPECT_EQ(eval("imcrop3(V, [1 1 1 1 1 1])").type(),  ValueType::DOUBLE);
    EXPECT_EQ(eval("imcrop3(V4, [1 1 1 1 1 0])").type(), ValueType::UINT8);
}
