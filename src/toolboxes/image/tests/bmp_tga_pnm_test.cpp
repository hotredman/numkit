// toolboxes/image/tests/bmp_tga_pnm_test.cpp
//
// Tests for in-tree BMP, TGA, and PNM (PPM/PGM/PBM) codecs.

#include <gtest/gtest.h>
#include "../src/io/bmp_codec.hpp"
#include "../src/io/tga_codec.hpp"
#include "../src/io/pnm_codec.hpp"
#include <numkit/value/value.hpp>

#include <random>
#include <vector>

using namespace numkit;
using namespace numkit::image;

TEST(BmpCodecTest, Gray8RoundTrip) {
    const size_t H = 17, W = 23;
    Value orig = Value::matrix(H, W, ValueType::UINT8);
    for (size_t y = 0; y < H; ++y) {
        for (size_t x = 0; x < W; ++x) {
            orig.uint8DataMut()[x * H + y] = static_cast<uint8_t>((x * 11 + y * 7) % 256);
        }
    }

    std::string bytes = writeBmpToBytes(orig);
    EXPECT_GT(bytes.size(), 14u + 40u + 1024u);

    uint32_t w32 = 0, h32 = 0;
    uint16_t bits = 0, channels = 0;
    EXPECT_TRUE(peekBmp(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(), w32, h32, bits, channels));
    EXPECT_EQ(w32, W);
    EXPECT_EQ(h32, H);

    Value readBack = readBmp(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
    EXPECT_EQ(readBack.dims().rows(), H);
    EXPECT_EQ(readBack.dims().cols(), W);
    EXPECT_EQ(readBack.type(), ValueType::UINT8);

    for (size_t i = 0; i < H * W; ++i) {
        EXPECT_EQ(readBack.uint8Data()[i], orig.uint8Data()[i]);
    }
}

TEST(BmpCodecTest, Rgb8AndRgba8RoundTrip) {
    const size_t H = 15, W = 19;
    // 3-channel RGB
    Value rgb = Value::matrix3d(H, W, 3, ValueType::UINT8);
    for (size_t i = 0; i < H * W * 3; ++i) {
        rgb.uint8DataMut()[i] = static_cast<uint8_t>((i * 17) % 256);
    }
    std::string rgbBytes = writeBmpToBytes(rgb);
    Value rgbBack = readBmp(reinterpret_cast<const uint8_t*>(rgbBytes.data()), rgbBytes.size());
    EXPECT_EQ(rgbBack.dims().pages(), 3u);
    for (size_t i = 0; i < H * W * 3; ++i) {
        EXPECT_EQ(rgbBack.uint8Data()[i], rgb.uint8Data()[i]);
    }

    // 4-channel RGBA
    Value rgba = Value::matrix3d(H, W, 4, ValueType::UINT8);
    for (size_t i = 0; i < H * W * 4; ++i) {
        rgba.uint8DataMut()[i] = static_cast<uint8_t>((i * 23) % 256);
    }
    std::string rgbaBytes = writeBmpToBytes(rgba);
    Value rgbaBack = readBmp(reinterpret_cast<const uint8_t*>(rgbaBytes.data()), rgbaBytes.size());
    EXPECT_EQ(rgbaBack.dims().pages(), 4u);
    for (size_t i = 0; i < H * W * 4; ++i) {
        EXPECT_EQ(rgbaBack.uint8Data()[i], rgba.uint8Data()[i]);
    }
}

TEST(TgaCodecTest, GrayAndRgbRoundTrip) {
    const size_t H = 20, W = 30;
    // Grayscale
    Value gray = Value::matrix(H, W, ValueType::UINT8);
    for (size_t i = 0; i < H * W; ++i) gray.uint8DataMut()[i] = static_cast<uint8_t>(i % 256);

    std::string grayBytes = writeTgaToBytes(gray);
    Value grayBack = readTga(reinterpret_cast<const uint8_t*>(grayBytes.data()), grayBytes.size());
    EXPECT_EQ(grayBack.dims().rows(), H);
    EXPECT_EQ(grayBack.dims().cols(), W);
    for (size_t i = 0; i < H * W; ++i) {
        EXPECT_EQ(grayBack.uint8Data()[i], gray.uint8Data()[i]);
    }

    // RGB
    Value rgb = Value::matrix3d(H, W, 3, ValueType::UINT8);
    for (size_t i = 0; i < H * W * 3; ++i) rgb.uint8DataMut()[i] = static_cast<uint8_t>((i * 13) % 256);

    std::string rgbBytes = writeTgaToBytes(rgb);
    Value rgbBack = readTga(reinterpret_cast<const uint8_t*>(rgbBytes.data()), rgbBytes.size());
    EXPECT_EQ(rgbBack.dims().pages(), 3u);
    for (size_t i = 0; i < H * W * 3; ++i) {
        EXPECT_EQ(rgbBack.uint8Data()[i], rgb.uint8Data()[i]);
    }
}

TEST(PnmCodecTest, BinaryPgmPpm8BitAnd16BitRoundTrip) {
    const size_t H = 16, W = 24;

    // 8-bit PGM (P5)
    Value pgm8 = Value::matrix(H, W, ValueType::UINT8);
    for (size_t i = 0; i < H * W; ++i) pgm8.uint8DataMut()[i] = static_cast<uint8_t>((i * 7) % 256);

    std::string pgm8Bytes = writePnmToBytes(pgm8);
    EXPECT_EQ(pgm8Bytes.substr(0, 3), "P5\n");
    Value pgm8Back = readPnm(reinterpret_cast<const uint8_t*>(pgm8Bytes.data()), pgm8Bytes.size());
    EXPECT_EQ(pgm8Back.type(), ValueType::UINT8);
    for (size_t i = 0; i < H * W; ++i) {
        EXPECT_EQ(pgm8Back.uint8Data()[i], pgm8.uint8Data()[i]);
    }

    // 16-bit PGM (P5)
    Value pgm16 = Value::matrix(H, W, ValueType::UINT16);
    for (size_t i = 0; i < H * W; ++i) pgm16.uint16DataMut()[i] = static_cast<uint16_t>(i * 100);

    std::string pgm16Bytes = writePnmToBytes(pgm16);
    Value pgm16Back = readPnm(reinterpret_cast<const uint8_t*>(pgm16Bytes.data()), pgm16Bytes.size());
    EXPECT_EQ(pgm16Back.type(), ValueType::UINT16);
    for (size_t i = 0; i < H * W; ++i) {
        EXPECT_EQ(pgm16Back.uint16Data()[i], pgm16.uint16Data()[i]);
    }

    // 8-bit PPM (P6)
    Value ppm8 = Value::matrix3d(H, W, 3, ValueType::UINT8);
    for (size_t i = 0; i < H * W * 3; ++i) ppm8.uint8DataMut()[i] = static_cast<uint8_t>((i * 19) % 256);

    std::string ppm8Bytes = writePnmToBytes(ppm8);
    EXPECT_EQ(ppm8Bytes.substr(0, 3), "P6\n");
    Value ppm8Back = readPnm(reinterpret_cast<const uint8_t*>(ppm8Bytes.data()), ppm8Bytes.size());
    EXPECT_EQ(ppm8Back.dims().pages(), 3u);
    for (size_t i = 0; i < H * W * 3; ++i) {
        EXPECT_EQ(ppm8Back.uint8Data()[i], ppm8.uint8Data()[i]);
    }
}
