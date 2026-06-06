// libs/wavelet/tests/wextend_test.cpp
//
// Backfill gtest for libs/wavelet/src/dwt/wkeep_wextend.cpp::wextend.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class WextendTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override
    {
        engine.eval("import compat.*;");
        engine.eval("x = [1 2 3 4 5];");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// MATLAB R2025b reference (lf=2):
//   sym → [2 1 1 2 3 4 5 5 4]
//   per → [5 5 1 2 3 4 5 5 1 2]   (odd N gets x(end) appended first)
//   zpd → [0 0 1 2 3 4 5 0 0]
//   ppd → [4 5 1 2 3 4 5 1 2]

TEST_F(WextendTest, SymWholePoint)
{
    eval("y = wextend(1, 'sym', x, 2);");
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(y)")), 9u);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 2);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 1);
    EXPECT_DOUBLE_EQ(evalScalar("y(8)"), 5);
    EXPECT_DOUBLE_EQ(evalScalar("y(9)"), 4);
}

TEST_F(WextendTest, PeriodicOddN)
{
    eval("y = wextend(1, 'per', x, 2);");
    // Odd N → MATLAB pre-pads x with x(end), then wraps.
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(y)")), 10u);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 5);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 5);
    EXPECT_DOUBLE_EQ(evalScalar("y(9)"), 1);
    EXPECT_DOUBLE_EQ(evalScalar("y(10)"), 2);
}

TEST_F(WextendTest, ZeroPad)
{
    eval("y = wextend(1, 'zpd', x, 2);");
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(y)")), 9u);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 0);
    EXPECT_DOUBLE_EQ(evalScalar("y(8)"), 0);
    EXPECT_DOUBLE_EQ(evalScalar("y(9)"), 0);
}

TEST_F(WextendTest, PurePeriodic)
{
    eval("y = wextend(1, 'ppd', x, 2);");
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(y)")), 9u);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 4);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 5);
    EXPECT_DOUBLE_EQ(evalScalar("y(8)"), 1);
    EXPECT_DOUBLE_EQ(evalScalar("y(9)"), 2);
}

TEST_F(WextendTest, SingleSideLeft)
{
    eval("y = wextend(1, 'sym', x, 2, 'l');");
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(y)")), 7u);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 2);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 1);
}

TEST_F(WextendTest, SingleSideRight)
{
    eval("y = wextend(1, 'sym', x, 2, 'r');");
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(y)")), 7u);
    EXPECT_DOUBLE_EQ(evalScalar("y(6)"), 5);
    EXPECT_DOUBLE_EQ(evalScalar("y(7)"), 4);
}

// Bug fix 2026-05-08 — added 'symw', 'asym', 'asymw', 'sp0', 'sp1' modes
// and the 2-D forms (type=2 / 'ar' / 'ac').

TEST_F(WextendTest, ModeSymw)
{
    // Whole-point symmetric: edge sample NOT replicated.
    // [3 2 | 1 2 3 4 5 | 4 3]
    eval("y = wextend(1, 'symw', [1 2 3 4 5], 2);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 3);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 2);
    EXPECT_DOUBLE_EQ(evalScalar("y(8)"), 4);
    EXPECT_DOUBLE_EQ(evalScalar("y(9)"), 3);
}

TEST_F(WextendTest, ModeAsym)
{
    // [-2 -1 | 1 2 3 4 5 | -5 -4]
    eval("y = wextend(1, 'asym', [1 2 3 4 5], 2);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), -2);
    EXPECT_DOUBLE_EQ(evalScalar("y(8)"), -5);
}

TEST_F(WextendTest, ModeAsymw)
{
    // Antisymmetric whole-point: pre = 2·x(1) - x(j) reversed.
    // [-1 0 | 1 2 3 4 5 | 6 7]
    eval("y = wextend(1, 'asymw', [1 2 3 4 5], 2);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), -1);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"),  0);
    EXPECT_DOUBLE_EQ(evalScalar("y(8)"),  6);
    EXPECT_DOUBLE_EQ(evalScalar("y(9)"),  7);
}

TEST_F(WextendTest, ModeSp0)
{
    eval("y = wextend(1, 'sp0', [1 2 3 4 5], 2);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 1);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 1);
    EXPECT_DOUBLE_EQ(evalScalar("y(8)"), 5);
    EXPECT_DOUBLE_EQ(evalScalar("y(9)"), 5);
}

TEST_F(WextendTest, ModeSp1)
{
    // Linear extrapolation (slope x(2) - x(1)).
    eval("y = wextend(1, 'sp1', [1 2 3 4 5], 2);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), -1);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"),  0);
    EXPECT_DOUBLE_EQ(evalScalar("y(8)"),  6);
    EXPECT_DOUBLE_EQ(evalScalar("y(9)"),  7);
}

TEST_F(WextendTest, Type2BothAxes)
{
    eval("y = wextend(2, 'zpd', [1 2; 3 4], 1);");
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(y, 1)")), 4u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(y, 2)")), 4u);
    EXPECT_DOUBLE_EQ(evalScalar("y(1,1)"), 0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2,2)"), 1);
    EXPECT_DOUBLE_EQ(evalScalar("y(3,3)"), 4);
}

TEST_F(WextendTest, TypeArAddsRows)
{
    // 'ar' = "along row direction" → ADDS ROWS (counter-intuitive MATLAB
    // naming but matches `help wextend`).
    eval("y = wextend('ar', 'zpd', [1 2; 3 4], 1);");
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(y, 1)")), 4u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(y, 2)")), 2u);
    EXPECT_DOUBLE_EQ(evalScalar("y(1,1)"), 0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2,1)"), 1);
    EXPECT_DOUBLE_EQ(evalScalar("y(4,2)"), 0);
}

TEST_F(WextendTest, TypeAcAddsCols)
{
    eval("y = wextend('ac', 'zpd', [1 2; 3 4], 1);");
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(y, 1)")), 2u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(y, 2)")), 4u);
    EXPECT_DOUBLE_EQ(evalScalar("y(1,1)"), 0);
    EXPECT_DOUBLE_EQ(evalScalar("y(1,2)"), 1);
    EXPECT_DOUBLE_EQ(evalScalar("y(2,4)"), 0);
}
