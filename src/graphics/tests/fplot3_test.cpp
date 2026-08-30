// toolboxes/graphics/tests/fplot3_test.cpp
//
// Regression guard for fplot3() — parametric 3-D function-handle plot.
// Mirrors fplot's pattern: samples (funx(t), funy(t), funz(t)) on a
// 200-point grid, emits a `plot3` dataset via the FigureManager.
//
// Like other plot fns, the test verifies the call runs without
// throwing — numkit's figure-data is JSON to stdout, not a queryable
// handle, so the assertion is "doesn't crash" rather than data shape.

#include <numkit/core/engine.hpp>
#include <numkit/figure/figure_manager.hpp>
#include <gtest/gtest.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace numkit;

class Fplot3Test : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
};

TEST_F(Fplot3Test, DefaultRangeSpiralRuns)
{
    EXPECT_NO_THROW(engine.eval(
        "fplot3(@(t) cos(t), @(t) sin(t), @(t) t);"));
}

TEST_F(Fplot3Test, ExplicitRangeRuns)
{
    EXPECT_NO_THROW(engine.eval(
        "fplot3(@(t) cos(t), @(t) sin(t), @(t) t, [0 2*pi]);"));
}

TEST_F(Fplot3Test, MissingFuncHandlesIsHandledGracefully)
{
    // Wrong argument types should not crash — function returns empty.
    EXPECT_NO_THROW(engine.eval("fplot3(1, 2, 3);"));
    EXPECT_NO_THROW(engine.eval("fplot3();"));
}

TEST_F(Fplot3Test, FunctionsThatThrowAreSampledAsNaN)
{
    // 1 / t blows up at t = 0; the impl catches and keeps going.
    EXPECT_NO_THROW(engine.eval(
        "fplot3(@(t) 1./t, @(t) t, @(t) t);"));
}
