// toolboxes/audio/src/io/midi_codec.hpp
//
// In-tree Standard MIDI File (SMF 0/1) reader, writer, and peeker.
// Zero-dependency pure C++17 implementation.

#pragma once

#include <cstdint>
#include <memory_resource>
#include <numkit/value/value.hpp>
#include <string>
#include <vector>

namespace numkit::audio {

struct MidiNote {
    uint16_t track = 0;
    uint8_t channel = 0;
    uint8_t noteNumber = 60; // Middle C
    uint8_t velocity = 64;
    double startTimeSec = 0.0;
    double endTimeSec = 0.0;
    double durationSec = 0.0;
};

struct MidiInfo {
    uint16_t format = 0;             // 0 or 1
    uint16_t numTracks = 0;
    uint16_t ticksPerQuarterNote = 480;
    double initialTempoBpm = 120.0;
    double duration = 0.0;           // in seconds
    uint64_t totalNotes = 0;
    std::string trackName;
};

/// Sniff MIDI 'MThd' magic directly from an in-memory buffer.
bool isMidiBytes(const std::uint8_t *data, std::size_t len);
bool isMidiBytes(const std::string &b);

/// Peek MIDI file metadata without decoding all note events.
MidiInfo peekMidi(const std::uint8_t *data, std::size_t len);

/// Decode MIDI note matrix: [Track, Channel, Note, Velocity, StartTime(s), EndTime(s)] (N x 6 matrix).
Value readMidi(const std::uint8_t *data, std::size_t len,
               std::pmr::memory_resource *mr = nullptr);

/// Encode note matrix [N x 6] to Standard MIDI File (SMF Format 0).
std::vector<std::uint8_t> writeMidiToBytes(const Value &notesMatrix,
                                           uint16_t ticksPerQuarterNote = 480,
                                           double tempoBpm = 120.0);

} // namespace numkit::audio
