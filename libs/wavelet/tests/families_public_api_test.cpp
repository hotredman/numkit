// libs/wavelet/tests/families_public_api_test.cpp
//
// Direct C++ API guard for libs/wavelet/src/filter/families.cpp after the
// lift from adapter-only to the typed entry points
// numkit::wavelet::{dbwavf, coifwavf, symwavf, orthfilt}. Calls the public
// functions directly (not via the engine); reference values match the
// script-level *_test.cpp guards (MATLAB R2025b).

#include <numkit/wavelet/filter/families.hpp>
#include <numkit/core/engine.hpp>

#include <cmath>
#include <gtest/gtest.h>

using namespace numkit;

namespace {
double sumOf(const Value &v)
{
    double s = 0.0;
    for (size_t i = 0; i < v.numel(); ++i) s += v.doubleData()[i];
    return s;
}
} // namespace

TEST(WaveletFamiliesPublicApi, Dbwavf)
{
    StandardEngine e;
    Value h = wavelet::dbwavf("db2"); // mr defaulted (process default)
    ASSERT_EQ(h.numel(), 4u);
    EXPECT_NEAR(h.doubleData()[0], 0.341506, 1e-5);
    EXPECT_NEAR(h.doubleData()[3], -0.091506, 1e-5);
    EXPECT_NEAR(sumOf(h), 1.0, 1e-12);
    // 'haar' aliases 'db1'
    Value a = wavelet::dbwavf("haar", e.resource());
    Value b = wavelet::dbwavf("db1", e.resource());
    ASSERT_EQ(a.numel(), b.numel());
    for (size_t i = 0; i < a.numel(); ++i)
        EXPECT_DOUBLE_EQ(a.doubleData()[i], b.doubleData()[i]);
    EXPECT_ANY_THROW(wavelet::dbwavf("sym4")); // wrong family rejected
}

TEST(WaveletFamiliesPublicApi, CoifwavfSymwavf)
{
    StandardEngine e;
    Value c = wavelet::coifwavf("coif1", e.resource());
    ASSERT_EQ(c.numel(), 6u); // length 6K, K = 1
    EXPECT_NEAR(c.doubleData()[0], -0.051430, 1e-5);
    EXPECT_NEAR(c.doubleData()[2], 0.602859, 1e-5);
    EXPECT_NEAR(sumOf(c), 1.0, 1e-12);
    EXPECT_ANY_THROW(wavelet::coifwavf("db2"));

    Value s = wavelet::symwavf("sym4", e.resource());
    ASSERT_EQ(s.numel(), 8u); // length 2N, N = 4
    EXPECT_NEAR(s.doubleData()[0], 0.022785, 1e-5);
    EXPECT_NEAR(s.doubleData()[7], -0.053574, 1e-5);
    EXPECT_NEAR(sumOf(s), 1.0, 1e-12);
    EXPECT_ANY_THROW(wavelet::symwavf("coif1"));
}

TEST(WaveletFamiliesPublicApi, Orthfilt)
{
    StandardEngine e;
    wavelet::OrthfiltResult r =
        wavelet::orthfilt(wavelet::dbwavf("db2"), e.resource());
    ASSERT_EQ(r.Lo_D.numel(), 4u);
    ASSERT_EQ(r.Hi_D.numel(), 4u);
    ASSERT_EQ(r.Lo_R.numel(), 4u);
    ASSERT_EQ(r.Hi_R.numel(), 4u);
    // High-precision filter values (match orthfilt_test.cpp).
    EXPECT_NEAR(r.Lo_D.doubleData()[0], -0.1294095225512603, 1e-12);
    EXPECT_NEAR(r.Lo_D.doubleData()[3], 0.4829629131445341, 1e-12);
    EXPECT_NEAR(r.Lo_R.doubleData()[0], 0.482963, 1e-5);
    EXPECT_NEAR(r.Hi_D.doubleData()[0], -0.482963, 1e-5);
    EXPECT_NEAR(r.Hi_R.doubleData()[0], -0.129410, 1e-5);
    // Lo_D == reverse(Lo_R), Hi_D == reverse(Hi_R).
    for (size_t k = 0; k < 4; ++k) {
        EXPECT_DOUBLE_EQ(r.Lo_D.doubleData()[k], r.Lo_R.doubleData()[3 - k]);
        EXPECT_DOUBLE_EQ(r.Hi_D.doubleData()[k], r.Hi_R.doubleData()[3 - k]);
    }
    // Empty scaling filter rejected.
    EXPECT_ANY_THROW(wavelet::orthfilt(Value::matrix(0, 0, ValueType::DOUBLE)));
}
