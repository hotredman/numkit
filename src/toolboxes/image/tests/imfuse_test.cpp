// toolboxes/image/tests/imfuse_test.cpp
//
// Regression guard for imfuse — composite two images for visual
// comparison. Reference values from MATLAB R2025b, bit-exact (tol=0).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ImfuseTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {
        engine.eval(

            "A = uint8([10 20 30; 40 50 60; 70 80 90]);"
            "B = uint8([90 80 70; 60 50 40; 30 20 10]);");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── Default (falsecolor green-magenta) ─────────────────────────────

TEST_F(ImfuseTest, DefaultFalsecolorGreenMagenta)
{
    eval("C = imfuse(A, B);");
    // Channels = [2 1 2] = [B A B]
    EXPECT_EQ(static_cast<int>(evalScalar("double(C(1,1,1))")), 255);
    EXPECT_EQ(static_cast<int>(evalScalar("double(C(1,1,2))")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("double(C(1,1,3))")), 255);
    EXPECT_EQ(static_cast<int>(evalScalar("double(C(3,3,1))")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("double(C(3,3,2))")), 255);
    EXPECT_EQ(static_cast<int>(evalScalar("double(C(2,2,1))")), 128);
}

// ── blend method ───────────────────────────────────────────────────

TEST_F(ImfuseTest, Blend)
{
    eval("C = imfuse(A, B, 'blend');");
    EXPECT_EQ(static_cast<int>(evalScalar("double(C(1,1))")), 128);
    EXPECT_EQ(static_cast<int>(evalScalar("double(C(2,2))")), 128);
}

// ── diff method ────────────────────────────────────────────────────

TEST_F(ImfuseTest, Diff)
{
    eval("C = imfuse(A, B, 'diff');");
    EXPECT_EQ(static_cast<int>(evalScalar("double(C(1,1))")), 255);
    EXPECT_EQ(static_cast<int>(evalScalar("double(C(2,2))")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("double(C(3,3))")), 255);
}

// ── checkerboard method ────────────────────────────────────────────

TEST_F(ImfuseTest, Checkerboard)
{
    eval("C = imfuse(A, B, 'checkerboard');");
    EXPECT_EQ(static_cast<int>(evalScalar("double(C(1,1))")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("double(C(2,2))")), 128);
}

// ── montage method ─────────────────────────────────────────────────

TEST_F(ImfuseTest, Montage)
{
    eval("C = imfuse(A, B, 'montage');");
    EXPECT_EQ(static_cast<int>(evalScalar("size(C, 1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(C, 2)")), 6);
}

// ── Scaling 'none' ─────────────────────────────────────────────────

TEST_F(ImfuseTest, BlendScalingNone)
{
    eval("C = imfuse(A, B, 'blend', 'Scaling', 'none');");
    // No scaling: A(2,2)=50, B(2,2)=50, avg=50.
    EXPECT_EQ(static_cast<int>(evalScalar("double(C(2,2))")), 50);
}

// ── Scaling 'joint' ────────────────────────────────────────────────

TEST_F(ImfuseTest, BlendScalingJoint)
{
    eval("C = imfuse(A, B, 'blend', 'Scaling', 'joint');");
    EXPECT_EQ(static_cast<int>(evalScalar("double(C(2,2))")), 128);
}

// ── ColorChannels 'red-cyan' ───────────────────────────────────────

TEST_F(ImfuseTest, FalsecolorRedCyan)
{
    eval("C = imfuse(A, B, 'falsecolor', 'ColorChannels', 'red-cyan');");
    // [1 2 2] = [A B B]. A(2,2) scaled = 128, B(2,2) scaled = 128.
    EXPECT_EQ(static_cast<int>(evalScalar("double(C(2,2,1))")), 128);
    EXPECT_EQ(static_cast<int>(evalScalar("double(C(2,2,2))")), 128);
    EXPECT_EQ(static_cast<int>(evalScalar("double(C(2,2,3))")), 128);
}

// ── ColorChannels custom [1 2 0] ──────────────────────────────────

TEST_F(ImfuseTest, FalsecolorCustomChannels)
{
    eval("C = imfuse(A, B, 'falsecolor', 'ColorChannels', [1 2 0]);");
    EXPECT_EQ(static_cast<int>(evalScalar("double(C(2,2,1))")), 128);
    EXPECT_EQ(static_cast<int>(evalScalar("double(C(2,2,2))")), 128);
    EXPECT_EQ(static_cast<int>(evalScalar("double(C(2,2,3))")), 0);
}

// ── Different-size A and B → zero-pad to max ──────────────────────

TEST_F(ImfuseTest, DifferentSizes)
{
    eval("A2 = uint8(reshape(1:12, 3, 4));"
         "B2 = uint8(reshape(13:42, 5, 6));"
         "C = imfuse(A2, B2, 'blend');");
    EXPECT_EQ(static_cast<int>(evalScalar("size(C, 1)")), 5);
    EXPECT_EQ(static_cast<int>(evalScalar("size(C, 2)")), 6);
}

// ── Errors ─────────────────────────────────────────────────────────

TEST_F(ImfuseTest, UnknownMethodThrows)
{
    EXPECT_THROW(eval("imfuse(A, B, 'foobar');"), std::exception);
}

TEST_F(ImfuseTest, UnknownScalingThrows)
{
    EXPECT_THROW(eval("imfuse(A, B, 'blend', 'Scaling', 'wat');"),
                 std::exception);
}
