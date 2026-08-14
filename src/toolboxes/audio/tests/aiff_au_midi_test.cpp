// toolboxes/audio/tests/aiff_au_midi_test.cpp
#include <gtest/gtest.h>
#include "../src/io/aiff_codec.hpp"
#include "../src/io/au_codec.hpp"
#include "../src/io/midi_codec.hpp"
#include <cmath>
#include <vector>

using namespace numkit;
using namespace numkit::audio;

TEST(AiffCodecTest, Pcm16And24RoundTrip) {
    const size_t N = 400;
    const double Fs = 48000.0;

    Value y = Value::matrix(N, 2, ValueType::DOUBLE);
    double *p = y.doubleDataMut();
    for (size_t i = 0; i < N; ++i) {
        p[0 * N + i] = 0.7 * std::sin(2.0 * 3.1415926535 * 300.0 * i / Fs);
        p[1 * N + i] = 0.4 * std::sin(2.0 * 3.1415926535 * 600.0 * i / Fs);
    }

    auto bytes = writeAiffToBytes(y, Fs, 16, "Apple Sound", "Cupertino", "Stereo AIFF");
    EXPECT_TRUE(isAiffBytes(bytes.data(), bytes.size()));

    AudioInfo info = peekAiff(bytes.data(), bytes.size());
    EXPECT_EQ(info.format, "aiff");
    EXPECT_EQ(info.numChannels, 2u);
    EXPECT_NEAR(info.sampleRate, 48000.0, 0.1);
    EXPECT_EQ(info.bitsPerSample, 16u);
    EXPECT_EQ(info.totalSamples, N);
    EXPECT_EQ(info.title, "Apple Sound");
    EXPECT_EQ(info.artist, "Cupertino");

    AudioData decoded = readAiff(bytes.data(), bytes.size());
    EXPECT_EQ(decoded.numChannels, 2u);
    EXPECT_NEAR(decoded.sampleRate, 48000.0, 0.1);
    EXPECT_EQ(decoded.totalSamples, N);

    const double *dec = decoded.y.doubleData();
    for (size_t i = 0; i < N; ++i) {
        EXPECT_NEAR(dec[0 * N + i], p[0 * N + i], 0.001);
        EXPECT_NEAR(dec[1 * N + i], p[1 * N + i], 0.001);
    }
}

TEST(AuCodecTest, LinearPcmAndMuLawRoundTrip) {
    const size_t N = 300;
    const double Fs = 16000.0;

    Value y = Value::matrix(N, 1, ValueType::DOUBLE);
    double *p = y.doubleDataMut();
    for (size_t i = 0; i < N; ++i) {
        p[i] = 0.6 * std::sin(2.0 * 3.1415926535 * 1000.0 * i / Fs);
    }

    // 16-bit PCM
    {
        auto bytes = writeAuToBytes(y, Fs, 16, "Sun Microsystems Audio");
        EXPECT_TRUE(isAuBytes(bytes.data(), bytes.size()));

        AudioInfo info = peekAu(bytes.data(), bytes.size());
        EXPECT_EQ(info.format, "au");
        EXPECT_EQ(info.numChannels, 1u);
        EXPECT_DOUBLE_EQ(info.sampleRate, 16000.0);
        EXPECT_EQ(info.bitsPerSample, 16u);
        EXPECT_EQ(info.totalSamples, N);

        AudioData decoded = readAu(bytes.data(), bytes.size());
        EXPECT_EQ(decoded.numChannels, 1u);
        EXPECT_DOUBLE_EQ(decoded.sampleRate, 16000.0);

        const double *dec = decoded.y.doubleData();
        for (size_t i = 0; i < N; ++i) {
            EXPECT_NEAR(dec[i], p[i], 0.001);
        }
    }
}

TEST(MidiCodecTest, NoteMatrixRoundTrip) {
    // 3 notes: C4 (60), E4 (64), G4 (67)
    // [Track, Channel, Note, Velocity, StartTime, EndTime]
    Value notes = Value::matrix(3, 6, ValueType::DOUBLE);
    double *p = notes.doubleDataMut();
    const size_t N = 3;

    // Track
    p[0 * N + 0] = 1; p[0 * N + 1] = 1; p[0 * N + 2] = 1;
    // Channel
    p[1 * N + 0] = 1; p[1 * N + 1] = 1; p[1 * N + 2] = 1;
    // Note
    p[2 * N + 0] = 60; p[2 * N + 1] = 64; p[2 * N + 2] = 67;
    // Velocity
    p[3 * N + 0] = 80; p[3 * N + 1] = 90; p[3 * N + 2] = 100;
    // StartTime
    p[4 * N + 0] = 0.0; p[4 * N + 1] = 0.5; p[4 * N + 2] = 1.0;
    // EndTime
    p[5 * N + 0] = 0.5; p[5 * N + 1] = 1.0; p[5 * N + 2] = 1.5;

    auto midiBytes = writeMidiToBytes(notes, 480, 120.0);
    EXPECT_TRUE(isMidiBytes(midiBytes.data(), midiBytes.size()));

    MidiInfo info = peekMidi(midiBytes.data(), midiBytes.size());
    EXPECT_EQ(info.numTracks, 1u);
    EXPECT_EQ(info.ticksPerQuarterNote, 480u);
    EXPECT_DOUBLE_EQ(info.initialTempoBpm, 120.0);
    EXPECT_EQ(info.totalNotes, 3u);

    Value readNotes = readMidi(midiBytes.data(), midiBytes.size());
    EXPECT_EQ(readNotes.dims().rows(), 3u);
    EXPECT_EQ(readNotes.dims().cols(), 6u);

    const double *rn = readNotes.doubleData();
    for (size_t i = 0; i < 3; ++i) {
        EXPECT_EQ(rn[2 * N + i], p[2 * N + i]); // Note number matches exactly
        EXPECT_EQ(rn[3 * N + i], p[3 * N + i]); // Velocity matches exactly
        EXPECT_NEAR(rn[4 * N + i], p[4 * N + i], 0.01); // Start time
        EXPECT_NEAR(rn[5 * N + i], p[5 * N + i], 0.01); // End time
    }
}
