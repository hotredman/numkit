// toolboxes/image/tests/deflate_test.cpp
//
// Tests for in-tree Deflate, Inflate, Zlib, CRC-32, and Adler-32.

#include <gtest/gtest.h>
#include "../src/io/deflate.hpp"
#include <random>
#include <string>
#include <vector>

using namespace numkit::image;

TEST(DeflateTest, Crc32AndAdler32KnownVectors) {
    const std::string text = "123456789";
    EXPECT_EQ(crc32(reinterpret_cast<const uint8_t*>(text.data()), text.size()), 0xCBF43926u);
    EXPECT_EQ(adler32(reinterpret_cast<const uint8_t*>(text.data()), text.size()), 0x091E01DEu);

    const std::string empty = "";
    EXPECT_EQ(crc32(reinterpret_cast<const uint8_t*>(empty.data()), 0), 0x00000000u);
    EXPECT_EQ(adler32(reinterpret_cast<const uint8_t*>(empty.data()), 0), 0x00000001u);
}

TEST(DeflateTest, EmptyRoundTrip) {
    std::vector<uint8_t> empty;
    std::vector<uint8_t> comp = deflateRaw(empty.data(), empty.size());
    std::vector<uint8_t> decomp = inflateRaw(comp.data(), comp.size());
    EXPECT_TRUE(decomp.empty());

    std::vector<uint8_t> zcomp = zlibCompress(empty.data(), empty.size());
    std::vector<uint8_t> zdecomp = zlibDecompress(zcomp.data(), zcomp.size());
    EXPECT_TRUE(zdecomp.empty());
}

TEST(DeflateTest, SmallStringRoundTrip) {
    std::string s = "Hello world! This is Numkit autonomous C++ image compression test.";
    const auto *ptr = reinterpret_cast<const uint8_t*>(s.data());
    
    // Raw Deflate
    std::vector<uint8_t> comp = deflateRaw(ptr, s.size(), 6);
    std::vector<uint8_t> decomp = inflateRaw(comp.data(), comp.size());
    EXPECT_EQ(std::string(decomp.begin(), decomp.end()), s);

    // Zlib format
    std::vector<uint8_t> zcomp = zlibCompress(ptr, s.size(), 6);
    std::vector<uint8_t> zdecomp = zlibDecompress(zcomp.data(), zcomp.size());
    EXPECT_EQ(std::string(zdecomp.begin(), zdecomp.end()), s);
}

TEST(DeflateTest, HighlyRepetitiveData) {
    std::vector<uint8_t> rep(100000, 0x42);
    std::vector<uint8_t> zcomp = zlibCompress(rep.data(), rep.size(), 6);
    EXPECT_LT(zcomp.size(), 1000u); // High compression ratio

    std::vector<uint8_t> zdecomp = zlibDecompress(zcomp.data(), zcomp.size());
    EXPECT_EQ(zdecomp, rep);
}

TEST(DeflateTest, RandomAndPatternedDataRoundTrip) {
    std::mt19937 rng(1337);
    std::vector<uint8_t> data(50000);
    for (size_t i = 0; i < data.size(); ++i) {
        // Mixture of smooth ramps and random noise
        data[i] = static_cast<uint8_t>((i % 256) ^ (rng() % 32));
    }

    // Test multiple compression levels: 0 (store), 1 (fast), 6 (default), 9 (high)
    for (int lvl : {0, 1, 6, 9}) {
        std::vector<uint8_t> zcomp = zlibCompress(data.data(), data.size(), lvl);
        std::vector<uint8_t> zdecomp = zlibDecompress(zcomp.data(), zcomp.size());
        EXPECT_EQ(zdecomp, data);
    }
}
