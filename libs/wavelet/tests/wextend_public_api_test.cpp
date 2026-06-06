// libs/wavelet/tests/wextend_public_api_test.cpp
//
// Direct C++ API guard for wextend after the lift from adapter-only to the
// public entry numkit::wavelet::wextend (1-D / 2-D boundary extension).
// Reference values match wextend_test.cpp / MATLAB R2025b.

#include <numkit/wavelet/dwt/wkeep_wextend.hpp>
#include <numkit/core/engine.hpp>

#include <gtest/gtest.h>

using namespace numkit;

namespace {
Value wv(Engine &e, const char *expr, const char *name)
{
    e.eval(std::string(name) + " = " + expr + ";");
    return *e.getVariable(name);
}
} // namespace

TEST(WextendPublicApi, OneD)
{
    StandardEngine e;
    Value x = wv(e, "[1 2 3 4 5]", "x");
    Value t1 = wv(e, "1", "t1");
    // 'sym' both, lf=2 -> [2 1 | 1 2 3 4 5 | 5 4]  (side "b", mr defaulted)
    Value ys = wavelet::wextend(t1, "sym", x, 2);
    ASSERT_EQ(ys.numel(), 9u);
    EXPECT_DOUBLE_EQ(ys.doubleData()[0], 2.0);
    EXPECT_DOUBLE_EQ(ys.doubleData()[1], 1.0);
    EXPECT_DOUBLE_EQ(ys.doubleData()[2], 1.0); // original starts here
    EXPECT_DOUBLE_EQ(ys.doubleData()[8], 4.0);
    // 'zpd' -> [0 0 | 1 2 3 4 5 | 0 0]
    Value yz = wavelet::wextend(t1, "zpd", x, 2, "b", e.resource());
    EXPECT_DOUBLE_EQ(yz.doubleData()[0], 0.0);
    EXPECT_DOUBLE_EQ(yz.doubleData()[2], 1.0);
    EXPECT_DOUBLE_EQ(yz.doubleData()[8], 0.0);
    // side 'l' -> left-pad only: [0 0 1 2 3 4 5] (numel 7)
    Value yl = wavelet::wextend(t1, "zpd", x, 2, "l", e.resource());
    ASSERT_EQ(yl.numel(), 7u);
    EXPECT_DOUBLE_EQ(yl.doubleData()[0], 0.0);
    EXPECT_DOUBLE_EQ(yl.doubleData()[6], 5.0);
    // bad mode / bad type throw
    EXPECT_ANY_THROW(wavelet::wextend(t1, "bogus", x, 2));
    EXPECT_ANY_THROW(wavelet::wextend(wv(e, "3", "t3"), "sym", x, 2));
}

TEST(WextendPublicApi, TwoD)
{
    StandardEngine e;
    Value M = wv(e, "[1 2; 3 4]", "M");
    // type 2, zpd, lf=1 both axes -> 4x4
    Value y2 = wavelet::wextend(wv(e, "2", "t2"), "zpd", M, 1, "b", e.resource());
    EXPECT_EQ(y2.dims().rows(), 4u);
    EXPECT_EQ(y2.dims().cols(), 4u);
    // 'ac' = add columns only -> rows unchanged (2), cols 2+2=4
    Value yac =
        wavelet::wextend(wv(e, "'ac'", "tac"), "zpd", M, 1, "b", e.resource());
    EXPECT_EQ(yac.dims().rows(), 2u);
    EXPECT_EQ(yac.dims().cols(), 4u);
}
