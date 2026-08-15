// toolboxes/audio/tests/audio_e2e_io_test.cpp
#include <gtest/gtest.h>
#include <numkit/bundle/standard_library.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/fs/vfs.hpp>
#include <filesystem>
#include <vector>

using namespace numkit;

class AudioE2EIoTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine_ = makeStandardEngine();
        engine_->eval("import compat.*;");
    }

    void TearDown() override {
        for (const auto &f : createdFiles_) {
            std::error_code ec;
            std::filesystem::remove(f, ec);
        }
    }

    void trackFile(const std::string &path) {
        createdFiles_.push_back(path);
    }

    std::unique_ptr<Engine> engine_;
    std::vector<std::string> createdFiles_;
};

TEST_F(AudioE2EIoTest, WavFullWorkflow) {
    trackFile("e2e_audio.wav");

    // Generate signal and write WAV
    engine_->eval("Fs = 44100; t = (0:999)' / Fs; y = [0.8*sin(2*pi*440*t), 0.4*sin(2*pi*880*t)];");
    engine_->eval("audiowrite('e2e_audio.wav', y, Fs, 'Title', 'Sine440', 'Artist', 'E2E Test');");

    // Info
    engine_->eval("info = audioinfo('e2e_audio.wav');");
    EXPECT_DOUBLE_EQ(engine_->eval("info.SampleRate").elemAsDouble(0), 44100.0);
    EXPECT_DOUBLE_EQ(engine_->eval("info.NumChannels").elemAsDouble(0), 2.0);
    EXPECT_DOUBLE_EQ(engine_->eval("info.TotalSamples").elemAsDouble(0), 1000.0);

    // Read full
    engine_->eval("[y_read, Fs_read] = audioread('e2e_audio.wav');");
    EXPECT_DOUBLE_EQ(engine_->eval("Fs_read").elemAsDouble(0), 44100.0);
    EXPECT_DOUBLE_EQ(engine_->eval("size(y_read, 1)").elemAsDouble(0), 1000.0);
    EXPECT_DOUBLE_EQ(engine_->eval("size(y_read, 2)").elemAsDouble(0), 2.0);

    // Read range
    engine_->eval("[y_sub, Fs_sub] = audioread('e2e_audio.wav', [100, 199]);");
    EXPECT_DOUBLE_EQ(engine_->eval("size(y_sub, 1)").elemAsDouble(0), 100.0);
}

TEST_F(AudioE2EIoTest, AiffWorkflow) {
    trackFile("e2e_audio.aiff");

    engine_->eval("Fs = 48000; y = 0.5 * sin(2*pi*500*(0:499)' / Fs);");
    engine_->eval("audiowrite('e2e_audio.aiff', y, Fs, 'BitsPerSample', 24);");

    engine_->eval("info = audioinfo('e2e_audio.aiff');");
    EXPECT_NEAR(engine_->eval("info.SampleRate").elemAsDouble(0), 48000.0, 0.1);
    EXPECT_DOUBLE_EQ(engine_->eval("info.BitsPerSample").elemAsDouble(0), 24.0);

    engine_->eval("[y_read, Fs_read] = audioread('e2e_audio.aiff');");
    EXPECT_NEAR(engine_->eval("Fs_read").elemAsDouble(0), 48000.0, 0.1);
    EXPECT_DOUBLE_EQ(engine_->eval("size(y_read, 1)").elemAsDouble(0), 500.0);
}

TEST_F(AudioE2EIoTest, AuWorkflow) {
    trackFile("e2e_audio.au");

    engine_->eval("Fs = 16000; y = 0.6 * sin(2*pi*300*(0:299)' / Fs);");
    engine_->eval("audiowrite('e2e_audio.au', y, Fs, 'BitsPerSample', 16);");

    engine_->eval("info = audioinfo('e2e_audio.au');");
    EXPECT_DOUBLE_EQ(engine_->eval("info.SampleRate").elemAsDouble(0), 16000.0);

    engine_->eval("[y_read, Fs_read] = audioread('e2e_audio.au');");
    EXPECT_DOUBLE_EQ(engine_->eval("Fs_read").elemAsDouble(0), 16000.0);
    EXPECT_DOUBLE_EQ(engine_->eval("size(y_read, 1)").elemAsDouble(0), 300.0);
}

TEST_F(AudioE2EIoTest, MidiWorkflow) {
    trackFile("e2e_music.mid");

    // [Track, Channel, Note, Velocity, StartTime, EndTime]
    engine_->eval("notes = [1, 1, 60, 90, 0.0, 0.5; 1, 1, 64, 95, 0.5, 1.0; 1, 1, 67, 100, 1.0, 1.5];");
    engine_->eval("midiwrite('e2e_music.mid', notes, 480, 120);");

    engine_->eval("minfo = midiinfo('e2e_music.mid');");
    EXPECT_DOUBLE_EQ(engine_->eval("minfo.TotalNotes").elemAsDouble(0), 3.0);
    EXPECT_DOUBLE_EQ(engine_->eval("minfo.InitialTempoBpm").elemAsDouble(0), 120.0);

    engine_->eval("[notes_read, info_read] = midiread('e2e_music.mid');");
    EXPECT_DOUBLE_EQ(engine_->eval("size(notes_read, 1)").elemAsDouble(0), 3.0);
    EXPECT_DOUBLE_EQ(engine_->eval("size(notes_read, 2)").elemAsDouble(0), 6.0);
}
