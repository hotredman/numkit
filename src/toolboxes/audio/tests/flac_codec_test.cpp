// toolboxes/audio/tests/flac_codec_test.cpp
#include <gtest/gtest.h>
#include "../src/io/flac_codec.hpp"
#include <cmath>
#include <vector>

using namespace numkit;
using namespace numkit::audio;

namespace {

// Helper to write bits to MSB-first buffer
class BitWriter {
public:
    void writeBits(uint32_t val, unsigned n) {
        for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
            bitBuf_ = (bitBuf_ << 1) | ((val >> i) & 1);
            if (++bitsCount_ == 8) {
                data_.push_back(static_cast<uint8_t>(bitBuf_));
                bitBuf_ = 0;
                bitsCount_ = 0;
            }
        }
    }

    void writeUtf8(uint32_t val) {
        if (val < 0x80) writeBits(val, 8);
        else {
            writeBits(0xC0 | (val >> 6), 8);
            writeBits(0x80 | (val & 0x3F), 8);
        }
    }

    void alignToByte() {
        if (bitsCount_ > 0) {
            writeBits(0, 8 - bitsCount_);
        }
    }

    const std::vector<uint8_t> &data() const { return data_; }

private:
    std::vector<uint8_t> data_;
    uint32_t bitBuf_ = 0;
    unsigned bitsCount_ = 0;
};

// Construct a minimal valid 16-bit stereo FLAC stream with Verbatim subframe
std::vector<uint8_t> makeSyntheticFlac(uint32_t blockSize, uint32_t sampleRate,
                                       const std::vector<int16_t> &left,
                                       const std::vector<int16_t> &right)
{
    BitWriter bw;
    // Magic 'fLaC'
    bw.writeBits('f', 8); bw.writeBits('L', 8); bw.writeBits('a', 8); bw.writeBits('C', 8);

    // Metadata block 0 (STREAMINFO, last=1, len=34)
    bw.writeBits(0x80, 8); // isLast=1, type=0
    bw.writeBits(0, 8); bw.writeBits(0, 8); bw.writeBits(34, 8); // length 34

    bw.writeBits(blockSize, 16);  // min block
    bw.writeBits(blockSize, 16);  // max block
    bw.writeBits(0, 24);          // min frame
    bw.writeBits(0, 24);          // max frame
    bw.writeBits(sampleRate, 20); // sample rate
    bw.writeBits(1, 3);           // channels - 1 = 1 (2 channels)
    bw.writeBits(15, 5);          // bps - 1 = 15 (16 bps)
    bw.writeBits(0, 4);           // total samples (high 4)
    bw.writeBits(blockSize, 32);  // total samples (low 32)
    for (int i = 0; i < 16; ++i) bw.writeBits(0, 8); // MD5

    // Frame Header
    bw.writeBits(0x3FFE, 14); // Sync
    bw.writeBits(0, 1);       // reserved
    bw.writeBits(0, 1);       // fixed block size
    // Block size code: 0010 = 576 if 576, or code 7 for custom
    bw.writeBits(7, 4);       // get 16-bit blocksize at end of header
    bw.writeBits(9, 4);       // 44100 Hz
    bw.writeBits(1, 4);       // Left, Right independent
    bw.writeBits(4, 3);       // 16-bit
    bw.writeBits(0, 1);       // reserved
    bw.writeUtf8(0);          // frame number 0
    bw.writeBits(blockSize - 1, 16); // custom block size
    bw.writeBits(0, 8);       // CRC-8

    // Subframe 0 (Left): Verbatim
    bw.writeBits(0, 1);       // zero
    bw.writeBits(1, 6);       // Verbatim (000001)
    bw.writeBits(0, 1);       // wasted bits = 0
    for (size_t i = 0; i < blockSize; ++i) {
        bw.writeBits(static_cast<uint16_t>(left[i]), 16);
    }

    // Subframe 1 (Right): Verbatim
    bw.writeBits(0, 1);
    bw.writeBits(1, 6);
    bw.writeBits(0, 1);
    for (size_t i = 0; i < blockSize; ++i) {
        bw.writeBits(static_cast<uint16_t>(right[i]), 16);
    }

    bw.alignToByte();
    bw.writeBits(0, 16); // CRC-16 footer

    return bw.data();
}

} // anonymous

TEST(FlacCodecTest, MagicAndSniffing) {
    EXPECT_FALSE(isFlacBytes(nullptr, 0));
    EXPECT_FALSE(isFlacBytes(reinterpret_cast<const uint8_t *>("fLaX"), 4));
    EXPECT_TRUE(isFlacBytes(reinterpret_cast<const uint8_t *>("fLaC1234"), 8));
}

TEST(FlacCodecTest, SyntheticStreamDecode) {
    const uint32_t N = 128;
    std::vector<int16_t> left(N), right(N);
    for (uint32_t i = 0; i < N; ++i) {
        left[i] = static_cast<int16_t>(i * 200 - 10000);
        right[i] = static_cast<int16_t>(10000 - i * 150);
    }

    auto flacBytes = makeSyntheticFlac(N, 44100, left, right);
    EXPECT_GT(flacBytes.size(), 42u);

    // Peek
    AudioInfo info = peekFlac(flacBytes.data(), flacBytes.size());
    EXPECT_EQ(info.format, "flac");
    EXPECT_EQ(info.numChannels, 2u);
    EXPECT_DOUBLE_EQ(info.sampleRate, 44100.0);
    EXPECT_EQ(info.bitsPerSample, 16u);
    EXPECT_EQ(info.totalSamples, N);

    // Read full audio
    AudioData data = readFlac(flacBytes.data(), flacBytes.size());
    EXPECT_EQ(data.numChannels, 2u);
    EXPECT_DOUBLE_EQ(data.sampleRate, 44100.0);
    EXPECT_EQ(data.totalSamples, N);

    const double *y = data.y.doubleData();
    for (uint32_t i = 0; i < N; ++i) {
        EXPECT_NEAR(y[0 * N + i], static_cast<double>(left[i]) / 32768.0, 0.0001);
        EXPECT_NEAR(y[1 * N + i], static_cast<double>(right[i]) / 32768.0, 0.0001);
    }
}
