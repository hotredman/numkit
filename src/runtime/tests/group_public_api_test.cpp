// toolboxes/builtin/tests/group_public_api_test.cpp
//
// Direct C++ API guard for the group operations lifted from adapter-only
// to public typed entry points in math/group/group.hpp:
// numkit::builtin::findgroups (FindgroupsResult) and groupcounts
// (GroupcountsResult). (splitapply/groupsummary/grouptransform/groupfilter
// stay adapter-only — they need engine function-handle callbacks.)

#include <numkit/builtin/datafun.hpp>
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

TEST(GroupPublicApi, Findgroups)
{
    StandardEngine e;
    // sorted-unique groups {10,20,30}; G keeps the input shape (row)
    builtin::FindgroupsResult r = builtin::findgroups(gv(e, "[10 20 10 30]", "g"));
    ASSERT_EQ(r.G.numel(), 4u);
    EXPECT_EQ(r.G.dims().rows(), 1u);
    EXPECT_DOUBLE_EQ(r.G.doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(r.G.doubleData()[1], 2.0);
    EXPECT_DOUBLE_EQ(r.G.doubleData()[2], 1.0);
    EXPECT_DOUBLE_EQ(r.G.doubleData()[3], 3.0);
    ASSERT_EQ(r.ID.numel(), 3u);
    EXPECT_DOUBLE_EQ(r.ID.doubleData()[0], 10.0);
    EXPECT_DOUBLE_EQ(r.ID.doubleData()[2], 30.0);
    // NaN treated as missing -> G=NaN, excluded from ID
    builtin::FindgroupsResult rn =
        builtin::findgroups(gv(e, "[5 NaN 5]", "gn"), e.resource());
    EXPECT_DOUBLE_EQ(rn.G.doubleData()[0], 1.0);
    EXPECT_TRUE(std::isnan(rn.G.doubleData()[1]));
    EXPECT_EQ(rn.ID.numel(), 1u);
}

TEST(GroupPublicApi, Groupcounts)
{
    StandardEngine e;
    builtin::GroupcountsResult r =
        builtin::groupcounts(gv(e, "[10 20 10 30]", "g"), e.resource());
    ASSERT_EQ(r.C.numel(), 3u);
    EXPECT_DOUBLE_EQ(r.C.doubleData()[0], 2.0); // 10 appears twice
    EXPECT_DOUBLE_EQ(r.C.doubleData()[1], 1.0);
    EXPECT_DOUBLE_EQ(r.GR.doubleData()[0], 10.0);
    EXPECT_DOUBLE_EQ(r.P.doubleData()[0], 50.0); // 2/4 * 100
    // NaN forms a single trailing bucket
    builtin::GroupcountsResult rn =
        builtin::groupcounts(gv(e, "[1 1 2 NaN]", "gn"), e.resource());
    ASSERT_EQ(rn.C.numel(), 3u); // {1, 2} + NaN bucket
    EXPECT_DOUBLE_EQ(rn.C.doubleData()[0], 2.0); // count of 1
    EXPECT_DOUBLE_EQ(rn.C.doubleData()[2], 1.0); // NaN bucket
    EXPECT_TRUE(std::isnan(rn.GR.doubleData()[2]));
}

TEST(GroupPublicApi, Groupsummary)
{
    StandardEngine e;
    Value A = gv(e, "[1;2;3;4]", "A");
    Value G = gv(e, "[1;1;2;2]", "G");
    builtin::GroupsummaryResult r = builtin::groupsummary(A, G, "sum", e.resource());
    ASSERT_EQ(r.B.numel(), 2u);
    EXPECT_DOUBLE_EQ(r.B.doubleData()[0], 3.0); // 1+2
    EXPECT_DOUBLE_EQ(r.B.doubleData()[1], 7.0); // 3+4
    EXPECT_DOUBLE_EQ(r.BG.doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(r.BG.doubleData()[1], 2.0);
    EXPECT_DOUBLE_EQ(r.BC.doubleData()[0], 2.0); // count per group
    // other reductions
    EXPECT_DOUBLE_EQ(
        builtin::groupsummary(A, G, "mean", e.resource()).B.doubleData()[0], 1.5);
    EXPECT_DOUBLE_EQ(
        builtin::groupsummary(A, G, "max", e.resource()).B.doubleData()[1], 4.0);
    // unsupported method + shape mismatch throw
    EXPECT_ANY_THROW(builtin::groupsummary(A, G, "bogus", e.resource()));
    EXPECT_ANY_THROW(
        builtin::groupsummary(A, gv(e, "[1;1;2]", "Gb"), "sum", e.resource()));
}
