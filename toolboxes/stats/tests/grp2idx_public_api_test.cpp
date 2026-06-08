// toolboxes/stats/tests/grp2idx_public_api_test.cpp
//
// Direct C++ API guard for grp2idx after the lift from adapter-only to the
// public typed entry point numkit::stats::grp2idx (multi-output via the
// Grp2idxResult struct { G, GN, GL }).

#include <numkit/stats/descriptive/descriptive.hpp>
#include <numkit/core/engine.hpp>

#include <cmath>
#include <gtest/gtest.h>

using namespace numkit;

namespace {
Value gv(Engine &e, const char *expr, const char *name)
{
    e.eval(std::string(name) + " = " + expr + ";");
    return *e.getVariable(name);
}
} // namespace

TEST(Grp2idxPublicApi, Numeric)
{
    StandardEngine e;
    // numeric: sorted-ascending unique groups {1,2,3}; mr defaulted
    stats::Grp2idxResult r = stats::grp2idx(gv(e, "[3 1 1 3 2]", "s"));
    ASSERT_EQ(r.G.numel(), 5u);
    EXPECT_EQ(r.G.dims().cols(), 1u); // column vector
    EXPECT_DOUBLE_EQ(r.G.doubleData()[0], 3.0);
    EXPECT_DOUBLE_EQ(r.G.doubleData()[1], 1.0);
    EXPECT_DOUBLE_EQ(r.G.doubleData()[4], 2.0);
    EXPECT_EQ(r.GN.numel(), 3u);
    EXPECT_EQ(r.GL.numel(), 3u); // GL == GN here
    // NaN -> NaN index, excluded from the group set
    stats::Grp2idxResult rn = stats::grp2idx(gv(e, "[1 NaN 2]", "sn"), e.resource());
    EXPECT_DOUBLE_EQ(rn.G.doubleData()[0], 1.0);
    EXPECT_TRUE(std::isnan(rn.G.doubleData()[1]));
    EXPECT_EQ(rn.GN.numel(), 2u);
}

TEST(Grp2idxPublicApi, Cellstr)
{
    StandardEngine e;
    // cellstr: groups in first-appearance order {'b','a'}
    stats::Grp2idxResult r = stats::grp2idx(gv(e, "{'b','a','b'}", "c"), e.resource());
    ASSERT_EQ(r.G.numel(), 3u);
    EXPECT_DOUBLE_EQ(r.G.doubleData()[0], 1.0); // 'b' first
    EXPECT_DOUBLE_EQ(r.G.doubleData()[1], 2.0); // 'a' second
    EXPECT_DOUBLE_EQ(r.G.doubleData()[2], 1.0); // 'b' again
    EXPECT_EQ(r.GN.numel(), 2u);
}
