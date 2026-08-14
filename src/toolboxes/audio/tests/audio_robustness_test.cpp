// toolboxes/audio/tests/audio_robustness_test.cpp
#include <gtest/gtest.h>
#include "../src/io/wav_codec.hpp"
#include "../src/io/flac_codec.hpp"
#include "../src/io/mp3_codec.hpp"
#include "../src/io/aiff_codec.hpp"
#include "../src/io/au_codec.hpp"
#include "../src/io/midi_codec.hpp"
#include <numkit/value/error.hpp>
#include <vector>

using namespace numkit;
using namespace numkit::audio;

TEST(AudioRobustnessTest, EmptyAndTinyBuffersThrowCleanly) {
    const uint8_t tiny[] = "RIF";
    EXPECT_THROW(readWav(nullptr, 0), Error);
    EXPECT_THROW(readWav(tiny, 3), Error);
    EXPECT_THROW(peekWav(tiny, 3), Error);

    EXPECT_THROW(readFlac(nullptr, 0), Error);
    EXPECT_THROW(readFlac(tiny, 3), Error);
    EXPECT_THROW(peekFlac(tiny, 3), Error);

    EXPECT_THROW(readMp3(nullptr, 0), Error);
    EXPECT_THROW(readMp3(tiny, 3), Error);
    EXPECT_THROW(peekMp3(tiny, 3), Error);

    EXPECT_THROW(readAiff(nullptr, 0), Error);
    EXPECT_THROW(readAiff(tiny, 3), Error);
    EXPECT_THROW(peekAiff(tiny, 3), Error);

    EXPECT_THROW(readAu(nullptr, 0), Error);
    EXPECT_THROW(readAu(tiny, 3), Error);
    EXPECT_THROW(peekAu(tiny, 3), Error);

    EXPECT_THROW(readMidi(nullptr, 0), Error);
    EXPECT_THROW(readMidi(tiny, 3), Error);
    EXPECT_THROW(peekMidi(tiny, 3), Error);
}

TEST(AudioRobustnessTest, TruncatedWavDataThrows) {
    Value y = Value::matrix(100, 1, ValueType::DOUBLE);
    auto bytes = writeWavToBytes(y, 44100.0, 16);
    EXPECT_GT(bytes.size(), 44u);

    // Truncate the payload by 50 bytes
    std::vector<uint8_t> truncated(bytes.begin(), bytes.end() - 50);
    EXPECT_THROW(readWav(truncated.data(), truncated.size()), Error);
}

TEST(AudioRobustnessTest, ExtremeSampleRatesAndMultiChannel) {
    const size_t N = 100;
    const size_t C = 6; // 5.1 surround 6-channel
    const double Fs = 192000.0; // 192 kHz studio rate

    Value y = Value::matrix(N, C, ValueType::DOUBLE);
    double *p = y.doubleDataMut();
    for (size_t c = 0; c < C; ++c) {
        for (size_t t = 0; t < N; ++t) {
            p[c * N + t] = 0.1 * (c + 1);
        }
    }

    auto bytes = writeWavToBytes(y, Fs, 24);
    AudioData d = readWav(bytes.data(), bytes.size());
    EXPECT_EQ(d.numChannels, 6u);
    EXPECT_DOUBLE_EQ(d.sampleRate, 192000.0);
    EXPECT_EQ(d.totalSamples, N);

    const double *dec = d.y.doubleData();
    for (size_t c = 0; c < C; ++c) {
        for (size_t t = 0; t < N; ++t) {
            EXPECT_NEAR(dec[c * N + t], 0.1 * (c + 1), 0.0001);
        }
    }
}
