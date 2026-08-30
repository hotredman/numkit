// toolboxes/image/tests/houghlines_test.cpp
//
// Regression guard for houghlines — line-segment extraction from
// Hough-transform peaks. Bit-exact MATLAB R2025b (tol = 0).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class HoughlinesTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {
        engine.eval(

            "BW = false(11, 11);"
            "BW(6, 1:11) = true;"
            "BW(1:11, 6) = true;"
            "[H, T, R] = hough(BW);"
            "P = houghpeaks(H, 2);");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── Custom FillGap + MinLength → 2 segments ────────────────────────

TEST_F(HoughlinesTest, TwoSegmentsCustomGap)
{
    eval("L = houghlines(BW, T, R, P, 'FillGap', 5, 'MinLength', 3);");
    EXPECT_EQ(static_cast<int>(evalScalar("length(L)")), 2);
}

// ── Endpoints of first segment ────────────────────────────────────

TEST_F(HoughlinesTest, FirstSegmentEndpoints)
{
    eval("L = houghlines(BW, T, R, P, 'FillGap', 5, 'MinLength', 3);");
    EXPECT_EQ(static_cast<int>(evalScalar("L(1).point1(1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("L(1).point1(2)")), 6);
    EXPECT_EQ(static_cast<int>(evalScalar("L(1).point2(1)")), 11);
    EXPECT_EQ(static_cast<int>(evalScalar("L(1).point2(2)")), 6);
    EXPECT_NEAR(evalScalar("L(1).theta"), -90.0, 1e-12);
}

// ── Default fillgap=20, minlength=40 too long → 0 segments ────────

TEST_F(HoughlinesTest, DefaultArgsZeroLines)
{
    eval("L = houghlines(BW, T, R, P);");
    EXPECT_EQ(static_cast<int>(evalScalar("length(L)")), 0);
}

// ── Single peak (top-1) gives single segment ──────────────────────

TEST_F(HoughlinesTest, SinglePeakOneLine)
{
    eval("P1 = houghpeaks(H, 1);"
         "L = houghlines(BW, T, R, P1, 'MinLength', 3);");
    EXPECT_EQ(static_cast<int>(evalScalar("length(L)")), 1);
}

// ── theta / rho match the peak's bin ──────────────────────────────

TEST_F(HoughlinesTest, ThetaRhoFields)
{
    eval("L = houghlines(BW, T, R, P, 'FillGap', 5, 'MinLength', 3);");
    EXPECT_NEAR(evalScalar("L(2).theta"), -2.0, 1e-12);
    EXPECT_NEAR(evalScalar("L(2).rho"),   5.0, 1e-12);
}

// ── Errors ─────────────────────────────────────────────────────────

TEST_F(HoughlinesTest, BadFillGapThrows)
{
    EXPECT_THROW(eval("houghlines(BW, T, R, P, 'FillGap', -1);"),
                 std::exception);
}

TEST_F(HoughlinesTest, BadMinLengthThrows)
{
    EXPECT_THROW(eval("houghlines(BW, T, R, P, 'MinLength', 0);"),
                 std::exception);
}

TEST_F(HoughlinesTest, BadPeaksShapeThrows)
{
    EXPECT_THROW(eval("houghlines(BW, T, R, [1 2 3]);"),
                 std::exception);
}
