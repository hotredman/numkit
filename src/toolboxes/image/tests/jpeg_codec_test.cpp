// toolboxes/image/tests/jpeg_codec_test.cpp
//
// Tests for in-tree Baseline JPEG codec.

#include <gtest/gtest.h>
#include "../src/io/jpeg_codec.hpp"
#include <numkit/value/value.hpp>

#include <cmath>
#include <vector>

using namespace numkit;
using namespace numkit::image;

TEST(JpegCodecTest, Gray8RoundTripAndPeek) {
    const size_t H = 16, W = 16;
    Value orig = Value::matrix(H, W, ValueType::UINT8);
    for (size_t y = 0; y < H; ++y) {
        for (size_t x = 0; x < W; ++x) {
            orig.uint8DataMut()[x * H + y] = static_cast<uint8_t>((x * 16 + y * 8) % 256);
        }
    }

    std::string jpgBytes = writeJpegToBytes(orig, 95);
    EXPECT_GT(jpgBytes.size(), 100u);

    // Verify SOI and EOI markers
    EXPECT_EQ(static_cast<uint8_t>(jpgBytes[0]), 0xFF);
    EXPECT_EQ(static_cast<uint8_t>(jpgBytes[1]), 0xD8);
    EXPECT_EQ(static_cast<uint8_t>(jpgBytes[jpgBytes.size() - 2]), 0xFF);
    EXPECT_EQ(static_cast<uint8_t>(jpgBytes[jpgBytes.size() - 1]), 0xD9);

    uint32_t w32 = 0, h32 = 0;
    uint16_t bits = 0, channels = 0;
    EXPECT_TRUE(peekJpeg(reinterpret_cast<const uint8_t*>(jpgBytes.data()), jpgBytes.size(), w32, h32, bits, channels));
    EXPECT_EQ(w32, W);
    EXPECT_EQ(h32, H);
    EXPECT_EQ(bits, 8);
    EXPECT_EQ(channels, 1);

    Value readBack = readJpeg(reinterpret_cast<const uint8_t*>(jpgBytes.data()), jpgBytes.size());
    EXPECT_EQ(readBack.dims().rows(), H);
    EXPECT_EQ(readBack.dims().cols(), W);
    EXPECT_EQ(readBack.type(), ValueType::UINT8);
}

TEST(JpegCodecTest, Rgb8RoundTripAndQualityLevels) {
    const size_t H = 24, W = 32;
    Value rgb = Value::matrix3d(H, W, 3, ValueType::UINT8);
    for (size_t i = 0; i < H * W * 3; ++i) {
        rgb.uint8DataMut()[i] = static_cast<uint8_t>((i * 19) % 256);
    }

    // High quality (90) vs Low quality (30)
    std::string highQ = writeJpegToBytes(rgb, 90);
    std::string lowQ  = writeJpegToBytes(rgb, 30);
    EXPECT_GT(highQ.size(), lowQ.size()); // Lower quality results in smaller byte size

    uint32_t w32 = 0, h32 = 0;
    uint16_t bits = 0, channels = 0;
    EXPECT_TRUE(peekJpeg(reinterpret_cast<const uint8_t*>(highQ.data()), highQ.size(), w32, h32, bits, channels));
    EXPECT_EQ(w32, W);
    EXPECT_EQ(h32, H);
    EXPECT_EQ(channels, 3);
}
