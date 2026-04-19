// tests/filter_test.cpp

#include "MEngine.hpp"
#include "MStdLibrary.hpp"
#include <cmath>
#include <gtest/gtest.h>

using namespace numkit::m;

class FilterTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { StdLibrary::install(engine); }
    MValue eval(const std::string &code) { return engine.eval(code); }
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
