// libs/image/tests/adapthisteq_cpp_api_test.cpp
//
// Pilot of the new uniform public API for libs/image. Calls
// numkit::image::adapthisteq directly from C++ without engine.eval,
// using the AdaptHistEqOptions struct + the mr-last convention.

#include <numkit/value/value.hpp>
#include <numkit/image/contrast/contrast.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

using namespace numkit;

namespace {
// Build a 16×16 uint8 ramp: I(r,c) = (r*16 + c). MATLAB column-major
// addressing matches: linear index k = r + c*16, value = r + c*16.
Value makeRamp16(std::pmr::memory_resource *mr = nullptr)
{
    Value I = Value::matrix(16, 16, ValueType::UINT8, mr);
    uint8_t *d = I.uint8DataMut();
    for (size_t c = 0; c < 16; ++c)
        for (size_t r = 0; r < 16; ++r)
            d[r + c * 16] = static_cast<uint8_t>(r + c * 16);
    return I;
}
} // namespace

TEST(AdaptHistEqCppApi, DefaultsPreserveClassAndSize)
{
    Value I = makeRamp16();
    Value J = image::adapthisteq(I);
    EXPECT_EQ(J.type(), ValueType::UINT8);
    EXPECT_EQ(J.dims().rows(), 16u);
    EXPECT_EQ(J.dims().cols(), 16u);
}

TEST(AdaptHistEqCppApi, OptionsStructFieldByField)
{
    Value I = makeRamp16();
    image::AdaptHistEqOptions opts;
    opts.numTilesR = 4;
    opts.numTilesC = 4;
    opts.clipLimit = 0.03;
    Value J = image::adapthisteq(I, opts);
    EXPECT_EQ(J.dims().rows(), 16u);
    EXPECT_EQ(J.dims().cols(), 16u);
}

TEST(AdaptHistEqCppApi, ClipLimitDifference)
{
    // Build a 32×32 squared-ramp — histogram is concentrated at the low
    // end, so clipLimit changes redistribution. A flat ramp gives an
    // already-flat histogram on which CLAHE is a no-op.
    Value I = Value::matrix(32, 32, ValueType::UINT8);
    uint8_t *d = I.uint8DataMut();
    for (size_t c = 0; c < 32; ++c)
        for (size_t r = 0; r < 32; ++r) {
            double x = (r + c) / 62.0;        // ∈ [0, 1]
            d[r + c * 32] = static_cast<uint8_t>(x * x * 255.0);
        }
    image::AdaptHistEqOptions oLow;  oLow.clipLimit  = 0.001;
    image::AdaptHistEqOptions oHigh; oHigh.clipLimit = 0.05;
    Value J1 = image::adapthisteq(I, oLow);
    Value J2 = image::adapthisteq(I, oHigh);
    bool differs = false;
    for (size_t i = 0; i < 32 * 32; ++i)
        if (J1.uint8Data()[i] != J2.uint8Data()[i]) { differs = true; break; }
    EXPECT_TRUE(differs);
}
