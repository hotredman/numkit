// toolboxes/image/src/io/io.cpp
//
// In-tree Image I/O: imread, imwrite, imfinfo routing exclusively to
// autonomous zero-dependency C++20 codecs (PNG, JPEG, BMP, TGA, PNM, TIFF).

#include <numkit/image/io/io.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include "io_detail.hpp"
#include "bmp_codec.hpp"
#include "tga_codec.hpp"
#include "pnm_codec.hpp"
#include "png_codec.hpp"
#include "jpeg_codec.hpp"
#include "tiff_codec.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace numkit::image {

namespace {

// Sniff file format by inspecting magic bytes of an in-memory buffer.
// Returns one of "png" / "jpg" / "bmp" / "pnm" / "tif" / "tga" / "" (unknown).
std::string detectFormatBytes(const std::string &bytes) {
    const std::size_t n = bytes.size();
    if (n < 2) return {};
    const auto *hdr = reinterpret_cast<const unsigned char *>(bytes.data());

    // PNG: 89 50 4E 47 0D 0A 1A 0A
    if (n >= 8 && hdr[0] == 0x89 && hdr[1] == 'P' && hdr[2] == 'N' &&
        hdr[3] == 'G' && hdr[4] == 0x0D && hdr[5] == 0x0A)
        return "png";

    // TIFF
    if (isTiffBytes(bytes)) return "tif";

    // JPEG: starts with FF D8 FF
    if (n >= 3 && hdr[0] == 0xFF && hdr[1] == 0xD8 && hdr[2] == 0xFF)
        return "jpg";

    // BMP: 'BM'
    if (hdr[0] == 'B' && hdr[1] == 'M') return "bmp";

    // PNM: 'P1' .. 'P6'
    if (hdr[0] == 'P' && hdr[1] >= '1' && hdr[1] <= '6')
        return "pnm";

    // TGA (last 18 bytes signature if 2.0 or valid header)
    if (n >= 18) {
        std::uint8_t imageType = hdr[2];
        if (imageType == 1 || imageType == 2 || imageType == 3 ||
            imageType == 9 || imageType == 10 || imageType == 11) {
            return "tga";
        }
    }

    return {};
}

const char *colorTypeFromChannels(int c) {
    switch (c) {
        case 1: return "grayscale";
        case 2: return "grayscalealpha";
        case 3: return "truecolor";
        case 4: return "truecoloralpha";
        default: return "unknown";
    }
}

} // anonymous namespace

// Decode an image from in-memory bytes through autonomous in-tree codecs.
Value imreadFromBytes(const std::string &bytes, std::pmr::memory_resource *mr) {
    const auto *data = reinterpret_cast<const std::uint8_t *>(bytes.data());
    const std::size_t len = bytes.size();

    if (len == 0) {
        throw Error("imread: empty image buffer",
                    0, 0, "imread", "", "numkit:imread:empty");
    }

    std::string fmt = detectFormatBytes(bytes);

    if (fmt == "png") {
        return readPng(data, len, mr);
    }
    if (fmt == "tif") {
        return readTiff(std::vector<std::uint8_t>(bytes.begin(), bytes.end()), 1u, mr);
    }
    if (fmt == "jpg") {
        return readJpeg(data, len, mr);
    }
    if (fmt == "bmp") {
        return readBmp(data, len, mr);
    }
    if (fmt == "pnm") {
        return readPnm(data, len, mr);
    }
    if (fmt == "tga") {
        return readTga(data, len, mr);
    }

    // Fallback: try PNG then BMP then JPEG then TGA
    try { return readPng(data, len, mr); } catch (...) {}
    try { return readBmp(data, len, mr); } catch (...) {}
    try { return readJpeg(data, len, mr); } catch (...) {}
    try { return readTga(data, len, mr); } catch (...) {}

    throw Error("imread: failed to decode image — unsupported or unknown format",
                0, 0, "imread", "", "numkit:imread:unknownFormat");
}

// Public/native entry: read file then decode from bytes.
Value imread(const std::string &path, std::pmr::memory_resource *mr) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        throw Error("imread: failed to load '" + path + "' — can't open",
                    0, 0, "imread", "", "numkit:imread:load");
    }
    std::string bytes((std::istreambuf_iterator<char>(f)),
                      std::istreambuf_iterator<char>());
    return imreadFromBytes(bytes, mr);
}

