// toolboxes/audio/tests/wav_codec_test.cpp
#include <gtest/gtest.h>
#include "../src/io/wav_codec.hpp"
#include <cmath>
#include <vector>

using namespace numkit;
using namespace numkit::audio;

TEST(WavCodecTest, MagicAndSniffing) {
    EXPECT_FALSE(isWavBytes(nullptr, 0));
    EXPECT_FALSE(isWavBytes(reinterpret_cast<const uint8_t *>("RIFF1234"), 8));

    const uint8_t dummyWav[] = "RIFF\x24\x00\x00\x00WAVEfmt ";
    EXPECT_TRUE(isWavBytes(dummyWav, sizeof(dummyWav) - 1));
}

TEST(WavCodecTest, Pcm16MonoAndStereoRoundTrip) {
    const size_t N = 1000;
    const double Fs = 44100.0;

    // Generate 2-channel stereo sine waves: 440 Hz and 880 Hz
    Value y = Value::matrix(N, 2, ValueType::DOUBLE);
    double *p = y.doubleDataMut();
    for (size_t i = 0; i < N; ++i) {
        p[0 * N + i] = 0.8 * std::sin(2.0 * 3.1415926535 * 440.0 * i / Fs);
        p[1 * N + i] = 0.5 * std::sin(2.0 * 3.1415926535 * 880.0 * i / Fs);
    }

    auto bytes = writeWavToBytes(y, Fs, 16, "Sine Test", "NumKit", "Unit Test");
    EXPECT_GT(bytes.size(), 44u);

    // Peek metadata
    AudioInfo info = peekWav(bytes.data(), bytes.size());
    EXPECT_EQ(info.format, "wav");
    EXPECT_EQ(info.numChannels, 2u);
    EXPECT_DOUBLE_EQ(info.sampleRate, 44100.0);
    EXPECT_EQ(info.bitsPerSample, 16u);
    EXPECT_EQ(info.totalSamples, N);
    EXPECT_EQ(info.title, "Sine Test");
    EXPECT_EQ(info.artist, "NumKit");

    // Read full audio
    AudioData decoded = readWav(bytes.data(), bytes.size());
    EXPECT_EQ(decoded.numChannels, 2u);
    EXPECT_DOUBLE_EQ(decoded.sampleRate, 44100.0);
    EXPECT_EQ(decoded.totalSamples, N);

    const double *dec = decoded.y.doubleData();
    for (size_t i = 0; i < N; ++i) {
        EXPECT_NEAR(dec[0 * N + i], p[0 * N + i], 0.001);
        EXPECT_NEAR(dec[1 * N + i], p[1 * N + i], 0.001);
    }
}

TEST(WavCodecTest, Pcm8And24And32BitRoundTrip) {
    const size_t N = 250;
    const double Fs = 48000.0;
    Value y = Value::matrix(N, 1, ValueType::DOUBLE);
    double *p = y.doubleDataMut();
    for (size_t i = 0; i < N; ++i) {
        p[i] = static_cast<double>(i) / N * 1.8 - 0.9;
    }

    // 8-bit
    {
        auto b8 = writeWavToBytes(y, Fs, 8);
        AudioData d8 = readWav(b8.data(), b8.size());
        EXPECT_EQ(d8.bitsPerSample, 8u);
        const double *p8 = d8.y.doubleData();
        for (size_t i = 0; i < N; ++i) EXPECT_NEAR(p8[i], p[i], 0.02);
    }

    // 24-bit
    {
        auto b24 = writeWavToBytes(y, Fs, 24);
        AudioData d24 = readWav(b24.data(), b24.size());
        EXPECT_EQ(d24.bitsPerSample, 24u);
        const double *p24 = d24.y.doubleData();
        for (size_t i = 0; i < N; ++i) EXPECT_NEAR(p24[i], p[i], 0.0001);
    }

    // 32-bit float
    {
        Value yf = Value::matrix(N, 1, ValueType::SINGLE);
        float *pf = yf.singleDataMut();
        for (size_t i = 0; i < N; ++i) pf[i] = static_cast<float>(p[i]);

        auto bf = writeWavToBytes(yf, Fs, 32);
        AudioData df = readWav(bf.data(), bf.size(), 1, -1, true); // native single
        EXPECT_EQ(df.y.type(), ValueType::SINGLE);
        const float *pfDec = df.y.singleData();
        for (size_t i = 0; i < N; ++i) EXPECT_NEAR(pfDec[i], pf[i], 1e-6f);
    }
}

TEST(WavCodecTest, RangeReading) {
    const size_t N = 1000;
    Value y = Value::matrix(N, 1, ValueType::DOUBLE);
    double *p = y.doubleDataMut();
    for (size_t i = 0; i < N; ++i) p[i] = static_cast<double>(i) / 1000.0;

    auto bytes = writeWavToBytes(y, 44100.0, 16);

    // Read range [101, 200] (100 samples)
    AudioData sub = readWav(bytes.data(), bytes.size(), 101, 200);
    EXPECT_EQ(sub.y.dims().rows(), 100u);
    EXPECT_EQ(sub.y.dims().cols(), 1u);

    const double *subPtr = sub.y.doubleData();
    for (size_t i = 0; i < 100; ++i) {
        EXPECT_NEAR(subPtr[i], p[100 + i], 0.001);
    }
}
