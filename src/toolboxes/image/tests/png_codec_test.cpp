// toolboxes/image/tests/png_codec_test.cpp
//
// Tests for in-tree PNG codec (8-bit, 16-bit, Gray, RGB, RGBA, filters).

#include <gtest/gtest.h>
#include "../src/io/png_codec.hpp"
#include <numkit/value/value.hpp>

#include <random>
#include <vector>

using namespace numkit;
using namespace numkit::image;

TEST(PngCodecTest, Gray8RoundTrip) {
    const size_t H = 25, W = 33;
    Value orig = Value::matrix(H, W, ValueType::UINT8);
    for (size_t y = 0; y < H; ++y) {
        for (size_t x = 0; x < W; ++x) {
            orig.uint8DataMut()[x * H + y] = static_cast<uint8_t>((x * 13 + y * 17) % 256);
        }
    }

    std::string pngBytes = writePngToBytes(orig, 6);
    EXPECT_GT(pngBytes.size(), 8u + 25u + 12u + 12u);

    uint32_t w32 = 0, h32 = 0;
    uint16_t bits = 0, channels = 0;
    EXPECT_TRUE(peekPng(reinterpret_cast<const uint8_t*>(pngBytes.data()), pngBytes.size(), w32, h32, bits, channels));
    EXPECT_EQ(w32, W);
    EXPECT_EQ(h32, H);
    EXPECT_EQ(bits, 8);
    EXPECT_EQ(channels, 1);

    Value readBack = readPng(reinterpret_cast<const uint8_t*>(pngBytes.data()), pngBytes.size());
    EXPECT_EQ(readBack.dims().rows(), H);
    EXPECT_EQ(readBack.dims().cols(), W);
    EXPECT_EQ(readBack.type(), ValueType::UINT8);

    for (size_t i = 0; i < H * W; ++i) {
        EXPECT_EQ(readBack.uint8Data()[i], orig.uint8Data()[i]);
    }
}

TEST(PngCodecTest, Rgb8AndRgba8RoundTrip) {
    const size_t H = 20, W = 20;

    // RGB 3 channels
    Value rgb = Value::matrix3d(H, W, 3, ValueType::UINT8);
    for (size_t i = 0; i < H * W * 3; ++i) {
        rgb.uint8DataMut()[i] = static_cast<uint8_t>((i * 31) % 256);
    }
    std::string rgbBytes = writePngToBytes(rgb, 6);
    Value rgbBack = readPng(reinterpret_cast<const uint8_t*>(rgbBytes.data()), rgbBytes.size());
    EXPECT_EQ(rgbBack.dims().pages(), 3u);
    EXPECT_EQ(rgbBack.type(), ValueType::UINT8);
    for (size_t i = 0; i < H * W * 3; ++i) {
        EXPECT_EQ(rgbBack.uint8Data()[i], rgb.uint8Data()[i]);
    }

    // RGBA 4 channels
    Value rgba = Value::matrix3d(H, W, 4, ValueType::UINT8);
    for (size_t i = 0; i < H * W * 4; ++i) {
        rgba.uint8DataMut()[i] = static_cast<uint8_t>((i * 47) % 256);
    }
    std::string rgbaBytes = writePngToBytes(rgba, 6);
    Value rgbaBack = readPng(reinterpret_cast<const uint8_t*>(rgbaBytes.data()), rgbaBytes.size());
    EXPECT_EQ(rgbaBack.dims().pages(), 4u);
    EXPECT_EQ(rgbaBack.type(), ValueType::UINT8);
    for (size_t i = 0; i < H * W * 4; ++i) {
        EXPECT_EQ(rgbaBack.uint8Data()[i], rgba.uint8Data()[i]);
    }
}

TEST(PngCodecTest, Gray16AndRgb16PrecisionRoundTrip) {
    const size_t H = 18, W = 22;

    // 16-bit Grayscale
    Value gray16 = Value::matrix(H, W, ValueType::UINT16);
    for (size_t i = 0; i < H * W; ++i) {
        gray16.uint16DataMut()[i] = static_cast<uint16_t>(i * 123);
    }
    std::string gray16Bytes = writePngToBytes(gray16, 6);
    Value gray16Back = readPng(reinterpret_cast<const uint8_t*>(gray16Bytes.data()), gray16Bytes.size());
    EXPECT_EQ(gray16Back.type(), ValueType::UINT16);
    EXPECT_EQ(gray16Back.dims().rows(), H);
    EXPECT_EQ(gray16Back.dims().cols(), W);
    for (size_t i = 0; i < H * W; ++i) {
        EXPECT_EQ(gray16Back.uint16Data()[i], gray16.uint16Data()[i]);
    }

    // 16-bit RGB
    Value rgb16 = Value::matrix3d(H, W, 3, ValueType::UINT16);
    for (size_t i = 0; i < H * W * 3; ++i) {
        rgb16.uint16DataMut()[i] = static_cast<uint16_t>(i * 57 + 1000);
    }
    std::string rgb16Bytes = writePngToBytes(rgb16, 6);
    Value rgb16Back = readPng(reinterpret_cast<const uint8_t*>(rgb16Bytes.data()), rgb16Bytes.size());
    EXPECT_EQ(rgb16Back.type(), ValueType::UINT16);
    EXPECT_EQ(rgb16Back.dims().pages(), 3u);
    for (size_t i = 0; i < H * W * 3; ++i) {
        EXPECT_EQ(rgb16Back.uint16Data()[i], rgb16.uint16Data()[i]);
    }
}
