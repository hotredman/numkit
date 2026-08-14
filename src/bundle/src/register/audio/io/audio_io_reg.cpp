// bundle/src/register/audio/io/audio_io_reg.cpp
//
// Registration of audioread, audiowrite, audioinfo, midiread, midiwrite, midiinfo
// into the NumKit engine with Virtual Filesystem (VFS) integration.

#include <numkit/audio/io/audio_io.hpp>
#include "../../../../../toolboxes/audio/src/io/midi_codec.hpp"

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

namespace numkit::audio::detail {

namespace {

std::string lowerExt(const std::string &path) {
    auto dot = path.find_last_of('.');
    if (dot == std::string::npos) return "";
    std::string ext = path.substr(dot + 1);
    for (char &c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext;
}

} // anonymous

void audioread_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                   CallContext &ctx)
{
    if (args.empty()) {
        throw Error("audioread: requires a filename",
                    0, 0, "audioread", "", "numkit:audioread:nargin");
    }
    if (!args[0].isChar() && !args[0].isString()) {
        throw Error("audioread: filename must be a string",
                    0, 0, "audioread", "", "numkit:audioread:type");
    }

    const std::string path = args[0].toString();
    int64_t startSample = 1;
    int64_t endSample = -1;
    bool nativeType = false;

    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i].isChar() || args[i].isString()) {
            std::string opt = args[i].toString();
            for (char &c : opt) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (opt == "native") nativeType = true;
            else if (opt == "double") nativeType = false;
        } else if (args[i].isNumeric()) {
            const auto &d = args[i].dims();
            size_t n = d.numel();
            if (n >= 1) startSample = static_cast<int64_t>(args[i].elemAsDouble(0));
            if (n >= 2) endSample = static_cast<int64_t>(args[i].elemAsDouble(1));
        }
    }

    // Resolve path via VFS
    auto rp = ctx.engine->resolvePath(path);
    if (!rp.fs || !rp.fs->exists(rp.path)) {
        throw Error("audioread: failed to read '" + path + "' — file not found",
                    0, 0, "audioread", "", "numkit:audioread:fileNotFound");
    }

    const std::string bytes = rp.fs->readFileBytes(rp.path);
    AudioData data = audioreadFromBytes(reinterpret_cast<const uint8_t *>(bytes.data()),
                                       bytes.size(), startSample, endSample, nativeType,
                                       ctx.engine->resource());

    outs[0] = std::move(data.y);
    if (nargout >= 2 && outs.size() >= 2) {
        outs[1] = Value::scalar(data.sampleRate, ctx.engine->resource());
    }
}

void audiowrite_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> /*outs*/,
                    CallContext &ctx)
{
    if (args.size() < 3) {
        throw Error("audiowrite: requires filename, audio data, and sample rate",
                    0, 0, "audiowrite", "", "numkit:audiowrite:nargin");
    }
    if (!args[0].isChar() && !args[0].isString()) {
        throw Error("audiowrite: filename must be a string",
                    0, 0, "audiowrite", "", "numkit:audiowrite:type");
    }
    if (!args[1].isNumeric()) {
        throw Error("audiowrite: audio data must be a numeric matrix",
                    0, 0, "audiowrite", "", "numkit:audiowrite:data");
    }

    const std::string path = args[0].toString();
    const Value &y = args[1];
    double sampleRate = args[2].isNumeric() ? args[2].elemAsDouble(0) : 44100.0;

    std::string ext = lowerExt(path);
    if (ext.empty()) ext = "wav";

    uint16_t bitsPerSample = 16;
    std::string title;
    std::string artist;
    std::string comment;

    // Parse NV pairs
    for (size_t i = 3; i + 1 < args.size(); i += 2) {
        if (!args[i].isChar() && !args[i].isString()) continue;
        std::string key = args[i].toString();
        for (char &c : key) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        if (key == "bitspersample") {
            if (args[i+1].isNumeric()) {
                bitsPerSample = static_cast<uint16_t>(args[i+1].elemAsDouble(0));
            }
        } else if (key == "title") {
            title = args[i+1].toString();
        } else if (key == "artist") {
            artist = args[i+1].toString();
        } else if (key == "comment") {
            comment = args[i+1].toString();
        }
    }

    auto outBytes = audiowriteToBytes(y, sampleRate, ext, bitsPerSample, title, artist, comment);

    auto rp = ctx.engine->resolvePath(path);
    if (!rp.fs) {
        throw Error("audiowrite: no filesystem for '" + path + "'",
                    0, 0, "audiowrite", "", "numkit:audiowrite:fs");
    }

    rp.fs->writeFileBytes(rp.path, std::string(outBytes.begin(), outBytes.end()));
}

