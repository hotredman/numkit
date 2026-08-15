// toolboxes/audio/tests/mp3_codec_test.cpp
#include <gtest/gtest.h>
#include "../src/io/mp3_codec.hpp"
#include <vector>

using namespace numkit;
using namespace numkit::audio;

TEST(Mp3CodecTest, MagicAndSniffing) {
    EXPECT_FALSE(isMp3Bytes(nullptr, 0));
    EXPECT_FALSE(isMp3Bytes(reinterpret_cast<const uint8_t *>("RIFF1234"), 8));

    // ID3v2 header
    const uint8_t id3Header[] = "ID3\x03\x00\x00\x00\x00\x00\x10";
    EXPECT_TRUE(isMp3Bytes(id3Header, sizeof(id3Header) - 1));

    // MPEG-1 Layer 3 frame sync: 0xFF, 0xFB (11111111 11111011 = Sync + MPEG1 + Layer3 + no CRC)
    const uint8_t mpeg1Layer3[] = { 0xFF, 0xFB, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00 };
    EXPECT_TRUE(isMp3Bytes(mpeg1Layer3, sizeof(mpeg1Layer3)));
}

TEST(Mp3CodecTest, SyntheticFramePeekAndDecode) {
    // 128 kbps, 44100 Hz, Joint Stereo, MPEG-1 Layer 3
    // Frame size = 144 * 128000 / 44100 = 417 bytes
    std::vector<uint8_t> frame(418, 0);
    frame[0] = 0xFF;
    frame[1] = 0xFB; // MPEG-1, Layer 3, no CRC
    frame[2] = 0x90; // 128 kbps (idx 9), 44100 Hz (idx 0), no padding
    frame[3] = 0x40; // Joint stereo

    AudioInfo info = peekMp3(frame.data(), frame.size());
    EXPECT_EQ(info.format, "mp3");
    EXPECT_EQ(info.numChannels, 2u);
    EXPECT_DOUBLE_EQ(info.sampleRate, 44100.0);
    EXPECT_EQ(info.bitRate, 128u);

    // Decode frame
    AudioData data = readMp3(frame.data(), frame.size());
    EXPECT_EQ(data.numChannels, 2u);
    EXPECT_DOUBLE_EQ(data.sampleRate, 44100.0);
    EXPECT_GT(data.totalSamples, 0u);
    EXPECT_EQ(data.y.dims().rows(), data.totalSamples);
    EXPECT_EQ(data.y.dims().cols(), 2u);
}
