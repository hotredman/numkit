// tests/filter_test.cpp

#include <numkit/core/engine.hpp>
#include <numkit/builtin/library.hpp>
#include <cmath>
#include <gtest/gtest.h>

using namespace numkit;

class FilterTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &code) { return engine.eval(code); }
    double evalScalar(const std::string &code) { return eval(code).toScalar(); }
};

// ============================================================
// filter
// ============================================================

TEST_F(FilterTest, FilterFirMovingAverage)
{
    // 3-tap moving average: b = [1/3 1/3 1/3], a = [1]
    eval("b = [1/3 1/3 1/3]; a = [1];");
    eval("x = [0 0 0 3 3 3 3 3];");
    eval("y = filter(b, a, x);");
    // y(4) = (0+0+3)/3 = 1, y(5) = (0+3+3)/3 = 2, y(6) = (3+3+3)/3 = 3
    EXPECT_NEAR(evalScalar("y(4)"), 1.0, 1e-10);
    EXPECT_NEAR(evalScalar("y(5)"), 2.0, 1e-10);
    EXPECT_NEAR(evalScalar("y(6)"), 3.0, 1e-10);
}

TEST_F(FilterTest, FilterIdentity)
{
    // b = [1], a = [1] → pass-through
    eval("x = [1 2 3 4 5];");
    eval("y = filter([1], [1], x);");
    for (int i = 1; i <= 5; ++i) {
        std::string expr = "y(" + std::to_string(i) + ")";
        EXPECT_NEAR(evalScalar(expr), static_cast<double>(i), 1e-10);
    }
}

TEST_F(FilterTest, FilterGain)
{
    // b = [2], a = [1] → multiply by 2
    eval("y = filter([2], [1], [1 2 3 4]);");
    EXPECT_NEAR(evalScalar("y(1)"), 2.0, 1e-10);
    EXPECT_NEAR(evalScalar("y(4)"), 8.0, 1e-10);
}

TEST_F(FilterTest, FilterIirFirstOrder)
{
    // First-order IIR: y[n] = x[n] + 0.5*y[n-1]
    // b = [1], a = [1 -0.5]
    eval("y = filter([1], [1 -0.5], [1 0 0 0 0]);");
    EXPECT_NEAR(evalScalar("y(1)"), 1.0, 1e-10);
    EXPECT_NEAR(evalScalar("y(2)"), 0.5, 1e-10);
    EXPECT_NEAR(evalScalar("y(3)"), 0.25, 1e-10);
    EXPECT_NEAR(evalScalar("y(4)"), 0.125, 1e-10);
}

TEST_F(FilterTest, FilterNormalizesA0)
{
    // a = [2 -1] should behave like a = [1 -0.5]
    eval("y = filter([1], [2 -1], [1 0 0 0]);");
    EXPECT_NEAR(evalScalar("y(1)"), 0.5, 1e-10);
    EXPECT_NEAR(evalScalar("y(2)"), 0.25, 1e-10);
}

TEST_F(FilterTest, FilterOutputLength)
{
    eval("y = filter([1 1], [1], [1 2 3 4 5]);");
    EXPECT_EQ(eval("y").numel(), 5u);
}

// ============================================================
// filtfilt
// ============================================================

TEST_F(FilterTest, FiltfiltZeroPhase)
{
    // filtfilt should not introduce phase shift
    // Apply a simple FIR to a delayed impulse, check peak stays put
    eval("x = zeros(1, 64); x(32) = 1;");
    eval("b = [0.25 0.5 0.25]; a = [1];");
    eval("y = filtfilt(b, a, x);");
    // Peak should be at or very near index 32
    eval("[~, idx] = max(y);");
    EXPECT_NEAR(evalScalar("idx"), 32.0, 1.0);
}

TEST_F(FilterTest, FiltfiltIdentity)
{
    // With b=[1], a=[1], output = input
    eval("x = [1 2 3 4 5 6 7 8];");
    eval("y = filtfilt([1], [1], x);");
    for (int i = 1; i <= 8; ++i) {
        std::string expr = "y(" + std::to_string(i) + ")";
        EXPECT_NEAR(evalScalar(expr), static_cast<double>(i), 1e-10);
    }
}

TEST_F(FilterTest, FiltfiltSmooths)
{
    // filtfilt with averaging filter reduces variance
    eval("x = [1 10 1 10 1 10 1 10 1 10 1 10 1 10 1 10];");
    eval("b = [0.25 0.5 0.25]; a = [1];");
    eval("y = filtfilt(b, a, x);");
    // Output variance should be less than input variance
    double inputVar = evalScalar("sum((x - mean(x)).^2)");
    double outputVar = evalScalar("sum((y - mean(y)).^2)");
    EXPECT_LT(outputVar, inputVar);
}

