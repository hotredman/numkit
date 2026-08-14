// toolboxes/audio/src/io/midi_codec.cpp
//
// In-tree Standard MIDI File (SMF 0/1) reader, writer, and peeker.
// Zero-dependency pure C++17 implementation.

#include "midi_codec.hpp"

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace numkit::audio {

bool isMidiBytes(const std::uint8_t *data, std::size_t len)
{
    if (len < 8) return false;
    return (data[0] == 'M' && data[1] == 'T' && data[2] == 'h' && data[3] == 'd');
}

bool isMidiBytes(const std::string &b)
{
    return isMidiBytes(reinterpret_cast<const std::uint8_t *>(b.data()), b.size());
}

namespace {

inline uint16_t readU16BE(const uint8_t *p) {
    return static_cast<uint16_t>((p[0] << 8) | p[1]);
}

inline uint32_t readU32BE(const uint8_t *p) {
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8)  |
            static_cast<uint32_t>(p[3]);
}

inline void writeU16BE(std::vector<uint8_t> &buf, uint16_t v) {
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
}

inline void writeU32BE(std::vector<uint8_t> &buf, uint32_t v) {
    buf.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
}

uint32_t readVlq(const uint8_t *data, size_t len, size_t &pos) {
    uint32_t val = 0;
    for (int i = 0; i < 4 && pos < len; ++i) {
        uint8_t byte = data[pos++];
        val = (val << 7) | (byte & 0x7F);
        if (!(byte & 0x80)) break;
    }
    return val;
}

void writeVlq(std::vector<uint8_t> &buf, uint32_t val) {
    uint32_t buffer = val & 0x7F;
    while ((val >>= 7)) {
        buffer <<= 8;
        buffer |= ((val & 0x7F) | 0x80);
    }
    while (true) {
        buf.push_back(static_cast<uint8_t>(buffer & 0xFF));
        if (buffer & 0x80) buffer >>= 8;
        else break;
    }
}

struct RawMidiEvent {
    uint64_t absoluteTick = 0;
    uint16_t track = 0;
    uint8_t status = 0;
    uint8_t data1 = 0;
    uint8_t data2 = 0;
    std::vector<uint8_t> metaData;
};

struct TempoPoint {
    uint64_t tick = 0;
    double timeSec = 0.0;
    uint32_t usPerQuarter = 500000; // default 120 BPM
};

double tickToTime(uint64_t tick, const std::vector<TempoPoint> &tempoMap, uint16_t tpqn) {
    if (tempoMap.empty() || tpqn == 0) return 0.0;

    size_t idx = 0;
    for (size_t i = 1; i < tempoMap.size(); ++i) {
        if (tempoMap[i].tick <= tick) idx = i;
        else break;
    }

    const auto &pt = tempoMap[idx];
    uint64_t deltaTicks = tick - pt.tick;
    double secondsPerTick = (static_cast<double>(pt.usPerQuarter) / 1000000.0) / tpqn;
    return pt.timeSec + deltaTicks * secondsPerTick;
}

} // anonymous

