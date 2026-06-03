// libs/wavelet/tests/wkeep_public_api_test.cpp
//
// Direct C++ API guard for wkeep after the lift from adapter-only to the
// public Value-polymorphic entry numkit::wavelet::wkeep (1-D keep + 2-D
// sub-matrix extraction). Reference values match wkeep_test.cpp / MATLAB.

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

TEST(WkeepPublicApi, OneD)
{
    Engine e;
    Value x = wv(e, "1:10", "x");
    // centered (opt default Value::Empty) -> [4 5 6 7]
    Value c = wavelet::wkeep(x, wv(e, "4", "n4"));
    ASSERT_EQ(c.numel(), 4u);
    EXPECT_DOUBLE_EQ(c.doubleData()[0], 4.0);
    EXPECT_DOUBLE_EQ(c.doubleData()[3], 7.0);
    // 'l' -> first n
    Value l = wavelet::wkeep(x, wv(e, "4", "n4b"), wv(e, "'l'", "ol"), e.resource());
    EXPECT_DOUBLE_EQ(l.doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(l.doubleData()[3], 4.0);
    // 'r' -> last n
    Value r = wavelet::wkeep(x, wv(e, "4", "n4c"), wv(e, "'r'", "orr"), e.resource());
    EXPECT_DOUBLE_EQ(r.doubleData()[0], 7.0);
    EXPECT_DOUBLE_EQ(r.doubleData()[3], 10.0);
    // numeric 1-based start = 2 -> x(2:5)
    Value s = wavelet::wkeep(x, wv(e, "4", "n4d"), wv(e, "2", "st2"), e.resource());
    EXPECT_DOUBLE_EQ(s.doubleData()[0], 2.0);
    EXPECT_DOUBLE_EQ(s.doubleData()[3], 5.0);
    // out-of-range n throws
    EXPECT_ANY_THROW(wavelet::wkeep(x, wv(e, "99", "nbig")));
}

TEST(WkeepPublicApi, TwoD)
{
    Engine e;
    Value M = wv(e, "reshape(1:25,5,5)", "M");
    // top-left 3x3 (corner [1 1])
    Value tl = wavelet::wkeep(M, wv(e, "[3 3]", "sz"), wv(e, "[1 1]", "cn"),
                              e.resource());
    EXPECT_EQ(tl.dims().rows(), 3u);
    EXPECT_EQ(tl.dims().cols(), 3u);
    EXPECT_DOUBLE_EQ(tl.doubleData()[0], 1.0);  // (1,1)
    EXPECT_DOUBLE_EQ(tl.doubleData()[3], 6.0);  // (1,2)
    EXPECT_DOUBLE_EQ(tl.doubleData()[8], 13.0); // (3,3)
    // central 3x3 (opt default) -> rows/cols 2..4; (1,1) = M(2,2) = 7
    Value cen = wavelet::wkeep(M, wv(e, "[3 3]", "sz2"));
    EXPECT_DOUBLE_EQ(cen.doubleData()[0], 7.0);
}
