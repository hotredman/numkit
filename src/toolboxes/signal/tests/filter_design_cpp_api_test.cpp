// toolboxes/signal/tests/filter_design_cpp_api_test.cpp
//
// Pilot of the new uniform public API: numkit::signal::* called
// directly from C++ without engine.eval. Covers:
//   * Value ergonomic ctors (init_list / vector / array / span)
//   * Value::view non-owning factory
//   * butter / fir1 / fir2 / firls / firpm with mr-last sig
//
// These tests are NOT redundant with the engine-driven regression
// tests in filter_design_test.cpp / firls_test.cpp / firpm_test.cpp.
// They specifically exercise the C++ entry points the way an external
// library user would, ensuring the public surface compiles cleanly and
// produces correct results without any interpreter involvement.

#include <numkit/value/value.hpp>
#include <numkit/signal/filter_design/filter_design.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <vector>

using namespace numkit;

// ── Value ergonomic ctors ──────────────────────────────────────────

TEST(ValueErgonomicCtors, InitListDouble)
{
    Value v = {1.0, 2.0, 3.0, 4.0};
    ASSERT_EQ(v.numel(), 4u);
    ASSERT_EQ(v.type(), ValueType::DOUBLE);
    EXPECT_DOUBLE_EQ(v.doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(v.doubleData()[3], 4.0);
    // Row vector — 1×4.
    EXPECT_EQ(v.dims().rows(), 1u);
    EXPECT_EQ(v.dims().cols(), 4u);
}

TEST(ValueErgonomicCtors, InitListIntPromotion)
{
    // Integer literals promote to double (init_list<double> ctor).
    Value v = {1, 2, 3};
    ASSERT_EQ(v.type(), ValueType::DOUBLE);
    EXPECT_DOUBLE_EQ(v.doubleData()[1], 2.0);
}

TEST(ValueErgonomicCtors, FromVectorDouble)
{
    std::vector<double> src = {0.5, 1.5, 2.5, 3.5, 4.5};
    Value v(src);
    ASSERT_EQ(v.numel(), src.size());
    ASSERT_EQ(v.type(), ValueType::DOUBLE);
    for (size_t i = 0; i < src.size(); ++i)
        EXPECT_DOUBLE_EQ(v.doubleData()[i], src[i]);
}

TEST(ValueErgonomicCtors, FromVectorFloat)
{
    std::vector<float> src = {1.0f, 2.0f, 3.0f};
    Value v(src);
    ASSERT_EQ(v.type(), ValueType::SINGLE);
    EXPECT_FLOAT_EQ(v.singleData()[0], 1.0f);
    EXPECT_FLOAT_EQ(v.singleData()[2], 3.0f);
}

TEST(ValueErgonomicCtors, FromVectorUint8)
{
    std::vector<uint8_t> src = {10, 20, 30, 40};
    Value v(src);
    ASSERT_EQ(v.type(), ValueType::UINT8);
    EXPECT_EQ(v.uint8Data()[1], 20);
    EXPECT_EQ(v.uint8Data()[3], 40);
}

TEST(ValueErgonomicCtors, FromArray)
{
    std::array<double, 3> src = {7.0, 8.0, 9.0};
    Value v(src);
    ASSERT_EQ(v.numel(), 3u);
    EXPECT_DOUBLE_EQ(v.doubleData()[0], 7.0);
    EXPECT_DOUBLE_EQ(v.doubleData()[2], 9.0);
}

TEST(ValueErgonomicCtors, FromSpan)
{
    double buf[] = {2.0, 4.0, 6.0, 8.0};
    Span<const double> s(buf, 4);
    Value v(s);
    ASSERT_EQ(v.numel(), 4u);
    EXPECT_DOUBLE_EQ(v.doubleData()[2], 6.0);
}

TEST(ValueErgonomicCtors, ViewIsNonOwning)
{
    double buf[5] = {1.0, 2.0, 3.0, 4.0, 5.0};
    Value v = Value::view(buf, ValueType::DOUBLE, Dims{1, 5});
    ASSERT_EQ(v.numel(), 5u);
    EXPECT_DOUBLE_EQ(v.doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(v.doubleData()[4], 5.0);

    // Mutating the source visible through the view (no copy was made).
    buf[2] = 99.0;
    EXPECT_DOUBLE_EQ(v.doubleData()[2], 99.0);
}

TEST(ValueErgonomicCtors, ViewCopyOnWriteOnMutation)
{
    double buf[3] = {1.0, 2.0, 3.0};
    Value v = Value::view(buf, ValueType::DOUBLE, Dims{1, 3});
    // doubleDataMut triggers detach → fresh owning buffer.
    double *m = v.doubleDataMut();
    m[0] = 100.0;
    // Source untouched after COW.
    EXPECT_DOUBLE_EQ(buf[0], 1.0);
    EXPECT_DOUBLE_EQ(v.doubleData()[0], 100.0);
}

TEST(ValueErgonomicCtors, EmptyInitList)
{
    Value v = std::initializer_list<double>{};
    EXPECT_EQ(v.numel(), 0u);
}

// ── Filter design — new public C++ API ─────────────────────────────

TEST(FilterDesignCppApi, ButterLowpass)
{
    auto [b, a] = signal::butter(4, 0.3);
    ASSERT_EQ(b.numel(), 5u);
    ASSERT_EQ(a.numel(), 5u);
    // a(0) is always 1 for a digital IIR.
    EXPECT_NEAR(a.doubleData()[0], 1.0, 1e-12);
    // DC gain must be 1 for LP.
    double bSum = 0, aSum = 0;
    for (size_t i = 0; i < 5; ++i) { bSum += b.doubleData()[i]; aSum += a.doubleData()[i]; }
    EXPECT_NEAR(bSum / aSum, 1.0, 1e-9);
}

TEST(FilterDesignCppApi, ButterHighpass)
{
    auto [b, a] = signal::butter(4, 0.3, "high");
    ASSERT_EQ(b.numel(), 5u);
    // Nyquist gain must be 1 for HP: sum(b * (-1)^k) / sum(a * (-1)^k) == 1.
    double bSum = 0, aSum = 0;
    for (size_t i = 0; i < 5; ++i) {
        const double sign = (i % 2 == 0) ? 1.0 : -1.0;
        bSum += sign * b.doubleData()[i];
        aSum += sign * a.doubleData()[i];
    }
    EXPECT_NEAR(bSum / aSum, 1.0, 1e-9);
}

TEST(FilterDesignCppApi, Fir1Lowpass)
{
    Value b = signal::fir1(32, 0.25, "low");
    ASSERT_EQ(b.numel(), 33u);
    // Hamming-windowed sinc is symmetric.
    for (size_t i = 0; i < 16; ++i)
        EXPECT_NEAR(b.doubleData()[i], b.doubleData()[32 - i], 1e-12);
    // DC gain is normalised to 1.
    double sum = 0;
    for (size_t i = 0; i < 33; ++i) sum += b.doubleData()[i];
    EXPECT_NEAR(sum, 1.0, 1e-9);
}

TEST(FilterDesignCppApi, FirlsFromInitList)
{
    // Ergonomic literal syntax — F and A go in directly as braces.
    Value b = signal::firls(20, {0.0, 0.4, 0.5, 1.0}, {1.0, 1.0, 0.0, 0.0});
    ASSERT_EQ(b.numel(), 21u);
    // Symmetric (Type-I).
    for (size_t i = 0; i < 10; ++i)
        EXPECT_NEAR(b.doubleData()[i], b.doubleData()[20 - i], 1e-12);
}

TEST(FilterDesignCppApi, FirlsFromVector)
{
    std::vector<double> F = {0.0, 0.4, 0.5, 1.0};
    std::vector<double> A = {1.0, 1.0, 0.0, 0.0};
    Value b = signal::firls(20, F, A);
    EXPECT_EQ(b.numel(), 21u);
}

TEST(FilterDesignCppApi, Fir2FromInitList)
{
    Value b = signal::fir2(50, {0.0, 0.3, 0.5, 0.7, 1.0},
                                {1.0, 1.0, 0.0, 0.0, 0.0});
    EXPECT_EQ(b.numel(), 51u);
}

TEST(FilterDesignCppApi, FirpmLowpass)
{
    // Ergonomic literal syntax for all array args.
    auto [h, err] = signal::firpm(30, {0.0, 0.4, 0.5, 1.0},
                                       {1.0, 1.0, 0.0, 0.0});
    ASSERT_EQ(h.numel(), 31u);
    EXPECT_GT(err, 0.0);
    // Equiripple Type-I is symmetric.
    for (size_t i = 0; i < 15; ++i)
        EXPECT_NEAR(h.doubleData()[i], h.doubleData()[30 - i], 1e-10);
}

TEST(FilterDesignCppApi, FirpmWeighted)
{
    // Weighted design redistributes ripple between bands. Check that
    // passing W produces a measurably different impulse response than
    // unweighted — the W argument is wired through correctly.
    auto [h1, e1] = signal::firpm(30, {0.0, 0.4, 0.5, 1.0},
                                       {1.0, 1.0, 0.0, 0.0});
    auto [h2, e2] = signal::firpm(30, {0.0, 0.4, 0.5, 1.0},
                                       {1.0, 1.0, 0.0, 0.0},
                                       {1.0, 10.0});  // stopband 10× weight
    double maxDelta = 0.0;
    for (size_t i = 0; i < 31; ++i)
        maxDelta = std::max(maxDelta,
                            std::abs(h1.doubleData()[i] - h2.doubleData()[i]));
    EXPECT_GT(maxDelta, 1e-6);
}

TEST(FilterDesignCppApi, FirpmHilbert)
{
    auto [h, err] = signal::firpm(30, {0.05, 0.95}, {1.0, 1.0},
                                   {}, "hilbert");
    ASSERT_EQ(h.numel(), 31u);
    // Type-III is anti-symmetric and h[N/2] == 0.
    EXPECT_NEAR(h.doubleData()[15], 0.0, 1e-10);
    for (size_t i = 0; i < 15; ++i)
        EXPECT_NEAR(h.doubleData()[i], -h.doubleData()[30 - i], 1e-10);
}

TEST(FilterDesignCppApi, FirpmWithExternalVector)
{
    std::vector<double> F = {0.0, 0.3, 0.4, 1.0};
    std::vector<double> A = {1.0, 1.0, 0.0, 0.0};
    auto [h, err] = signal::firpm(40, F, A);
    EXPECT_EQ(h.numel(), 41u);
    EXPECT_GT(err, 0.0);
}