void audioinfo_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                   CallContext &ctx)
{
    if (args.empty()) {
        throw Error("audioinfo: requires a filename",
                    0, 0, "audioinfo", "", "numkit:audioinfo:nargin");
    }
    if (!args[0].isChar() && !args[0].isString()) {
        throw Error("audioinfo: filename must be a string",
                    0, 0, "audioinfo", "", "numkit:audioinfo:type");
    }

    const std::string path = args[0].toString();
    auto rp = ctx.engine->resolvePath(path);
    if (!rp.fs || !rp.fs->exists(rp.path)) {
        throw Error("audioinfo: failed to read '" + path + "' — file not found",
                    0, 0, "audioinfo", "", "numkit:audioinfo:fileNotFound");
    }

    const std::string bytes = rp.fs->readFileBytes(rp.path);
    AudioInfo info = audioinfoFromBytes(reinterpret_cast<const uint8_t *>(bytes.data()), bytes.size());

    auto *mr = ctx.engine->resource();
    Value s = Value::structure(mr);
    s.field("Filename")          = Value::fromString(path, mr);
    s.field("CompressionMethod") = Value::fromString(info.compressionMethod, mr);
    s.field("NumChannels")        = Value::scalar(static_cast<double>(info.numChannels), mr);
    s.field("SampleRate")         = Value::scalar(info.sampleRate, mr);
    s.field("TotalSamples")       = Value::scalar(static_cast<double>(info.totalSamples), mr);
    s.field("Duration")           = Value::scalar(info.duration, mr);
    s.field("BitsPerSample")      = Value::scalar(static_cast<double>(info.bitsPerSample), mr);
    s.field("BitRate")            = Value::scalar(static_cast<double>(info.bitRate), mr);
    s.field("Title")              = Value::fromString(info.title, mr);
    s.field("Artist")             = Value::fromString(info.artist, mr);
    s.field("Comment")            = Value::fromString(info.comment, mr);

    outs[0] = std::move(s);
}

void midiread_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                  CallContext &ctx)
{
    if (args.empty()) {
        throw Error("midiread: requires a filename",
                    0, 0, "midiread", "", "numkit:midiread:nargin");
    }
    const std::string path = args[0].toString();
    auto rp = ctx.engine->resolvePath(path);
    if (!rp.fs || !rp.fs->exists(rp.path)) {
        throw Error("midiread: file not found: " + path,
                    0, 0, "midiread", "", "numkit:midiread:fileNotFound");
    }

    const std::string bytes = rp.fs->readFileBytes(rp.path);
    outs[0] = readMidi(reinterpret_cast<const uint8_t *>(bytes.data()), bytes.size(), ctx.engine->resource());

    if (nargout >= 2 && outs.size() >= 2) {
        MidiInfo mi = peekMidi(reinterpret_cast<const uint8_t *>(bytes.data()), bytes.size());
        auto *mr = ctx.engine->resource();
        Value s = Value::structure(mr);
        s.field("Format")               = Value::scalar(static_cast<double>(mi.format), mr);
        s.field("NumTracks")            = Value::scalar(static_cast<double>(mi.numTracks), mr);
        s.field("TicksPerQuarterNote")  = Value::scalar(static_cast<double>(mi.ticksPerQuarterNote), mr);
        s.field("InitialTempoBpm")      = Value::scalar(mi.initialTempoBpm, mr);
        s.field("Duration")             = Value::scalar(mi.duration, mr);
        s.field("TotalNotes")           = Value::scalar(static_cast<double>(mi.totalNotes), mr);
        s.field("TrackName")            = Value::fromString(mi.trackName, mr);
        outs[1] = std::move(s);
    }
}

void midiwrite_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> /*outs*/,
                   CallContext &ctx)
{
    if (args.size() < 2) {
        throw Error("midiwrite: requires filename and notes matrix",
                    0, 0, "midiwrite", "", "numkit:midiwrite:nargin");
    }
    const std::string path = args[0].toString();
    const Value &notes = args[1];

    uint16_t tpqn = 480;
    double tempo = 120.0;
    if (args.size() >= 3 && args[2].isNumeric()) tpqn = static_cast<uint16_t>(args[2].elemAsDouble(0));
    if (args.size() >= 4 && args[3].isNumeric()) tempo = args[3].elemAsDouble(0);

    auto bytes = writeMidiToBytes(notes, tpqn, tempo);
    auto rp = ctx.engine->resolvePath(path);
    if (!rp.fs) {
        throw Error("midiwrite: no filesystem for '" + path + "'",
                    0, 0, "midiwrite", "", "numkit:midiwrite:fs");
    }
    rp.fs->writeFileBytes(rp.path, std::string(bytes.begin(), bytes.end()));
}

void midiinfo_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                  CallContext &ctx)
{
    if (args.empty()) {
        throw Error("midiinfo: requires a filename",
                    0, 0, "midiinfo", "", "numkit:midiinfo:nargin");
    }
    const std::string path = args[0].toString();
    auto rp = ctx.engine->resolvePath(path);
    if (!rp.fs || !rp.fs->exists(rp.path)) {
        throw Error("midiinfo: file not found: " + path,
                    0, 0, "midiinfo", "", "numkit:midiinfo:fileNotFound");
    }

    const std::string bytes = rp.fs->readFileBytes(rp.path);
    MidiInfo mi = peekMidi(reinterpret_cast<const uint8_t *>(bytes.data()), bytes.size());

    auto *mr = ctx.engine->resource();
    Value s = Value::structure(mr);
    s.field("Format")               = Value::scalar(static_cast<double>(mi.format), mr);
    s.field("NumTracks")            = Value::scalar(static_cast<double>(mi.numTracks), mr);
    s.field("TicksPerQuarterNote")  = Value::scalar(static_cast<double>(mi.ticksPerQuarterNote), mr);
    s.field("InitialTempoBpm")      = Value::scalar(mi.initialTempoBpm, mr);
    s.field("Duration")             = Value::scalar(mi.duration, mr);
    s.field("TotalNotes")           = Value::scalar(static_cast<double>(mi.totalNotes), mr);
    s.field("TrackName")            = Value::fromString(mi.trackName, mr);

    outs[0] = std::move(s);
}

} // namespace numkit::audio::detail