// filter final-state output [y,zf] and initial-conditions input
// filter(b,a,x,zi). Both were unsupported (zf undefined; zi ignored).
// vs MATLAB R2025b.
TEST_F(FilterTest, FilterFinalStateAndInitialConditions)
{
    eval("[y, zf] = filter([1 1], [1 -0.5], [1 2 3 4]);");
    EXPECT_NEAR(evalScalar("y(4)"),  10.375, 1e-12);
    EXPECT_NEAR(evalScalar("zf"),     9.1875, 1e-12);   // final DF2T state
    // zi seeds the state: y(1) = b(1)*x(1) + zi(1) = 1 + 10 = 11.
    eval("yi = filter([1 1], [1 -0.5], [1 2 3 4], 10);");
    EXPECT_NEAR(evalScalar("yi(1)"), 11.0, 1e-12);
    // zf length = max(na,nb)-1 = 2 for a 3-tap FIR.
    eval("[y2, zf2] = filter([1 0.5 0.25], 1, [1 2 3 4 5]);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(zf2)")), 2);
    EXPECT_NEAR(evalScalar("zf2(1)"), 3.5,  1e-12);
    EXPECT_NEAR(evalScalar("zf2(2)"), 1.25, 1e-12);
}

// ============================================================
// filter on a matrix: each column (first non-singleton dim) is filtered
// independently — the delay state resets between columns. Regression for
// the old bug where the column-major buffer was filtered as one signal,
// leaking state across the column boundary (col2(1) came out 7, not 2).
// ============================================================

TEST_F(FilterTest, FilterMatrixFirPerColumn)
{
    eval("M = [1 2; 3 4; 5 6];");
    eval("Y = filter([1 1], 1, M);");
    // col1 [1;3;5] -> [1;4;8]; col2 [2;4;6] -> [2;6;10]
    EXPECT_NEAR(evalScalar("Y(1,1)"), 1.0,  1e-12);
    EXPECT_NEAR(evalScalar("Y(2,1)"), 4.0,  1e-12);
    EXPECT_NEAR(evalScalar("Y(3,1)"), 8.0,  1e-12);
    EXPECT_NEAR(evalScalar("Y(1,2)"), 2.0,  1e-12);   // was 7 before the fix
    EXPECT_NEAR(evalScalar("Y(2,2)"), 6.0,  1e-12);
    EXPECT_NEAR(evalScalar("Y(3,2)"), 10.0, 1e-12);
    // shape preserved
    EXPECT_EQ(static_cast<int>(evalScalar("size(Y,1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(Y,2)")), 2);
}

TEST_F(FilterTest, FilterMatrixIirPerColumn)
{
    eval("M = [1 2; 3 4; 5 6];");
    eval("Y = filter(1, [1 -0.5], M);");
    // col1 [1;3;5] -> [1;3.5;6.75]; col2 [2;4;6] -> [2;5;8.5]
    EXPECT_NEAR(evalScalar("Y(3,1)"), 6.75, 1e-12);
    EXPECT_NEAR(evalScalar("Y(3,2)"), 8.5,  1e-12);
}

TEST_F(FilterTest, FilterMatrixZfPerColumn)
{
    // [y, zf] on a matrix: zf is (nfilt-1) x ncols, one final state per column.
    eval("M = [1 2; 3 4; 5 6];");
    eval("[Y, zf] = filter([1 0.5], 1, M);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(zf,1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(zf,2)")), 2);
    EXPECT_NEAR(evalScalar("zf(1)"), 2.5, 1e-12);   // 0.5 * M(3,1) = 0.5*5
    EXPECT_NEAR(evalScalar("zf(2)"), 3.0, 1e-12);   // 0.5 * M(3,2) = 0.5*6
}

TEST_F(FilterTest, FilterColumnVectorUnchanged)
{
    // A column / row vector is a single signal — unchanged by the fix.
    eval("yc = filter([1 1], 1, [1; 3; 5]);");
    EXPECT_NEAR(evalScalar("yc(2)"), 4.0, 1e-12);
    EXPECT_NEAR(evalScalar("yc(3)"), 8.0, 1e-12);
    eval("yr = filter([1 1], 1, [1 3 5]);");
    EXPECT_NEAR(evalScalar("yr(3)"), 8.0, 1e-12);
}