// Encode image Value to format bytes.
std::string imwriteToBytes(const Value &A, const std::string &ext,
                           std::pmr::memory_resource *mr)
{
    if (ext == "png") {
        return writePngToBytes(A, 6);
    }
    if (ext == "bmp") {
        return writeBmpToBytes(A);
    }
    if (ext == "tga") {
        return writeTgaToBytes(A);
    }
    if (ext == "jpg" || ext == "jpeg") {
        return writeJpegToBytes(A, 90);
    }
    if (ext == "pnm" || ext == "ppm" || ext == "pgm" || ext == "pbm") {
        return writePnmToBytes(A, ext);
    }
    if (ext == "tif" || ext == "tiff") {
        const auto buf = writeTiffToBytes(A, "none", nullptr);
        return std::string(buf.begin(), buf.end());
    }

    throw Error("imwrite: unsupported extension '" + ext +
                "' (supported: .png, .jpg, .bmp, .tga, .pnm, .tif)",
                0, 0, "imwrite", "", "numkit:imwrite:ext");
}

// Public/native entry: encode then write file.
void imwrite(const Value &A, const std::string &path, std::pmr::memory_resource *mr) {
    const std::string ext = lowerExt(path);
    if (ext == "tif" || ext == "tiff") {
        writeTiff(A, path, "none", /*appendMode=*/false);
        return;
    }
    const std::string bytes = imwriteToBytes(A, ext, mr);
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) {
        throw Error("imwrite: cannot open '" + path + "' for write",
                    0, 0, "imwrite", "", "numkit:imwrite:write");
    }
    f.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!f) {
        throw Error("imwrite: failed to write '" + path + "'",
                    0, 0, "imwrite", "", "numkit:imwrite:write");
    }
}

// Inspect image header metadata without decoding pixels.
Value imfinfoFromBytes(const std::string &bytes, const std::string &filename,
                       std::pmr::memory_resource *mr)
{
    const auto *data = reinterpret_cast<const std::uint8_t *>(bytes.data());
    const std::size_t len = bytes.size();

    std::uint32_t W = 0, H = 0;
    std::uint16_t bits = 8, channels = 1;
    std::string fmt = detectFormatBytes(bytes);
    if (fmt.empty()) fmt = lowerExt(filename);

    bool ok = false;
    if (fmt == "png") {
        ok = peekPng(data, len, W, H, bits, channels);
    } else if (fmt == "tif") {
        std::vector<std::uint8_t> buf(bytes.begin(), bytes.end());
        peekTiff(buf, W, H, bits, channels);
        ok = true;
    } else if (fmt == "jpg" || fmt == "jpeg") {
        ok = peekJpeg(data, len, W, H, bits, channels);
    } else if (fmt == "bmp") {
        ok = peekBmp(data, len, W, H, bits, channels);
    } else if (fmt == "pnm" || fmt == "ppm" || fmt == "pgm" || fmt == "pbm") {
        ok = peekPnm(data, len, W, H, bits, channels);
    } else if (fmt == "tga") {
        ok = peekTga(data, len, W, H, bits, channels);
    }

    if (!ok) {
        throw Error("imfinfo: failed to parse header for '" + filename + "'",
                    0, 0, "imfinfo", "", "numkit:imfinfo:read");
    }

    Value s = Value::structure(mr);
    s.field("Filename")          = Value::fromString(filename, mr);
    s.field("Format")            = Value::fromString(fmt, mr);
    s.field("Width")             = Value::scalar(double(W), mr);
    s.field("Height")            = Value::scalar(double(H), mr);
    s.field("NumberOfChannels")  = Value::scalar(double(channels), mr);
    s.field("BitDepth")          = Value::scalar(double(bits * channels), mr);
    s.field("ColorType")         = Value::fromString(colorTypeFromChannels(channels), mr);
    s.field("FileSize")          = Value::scalar(double(bytes.size()), mr);
    return s;
}

Value imfinfo(const std::string &path, std::pmr::memory_resource *mr) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        throw Error("imfinfo: failed to read '" + path + "' — can't open",
                    0, 0, "imfinfo", "", "numkit:imfinfo:read");
    }
    std::string bytes((std::istreambuf_iterator<char>(f)),
                      std::istreambuf_iterator<char>());
    return imfinfoFromBytes(bytes, path, mr);
}

} // namespace numkit::image
