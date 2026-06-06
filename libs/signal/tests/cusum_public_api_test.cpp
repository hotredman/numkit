// libs/signal/tests/cusum_public_api_test.cpp
//
// Direct C++ API guard for cusum after the lift from adapter-only to the
// public typed entry point numkit::signal::cusum (multi-output via the
// CusumResult struct; optional tmean/tdev as Value::Empty = auto).

#include <numkit/signal/measurements/sig_utils.hpp>
#include <numkit/core/engine.hpp>

#include <gtest/gtest.h>

using namespace numkit;

namespace {
Value cv(Engine &e, const char *expr, const char *name)
{
    e.eval(std::string(name) + " = " + expr + ";");
    return *e.getVariable(name);
}
} // namespace

TEST(CusumPublicApi, StepDetection)
{
    StdEngine e;
    // explicit tmean=0, tdev=1, climit=2, mshift=1 -> z = x, half_shift = 0.5.
    // Upper sum first exceeds 2 at sample 4 (value 5 - 0.5 = 4.5).
    Value x = cv(e, "[0 0 0 5 5 5 5 5]", "x");
    signal::CusumResult r =
        signal::cusum(x, 2.0, 1.0, cv(e, "0", "tm"), cv(e, "1", "td"), e.resource());
    ASSERT_EQ(r.iupper.numel(), 1u);
    EXPECT_DOUBLE_EQ(r.iupper.toScalar(), 4.0);
    EXPECT_EQ(r.ilower.numel(), 0u); // no downward detection
    ASSERT_EQ(r.uppersum.numel(), 8u);
    EXPECT_DOUBLE_EQ(r.uppersum.doubleData()[0], 0.0);
    EXPECT_DOUBLE_EQ(r.uppersum.doubleData()[3], 4.5);
}

TEST(CusumPublicApi, DefaultsConstantSignal)
{
    StdEngine e;
    // all defaults (climit=5, mshift=1, tmean/tdev auto), mr default.
    // A constant signal has zero deviation -> no detection.
    Value x = cv(e, "ones(1,30)", "x");
    signal::CusumResult r = signal::cusum(x);
    EXPECT_EQ(r.iupper.numel(), 0u);
    EXPECT_EQ(r.ilower.numel(), 0u);
    EXPECT_EQ(r.uppersum.numel(), 30u);
}
