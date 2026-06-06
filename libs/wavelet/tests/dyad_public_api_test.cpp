// libs/wavelet/tests/dyad_public_api_test.cpp
//
// Direct C++ API guard for libs/wavelet/src/dwt/dyad.cpp after the lift
// from adapter-only to numkit::wavelet::{dyaddown, dyadup, wmaxlev}.
// Reference values match the script-level dyaddown/dyadup/wmaxlev tests
// (MATLAB R2025b).

#include <numkit/wavelet/dwt/dyad.hpp>
#include <numkit/core/engine.hpp>

#include <gtest/gtest.h>

using namespace numkit;

namespace {
Value dvar(Engine &e, const char *expr, const char *name)
{
    e.eval(std::string(name) + " = " + expr + ";");
    return *e.getVariable(name);
}
} // namespace

TEST(DyadPublicApi, Dyaddown)
{
    StdEngine e;
    Value x = dvar(e, "[10 20 30 40 50 60 70]", "x");
    // defaults: odd = 0 (keep even-indexed), type = 'c', mr defaulted
    Value y = wavelet::dyaddown(x);
    ASSERT_EQ(y.numel(), 3u);
    EXPECT_DOUBLE_EQ(y.doubleData()[0], 20.0);
    EXPECT_DOUBLE_EQ(y.doubleData()[2], 60.0);
    // odd = 1 keeps odd-indexed -> [10 30 50 70]
    Value yo = wavelet::dyaddown(x, 1, 'c', e.resource());
    ASSERT_EQ(yo.numel(), 4u);
    EXPECT_DOUBLE_EQ(yo.doubleData()[0], 10.0);
    EXPECT_DOUBLE_EQ(yo.doubleData()[3], 70.0);
    // matrix, type 'c' keeps even columns of [1 2 3 4; 5 6 7 8] -> cols 2,4
    Value m = dvar(e, "[1 2 3 4; 5 6 7 8]", "m");
    Value ym = wavelet::dyaddown(m, 0, 'c', e.resource());
    EXPECT_EQ(ym.dims().rows(), 2u);
    EXPECT_EQ(ym.dims().cols(), 2u);
    EXPECT_DOUBLE_EQ(ym.doubleData()[0], 2.0); // (1,1) = col 2, row 1
}

TEST(DyadPublicApi, Dyadup)
{
    StdEngine e;
    Value x = dvar(e, "[1 2 3]", "x");
    // default odd = 1 -> [0 1 0 2 0 3 0]
    Value y = wavelet::dyadup(x);
    ASSERT_EQ(y.numel(), 7u);
    EXPECT_DOUBLE_EQ(y.doubleData()[0], 0.0);
    EXPECT_DOUBLE_EQ(y.doubleData()[1], 1.0);
    EXPECT_DOUBLE_EQ(y.doubleData()[6], 0.0);
    // odd = 0 -> [1 0 2 0 3]
    Value y0 = wavelet::dyadup(x, 0, 'c', e.resource());
    ASSERT_EQ(y0.numel(), 5u);
    EXPECT_DOUBLE_EQ(y0.doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(y0.doubleData()[1], 0.0);
    EXPECT_DOUBLE_EQ(y0.doubleData()[4], 3.0);
}

TEST(DyadPublicApi, Wmaxlev)
{
    StdEngine e;
    EXPECT_DOUBLE_EQ(wavelet::wmaxlev(dvar(e, "64", "n64"), "db2").toScalar(), 4.0);
    EXPECT_DOUBLE_EQ(
        wavelet::wmaxlev(dvar(e, "[8 8]", "n88"), "db1", e.resource()).toScalar(),
        3.0);
    EXPECT_DOUBLE_EQ(wavelet::wmaxlev(dvar(e, "2", "n2"), "db1").toScalar(), 1.0);
    EXPECT_DOUBLE_EQ(wavelet::wmaxlev(dvar(e, "1", "n1"), "db1").toScalar(), 0.0);
    // empty N throws
    EXPECT_ANY_THROW(wavelet::wmaxlev(dvar(e, "[]", "ne"), "db1"));
}
