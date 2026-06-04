// libs/stats/tests/lillietest_public_api_test.cpp
//
// Direct C++ API guard for lillietest after the lift from adapter-only to
// the public typed entry point numkit::stats::lillietest (multi-output
// std::tuple<Value,Value,Value,Value> = (h, p, kstat, critval)). Reference
// inputs/decisions match the lillietest.json parity spec (MATLAB R2025b).

#include <numkit/stats/test/hypothesis.hpp>
#include <numkit/core/engine.hpp>

#include <tuple>
#include <gtest/gtest.h>

using namespace numkit;

namespace {
Value lv(Engine &e, const char *expr, const char *name)
{
    e.eval(std::string(name) + " = " + expr + ";");
    return *e.getVariable(name);
}
} // namespace

TEST(LillietestPublicApi, Basic)
{
    Engine e;
    // normal-ish sample -> fail to reject (h = 0); default alpha, mr
    auto [h1, p1, ks1, cv1] = stats::lillietest(lv(
        e, "[0.1 0.5 -0.3 1.2 -0.7 0.4 -0.1 0.8 -0.4 0.3 0.6 -0.2 0.7 -0.5 0.1]",
        "xn"));
    EXPECT_DOUBLE_EQ(h1.toScalar(), 0.0);
    EXPECT_GT(ks1.toScalar(), 0.0);
    EXPECT_GT(p1.toScalar(), 0.0);
    EXPECT_LE(p1.toScalar(), 0.5);
    EXPECT_GT(cv1.toScalar(), 0.0);
    // clearly bimodal sample -> reject (h = 1); explicit alpha + mr
    auto [h2, p2, ks2, cv2] = stats::lillietest(
        lv(e, "[-3 -3 -3 -3 -3 -3 -3 -3 -3 -3 3 3 3 3 3 3 3 3 3 3]", "xb"), 0.05,
        e.resource());
    EXPECT_DOUBLE_EQ(h2.toScalar(), 1.0);
    EXPECT_GT(ks2.toScalar(), ks1.toScalar()); // bimodal deviates more
    // errors: N < 4, alpha out of range
    EXPECT_ANY_THROW(stats::lillietest(lv(e, "[1 2 3]", "xs")));
    EXPECT_ANY_THROW(
        stats::lillietest(lv(e, "[1 2 3 4 5]", "xa"), 1.5, e.resource()));
}