MidiInfo peekMidi(const std::uint8_t *data, std::size_t len)
{
    if (!isMidiBytes(data, len) || len < 14)
        throw Error("midiinfo: not a valid MIDI file (missing MThd header)",
                    0, 0, "midiinfo", "", "numkit:midiinfo:badMagic");

    uint32_t headerLen = readU32BE(data + 4);
    if (headerLen < 6)
        throw Error("midiinfo: malformed MThd chunk",
                    0, 0, "midiinfo", "", "numkit:midiinfo:badHeader");

    uint16_t format = readU16BE(data + 8);
    uint16_t numTracks = readU16BE(data + 10);
    uint16_t tpqn = readU16BE(data + 12);

    MidiInfo info;
    info.format = format;
    info.numTracks = numTracks;
    info.ticksPerQuarterNote = (tpqn & 0x8000) ? 480 : tpqn; // handle SMPTE default fallback

    // Quick track scan to determine notes, tempo, and duration
    size_t pos = 8 + headerLen;
    uint64_t maxTick = 0;
    uint64_t noteCount = 0;
    uint32_t initialUsPerQuarter = 500000;

    for (uint16_t trk = 0; trk < numTracks && pos + 8 <= len; ++trk) {
        if (std::memcmp(data + pos, "MTrk", 4) != 0) break;
        uint32_t trkLen = readU32BE(data + pos + 4);
        pos += 8;
        size_t trkEnd = pos + trkLen;
        if (trkEnd > len) trkEnd = len;

        uint64_t currentTick = 0;
        uint8_t runningStatus = 0;

        while (pos < trkEnd) {
            uint32_t dt = readVlq(data, trkEnd, pos);
            currentTick += dt;
            if (pos >= trkEnd) break;

            uint8_t status = data[pos];
            if (status & 0x80) {
                runningStatus = status;
                ++pos;
            } else {
                status = runningStatus;
            }

            if (status == 0xFF) { // Meta Event
                if (pos >= trkEnd) break;
                uint8_t metaType = data[pos++];
                uint32_t metaLen = readVlq(data, trkEnd, pos);
                if (metaType == 0x51 && metaLen == 3 && pos + 3 <= trkEnd) { // Set Tempo
                    if (currentTick == 0) {
                        initialUsPerQuarter = (static_cast<uint32_t>(data[pos]) << 16) |
                                              (static_cast<uint32_t>(data[pos+1]) << 8) |
                                               static_cast<uint32_t>(data[pos+2]);
                    }
                } else if (metaType == 0x03 && metaLen > 0 && pos + metaLen <= trkEnd && info.trackName.empty()) {
                    info.trackName = std::string(reinterpret_cast<const char *>(data + pos), metaLen);
                }
                pos += metaLen;
            } else if (status == 0xF0 || status == 0xF7) { // Sysex
                uint32_t sysexLen = readVlq(data, trkEnd, pos);
                pos += sysexLen;
            } else { // Channel Voice Message
                uint8_t msgType = status & 0xF0;
                if (msgType == 0x90) { // Note On
                    uint8_t note = (pos < trkEnd) ? data[pos++] : 0;
                    uint8_t vel  = (pos < trkEnd) ? data[pos++] : 0;
                    (void)note;
                    if (vel > 0) ++noteCount;
                } else if (msgType == 0x80 || msgType == 0xA0 || msgType == 0xB0 || msgType == 0xE0) {
                    if (pos < trkEnd) ++pos;
                    if (pos < trkEnd) ++pos;
                } else if (msgType == 0xC0 || msgType == 0xD0) {
                    if (pos < trkEnd) ++pos;
                }
            }
        }
        if (currentTick > maxTick) maxTick = currentTick;
        pos = trkEnd;
    }

    info.initialTempoBpm = 60000000.0 / static_cast<double>(initialUsPerQuarter);
    info.totalNotes = noteCount;
    if (info.ticksPerQuarterNote > 0) {
        double secPerTick = (static_cast<double>(initialUsPerQuarter) / 1000000.0) / info.ticksPerQuarterNote;
        info.duration = maxTick * secPerTick;
    }

    return info;
}

