// libs/image/tests/bwtraceboundary_test.cpp
//
// Regression guard for bwtraceboundary — Moore-Neighbor boundary
// tracing. Bit-exact MATLAB R2025b (tol = 0).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class BwtraceboundaryTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override {
        engine.eval(
            "import compat.*;"
            "BW = false(7, 7);"
            "BW(2:6, 2:6) = true;"
            "LL = false(8, 8);"
            "LL(2:5, 2:3) = true;"
            "LL(2:3, 4:6) = true;"
            "S1 = false(5, 5);"
            "S1(3, 3) = true;");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── Square E clockwise — 17 pts ────────────────────────────────────

TEST_F(BwtraceboundaryTest, SquareEastClockwise)
{
    eval("B = bwtraceboundary(BW, [2 2], 'E');");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B, 1)")), 17);
    // First two: (2,2), (2,3).
    EXPECT_EQ(static_cast<int>(evalScalar("B(1,1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("B(1,2)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("B(2,1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("B(2,2)")), 3);
    // Last: closes back to (2,2).
    EXPECT_EQ(static_cast<int>(evalScalar("B(end,1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("B(end,2)")), 2);
}

// ── Limit N=5 → 5 pts ──────────────────────────────────────────────

TEST_F(BwtraceboundaryTest, LimitN5)
{
    eval("B = bwtraceboundary(BW, [2 2], 'E', 8, 5);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B, 1)")), 5);
}

// ── conn = 4 → 17 pts (same square perimeter) ────────────────────

TEST_F(BwtraceboundaryTest, FourConnSquare)
{
    eval("B = bwtraceboundary(BW, [2 2], 'E', 4);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B, 1)")), 17);
}

// ── fstep = 'S' from (2,2) still goes east first (boundary) ──────

TEST_F(BwtraceboundaryTest, StartSouth)
{
    eval("B = bwtraceboundary(BW, [2 2], 'S');");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B, 1)")), 17);
    EXPECT_EQ(static_cast<int>(evalScalar("B(2,1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("B(2,2)")), 3);
}

// ── L-shape (concave) — 14 pts ────────────────────────────────────

TEST_F(BwtraceboundaryTest, LShape)
{
    eval("B = bwtraceboundary(LL, [2 2], 'E');");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B, 1)")), 14);
    // Diagonal inner corner: (3,4) → (4,3).
    EXPECT_EQ(static_cast<int>(evalScalar("B(9,1)")), 4);
    EXPECT_EQ(static_cast<int>(evalScalar("B(9,2)")), 3);
}

// ── Counterclockwise direction ────────────────────────────────────

TEST_F(BwtraceboundaryTest, Counterclockwise)
{
    eval("B = bwtraceboundary(BW, [2 2], 'E', 8, Inf, 'counterclockwise');");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B, 1)")), 17);
}

// ── Single pixel → [P; P] ──────────────────────────────────────────

TEST_F(BwtraceboundaryTest, SinglePixel)
{
    eval("B = bwtraceboundary(S1, [3 3], 'E');");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B, 1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("B(1,1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("B(2,1)")), 3);
}

// ── Errors ─────────────────────────────────────────────────────────

TEST_F(BwtraceboundaryTest, BadConnThrows)
{
    EXPECT_THROW(eval("bwtraceboundary(BW, [2 2], 'E', 5);"),
                 std::exception);
}

TEST_F(BwtraceboundaryTest, BadFstepThrows)
{
    EXPECT_THROW(eval("bwtraceboundary(BW, [2 2], 'XYZ');"),
                 std::exception);
}

TEST_F(BwtraceboundaryTest, BackgroundStartThrows)
{
    EXPECT_THROW(eval("bwtraceboundary(BW, [1 1], 'E');"),
                 std::exception);
}
