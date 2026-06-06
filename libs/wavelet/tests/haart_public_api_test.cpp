// libs/wavelet/tests/haart_public_api_test.cpp
//
// Direct C++ API guard for haart / ihaart after the lift from adapter-only
// to numkit::wavelet::haart (HaartResult {a, d}) and ihaart (Value). The
// forward/inverse pair is exercised via round-trip reconstruction.

#include <numkit/wavelet/dwt/haart.hpp>
#include <numkit/wavelet/dwt/ihaart.hpp>
#include <numkit/core/engine.hpp>

#include <cmath>
#include <gtest/gtest.h>

using namespace numkit;

namespace {
Value hv(Engine &e, const char *expr, const char *name)
{
    e.eval(std::string(name) + " = " + expr + ";");
    return *e.getVariable(name);
}
} // namespace

TEST(HaartPublicApi, Level1)
{
    StandardEngine e;
    Value x = hv(e, "[1 2 3 4 5 6 7 8]", "x");
    wavelet::HaartResult r = wavelet::haart(x, 1); // level 1, mr defaulted
    ASSERT_EQ(r.a.numel(), 4u);
    EXPECT_NEAR(r.a.doubleData()[0], (1.0 + 2.0) / std::sqrt(2.0), 1e-12);
    EXPECT_FALSE(r.d.isCell()); // level 1 -> plain matrix
    ASSERT_EQ(r.d.numel(), 4u);
    EXPECT_NEAR(r.d.doubleData()[0], (2.0 - 1.0) / std::sqrt(2.0), 1e-12);
    // round-trip: ihaart(a, d) recovers x
    Value xr = wavelet::ihaart(r.a, r.d, 0, false, e.resource());
    ASSERT_EQ(xr.numel(), 8u);
    for (size_t i = 0; i < 8; ++i)
        EXPECT_NEAR(xr.doubleData()[i], x.doubleData()[i], 1e-12);
}

TEST(HaartPublicApi, MultiLevelAndInteger)
{
    StandardEngine e;
    Value x = hv(e, "[1 2 3 4 5 6 7 8]", "x");
    // auto level (= 3 for length 8): final approximation is a scalar,
    // detail is a 3-element cell (d{1} finest).
    wavelet::HaartResult r = wavelet::haart(x);
    EXPECT_EQ(r.a.numel(), 1u);
    EXPECT_TRUE(r.d.isCell());
    EXPECT_EQ(r.d.numel(), 3u);
    Value xr = wavelet::ihaart(r.a, r.d, 0, false, e.resource());
    ASSERT_EQ(xr.numel(), 8u);
    for (size_t i = 0; i < 8; ++i)
        EXPECT_NEAR(xr.doubleData()[i], x.doubleData()[i], 1e-12);
    // integer lifting round-trips exactly too
    wavelet::HaartResult ri = wavelet::haart(x, 0, true, e.resource());
    Value xri = wavelet::ihaart(ri.a, ri.d, 0, true, e.resource());
    for (size_t i = 0; i < 8; ++i)
        EXPECT_NEAR(xri.doubleData()[i], x.doubleData()[i], 1e-9);
    // errors: odd-length haart; out-of-range ihaart level
    EXPECT_ANY_THROW(wavelet::haart(hv(e, "[1 2 3]", "xo")));
    EXPECT_ANY_THROW(wavelet::ihaart(r.a, r.d, 99, false, e.resource()));
}