Value readMidi(const std::uint8_t *data, std::size_t len,
               std::pmr::memory_resource *mr)
{
    if (!isMidiBytes(data, len) || len < 14)
        throw Error("midiread: not a valid MIDI file (missing MThd header)",
                    0, 0, "midiread", "", "numkit:midiread:badMagic");

    uint32_t headerLen = readU32BE(data + 4);
    uint16_t format = readU16BE(data + 8);
    uint16_t numTracks = readU16BE(data + 10);
    uint16_t tpqn = readU16BE(data + 12);
    (void)format;
    if (tpqn == 0 || (tpqn & 0x8000)) tpqn = 480;

    std::vector<RawMidiEvent> events;
    std::vector<TempoPoint> tempoMap;
    tempoMap.push_back({0, 0.0, 500000}); // 120 BPM default

    size_t pos = 8 + headerLen;

    for (uint16_t trk = 0; trk < numTracks && pos + 8 <= len; ++trk) {
        if (std::memcmp(data + pos, "MTrk", 4) != 0) break;
        uint32_t trkLen = readU32BE(data + pos + 4);
        pos += 8;
        size_t trkEnd = pos + trkLen;
        if (trkEnd > len) trkEnd = len;

        uint64_t currentTick = 0;
        uint8_t runningStatus = 0;

        while (pos < trkEnd) {
            uint32_t dt = readVlq(data, trkEnd, pos);
            currentTick += dt;
            if (pos >= trkEnd) break;

            uint8_t status = data[pos];
            if (status & 0x80) {
                runningStatus = status;
                ++pos;
            } else {
                status = runningStatus;
            }

            if (status == 0xFF) { // Meta Event
                if (pos >= trkEnd) break;
                uint8_t metaType = data[pos++];
                uint32_t metaLen = readVlq(data, trkEnd, pos);
                if (metaType == 0x51 && metaLen == 3 && pos + 3 <= trkEnd) { // Set Tempo
                    uint32_t us = (static_cast<uint32_t>(data[pos]) << 16) |
                                  (static_cast<uint32_t>(data[pos+1]) << 8) |
                                   static_cast<uint32_t>(data[pos+2]);
                    double tSec = tickToTime(currentTick, tempoMap, tpqn);
                    tempoMap.push_back({currentTick, tSec, us});
                }
                pos += metaLen;
            } else if (status == 0xF0 || status == 0xF7) { // Sysex
                uint32_t sysexLen = readVlq(data, trkEnd, pos);
                pos += sysexLen;
            } else { // Voice message
                uint8_t msgType = status & 0xF0;
                uint8_t ch = status & 0x0F;
                uint8_t d1 = (pos < trkEnd) ? data[pos++] : 0;
                uint8_t d2 = 0;
                if (msgType != 0xC0 && msgType != 0xD0 && pos < trkEnd) {
                    d2 = data[pos++];
                }

                if (msgType == 0x90 || msgType == 0x80) {
                    RawMidiEvent ev;
                    ev.absoluteTick = currentTick;
                    ev.track = trk + 1;
                    ev.status = (msgType == 0x90 && d2 > 0) ? (0x90 | ch) : (0x80 | ch);
                    ev.data1 = d1;
                    ev.data2 = d2;
                    events.push_back(ev);
                }
            }
        }
        pos = trkEnd;
    }

    // Sort tempo points by tick
    std::sort(tempoMap.begin(), tempoMap.end(), [](const TempoPoint &a, const TempoPoint &b) {
        return a.tick < b.tick;
    });

    // Pair Note-On with Note-Off into completed MidiNote structs
    struct ActiveNote {
        uint64_t startTick = 0;
        uint8_t velocity = 64;
    };

    std::map<uint32_t, std::vector<ActiveNote>> activeNotes; // Key: (track << 16) | (channel << 8) | note
    std::vector<MidiNote> finalNotes;

    for (const auto &ev : events) {
        uint8_t ch = ev.status & 0x0F;
        uint8_t noteNum = ev.data1;
        uint32_t key = (static_cast<uint32_t>(ev.track) << 16) | (static_cast<uint32_t>(ch) << 8) | noteNum;

        if ((ev.status & 0xF0) == 0x90) { // Note-On
            activeNotes[key].push_back({ev.absoluteTick, ev.data2});
        } else { // Note-Off
            auto it = activeNotes.find(key);
            if (it != activeNotes.end() && !it->second.empty()) {
                ActiveNote an = it->second.front();
                it->second.erase(it->second.begin());

                MidiNote n;
                n.track = ev.track;
                n.channel = ch + 1; // 1-based channel
                n.noteNumber = noteNum;
                n.velocity = an.velocity;
                n.startTimeSec = tickToTime(an.startTick, tempoMap, tpqn);
                n.endTimeSec = tickToTime(ev.absoluteTick, tempoMap, tpqn);
                if (n.endTimeSec < n.startTimeSec) n.endTimeSec = n.startTimeSec;
                n.durationSec = n.endTimeSec - n.startTimeSec;
                finalNotes.push_back(n);
            }
        }
    }

    // Return N x 6 double matrix: [Track, Channel, Note, Velocity, StartTime, EndTime]
    size_t numNotes = finalNotes.size();
    Value res = Value::matrix(numNotes, 6, ValueType::DOUBLE, mr);
    double *dst = res.doubleDataMut();

    for (size_t i = 0; i < numNotes; ++i) {
        const auto &n = finalNotes[i];
        dst[0 * numNotes + i] = static_cast<double>(n.track);
        dst[1 * numNotes + i] = static_cast<double>(n.channel);
        dst[2 * numNotes + i] = static_cast<double>(n.noteNumber);
        dst[3 * numNotes + i] = static_cast<double>(n.velocity);
        dst[4 * numNotes + i] = n.startTimeSec;
        dst[5 * numNotes + i] = n.endTimeSec;
    }

    return res;
}

std::vector<std::uint8_t> writeMidiToBytes(const Value &notesMatrix,
                                           uint16_t ticksPerQuarterNote,
                                           double tempoBpm)
{
    const auto &d = notesMatrix.dims();
    size_t numNotes = d.rows();
    if (ticksPerQuarterNote == 0) ticksPerQuarterNote = 480;
    if (tempoBpm <= 0) tempoBpm = 120.0;

    uint32_t usPerQuarter = static_cast<uint32_t>(60000000.0 / tempoBpm);
    double secondsPerTick = (static_cast<double>(usPerQuarter) / 1000000.0) / ticksPerQuarterNote;

    struct MidiEvent {
        uint64_t tick;
        uint8_t status;
        uint8_t data1;
        uint8_t data2;
    };

    std::vector<MidiEvent> eventList;

    for (size_t i = 0; i < numNotes; ++i) {
        uint8_t ch = 0;
        if (d.cols() >= 2) ch = static_cast<uint8_t>(std::max(1.0, notesMatrix.elemAsDouble(1 * numNotes + i)) - 1);
        uint8_t noteNum = 60;
        if (d.cols() >= 3) noteNum = static_cast<uint8_t>(notesMatrix.elemAsDouble(2 * numNotes + i));
        uint8_t vel = 64;
        if (d.cols() >= 4) vel = static_cast<uint8_t>(notesMatrix.elemAsDouble(3 * numNotes + i));
        double startSec = (d.cols() >= 5) ? notesMatrix.elemAsDouble(4 * numNotes + i) : 0.0;
        double endSec = (d.cols() >= 6) ? notesMatrix.elemAsDouble(5 * numNotes + i) : (startSec + 0.5);

        uint64_t startTick = static_cast<uint64_t>(startSec / secondsPerTick);
        uint64_t endTick = static_cast<uint64_t>(endSec / secondsPerTick);
        if (endTick <= startTick) endTick = startTick + 1;

        eventList.push_back({startTick, static_cast<uint8_t>(0x90 | (ch & 0x0F)), noteNum, vel});
        eventList.push_back({endTick, static_cast<uint8_t>(0x80 | (ch & 0x0F)), noteNum, 0});
    }

    // Sort events by tick
    std::sort(eventList.begin(), eventList.end(), [](const MidiEvent &a, const MidiEvent &b) {
        if (a.tick != b.tick) return a.tick < b.tick;
        return (a.status & 0xF0) < (b.status & 0xF0); // Note-Off before Note-On at same tick
    });

    // Build MTrk payload
    std::vector<uint8_t> trkPayload;

    // Tempo meta event at tick 0
    writeVlq(trkPayload, 0); // delta-time 0
    trkPayload.push_back(0xFF);
    trkPayload.push_back(0x51);
    trkPayload.push_back(0x03);
    trkPayload.push_back(static_cast<uint8_t>((usPerQuarter >> 16) & 0xFF));
    trkPayload.push_back(static_cast<uint8_t>((usPerQuarter >> 8) & 0xFF));
    trkPayload.push_back(static_cast<uint8_t>(usPerQuarter & 0xFF));

    uint64_t lastTick = 0;
    for (const auto &ev : eventList) {
        uint32_t dt = static_cast<uint32_t>(ev.tick - lastTick);
        writeVlq(trkPayload, dt);
        trkPayload.push_back(ev.status);
        trkPayload.push_back(ev.data1);
        trkPayload.push_back(ev.data2);
        lastTick = ev.tick;
    }

    // End of Track meta event
    writeVlq(trkPayload, 0);
    trkPayload.push_back(0xFF);
    trkPayload.push_back(0x2F);
    trkPayload.push_back(0x00);

    // SMF Format 0 file
    std::vector<uint8_t> out;
    // MThd
    out.insert(out.end(), {'M', 'T', 'h', 'd'});
    writeU32BE(out, 6);
    writeU16BE(out, 0); // Format 0
    writeU16BE(out, 1); // 1 Track
    writeU16BE(out, ticksPerQuarterNote);

    // MTrk
    out.insert(out.end(), {'M', 'T', 'r', 'k'});
    writeU32BE(out, static_cast<uint32_t>(trkPayload.size()));
    out.insert(out.end(), trkPayload.begin(), trkPayload.end());

    return out;
}

} // namespace numkit::audio
