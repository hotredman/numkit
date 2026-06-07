// libs/image/src/io/io.cpp
//
// imread — decode an image from disk via stb_image. Output layout
// matches MATLAB's imread:
//   gray  → H×W   uint8
//   color → H×W×C uint8 (channels = R, G, B [, A])
// numkit stores arrays in column-major, so a stb pixel at
// (y, x, c) maps to linear index  c·H·W + x·H + y.

#include <numkit/image/io/io.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include "io_detail.hpp"

#include <stb_image.h>
#include <stb_image_write.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace numkit::image {


// Sniff TIFF magic directly from an in-memory buffer (no fopen).
bool isTiffBytes(const std::string &b)
{
    if (b.size() < 4) return false;
    const auto *h = reinterpret_cast<const unsigned char *>(b.data());
    if (h[0] == 'I' && h[1] == 'I' && h[3] == 0x00 && (h[2] == 0x2A || h[2] == 0x2B))
        return true;
    if (h[0] == 'M' && h[1] == 'M' && h[2] == 0x00 && (h[3] == 0x2A || h[3] == 0x2B))
        return true;
    return false;
}

// Decode an image from in-memory bytes. This is the single place pixels
// are produced; both the native path entry (imread) and the engine entry
// (imread_reg, which reads via the VFS) funnel through here so reads work
// on the virtual AND real filesystem — never a direct fopen.
Value imreadFromBytes(const std::string &bytes, std::pmr::memory_resource *mr)
{
    // TIFF route — stb_image doesn't decode TIFF, so dispatch to our
    // minimal in-tree reader (buffer overload).
    if (isTiffBytes(bytes))
        return readTiff(std::vector<std::uint8_t>(bytes.begin(), bytes.end()), 1u, mr);

    int W = 0, H = 0, channelsInFile = 0;
    // 0 = take whatever channel count the file has (1, 3, or 4).
    unsigned char *pixels = stbi_load_from_memory(
        reinterpret_cast<const stbi_uc *>(bytes.data()),
        static_cast<int>(bytes.size()), &W, &H, &channelsInFile, 0);
    if (!pixels) {
        const char *err = stbi_failure_reason();
        throw Error(std::string("imread: failed to decode image") +
                    (err ? std::string(" — ") + err : std::string()),
                    0, 0, "imread", "", "numkit:imread:load");
    }
    int C = channelsInFile;
    if (C != 1 && C != 3 && C != 4) {
        stbi_image_free(pixels);
        throw Error("imread: unsupported channel count " + std::to_string(C),
                    0, 0, "imread", "", "numkit:imread:channels");
    }

    Value out;
    if (C == 1) {
        out = Value::matrix(static_cast<size_t>(H), static_cast<size_t>(W),
                            ValueType::UINT8, mr);
        std::uint8_t *dst = out.uint8DataMut();
        // stb row-major (y * W + x) → numkit column-major (x * H + y).
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                dst[static_cast<size_t>(x) * static_cast<size_t>(H) +
                    static_cast<size_t>(y)] =
                    pixels[static_cast<size_t>(y) * static_cast<size_t>(W) +
                           static_cast<size_t>(x)];
    } else {
        out = Value::matrix3d(static_cast<size_t>(H),
                              static_cast<size_t>(W),
                              static_cast<size_t>(C),
                              ValueType::UINT8, mr);
        std::uint8_t *dst = out.uint8DataMut();
        // stb has channels interleaved at each pixel:
        //   pixels[(y*W + x)*C + c]
        // numkit column-major (y, x, c) layout:
        //   idx = c · H · W  +  x · H  +  y
        const size_t plane = static_cast<size_t>(H) *
                             static_cast<size_t>(W);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                const size_t srcBase = static_cast<size_t>(y * W + x) *
                                       static_cast<size_t>(C);
                for (int c = 0; c < C; ++c) {
                    const size_t dstIdx = static_cast<size_t>(c) * plane +
                                          static_cast<size_t>(x) *
                                              static_cast<size_t>(H) +
                                          static_cast<size_t>(y);
                    dst[dstIdx] = pixels[srcBase + static_cast<size_t>(c)];
                }
            }
    }

    stbi_image_free(pixels);
    return out;
}

// Public/native entry: read the whole file (real FS) then decode from the
// bytes. The engine entry (detail::imread_reg) instead reads via the VFS
// so it works on the IDE's virtual filesystem too.
Value imread(const std::string &path, std::pmr::memory_resource *mr)
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
        throw Error("imread: failed to load '" + path + "' — can't open",
                    0, 0, "imread", "", "numkit:imread:load");
    std::string bytes((std::istreambuf_iterator<char>(f)),
                      std::istreambuf_iterator<char>());
    return imreadFromBytes(bytes, mr);
}

namespace {

// stb write callback — appends encoded bytes to a std::string.
void appendToString(void *ctx, void *data, int size)
{
    auto *s = static_cast<std::string *>(ctx);
    s->append(static_cast<const char *>(data), static_cast<std::size_t>(size));
}

// Pack a numkit column-major (y, x, c) image into stb's row-major
// interleaved RGB[A] layout (y*W*C + x*C + c). Sets W, H, C; clamps to uint8.
std::vector<unsigned char> packForStb(const Value &A, int &W, int &H, int &C)
{
    const size_t Hs = A.dims().rows();
    const size_t Ws = A.dims().cols();
    int c = 1;
    if (A.numel() == Hs * Ws)          c = 1;
    else if (A.numel() == Hs * Ws * 3) c = 3;
    else if (A.numel() == Hs * Ws * 4) c = 4;
    else
        throw Error("imwrite: input must be H×W or H×W×{1,3,4}",
                    0, 0, "imwrite", "", "numkit:imwrite:shape");

    std::vector<unsigned char> buf(Hs * Ws * static_cast<size_t>(c));
    if (c == 1) {
        for (size_t y = 0; y < Hs; ++y)
            for (size_t x = 0; x < Ws; ++x) {
                int b = static_cast<int>(A.elemAsDouble(x * Hs + y));
                if (b < 0) b = 0; if (b > 255) b = 255;
                buf[y * Ws + x] = static_cast<unsigned char>(b);
            }
    } else {
        const size_t plane = Hs * Ws;
        for (size_t y = 0; y < Hs; ++y)
            for (size_t x = 0; x < Ws; ++x)
                for (int ch = 0; ch < c; ++ch) {
                    int b = static_cast<int>(
                        A.elemAsDouble(static_cast<size_t>(ch) * plane + x * Hs + y));
                    if (b < 0) b = 0; if (b > 255) b = 255;
                    buf[(y * Ws + x) * static_cast<size_t>(c) + static_cast<size_t>(ch)] =
                        static_cast<unsigned char>(b);
                }
    }
    W = static_cast<int>(Ws);
    H = static_cast<int>(Hs);
    C = c;
    return buf;
}

} // anonymous

std::string imwriteToBytes(const Value &A, const std::string &ext,
                           std::pmr::memory_resource * /*mr*/)
{
    int W = 0, H = 0, C = 0;
    const std::vector<unsigned char> buf = packForStb(A, W, H, C);

    std::string out;
    int rc = 0;
    if (ext == "png") {
        rc = stbi_write_png_to_func(appendToString, &out, W, H, C, buf.data(), W * C);
    } else if (ext == "bmp") {
        rc = stbi_write_bmp_to_func(appendToString, &out, W, H, C, buf.data());
    } else if (ext == "tga") {
        rc = stbi_write_tga_to_func(appendToString, &out, W, H, C, buf.data());
    } else if (ext == "jpg" || ext == "jpeg") {
        // Quality 90 — close to MATLAB's default writer.
        rc = stbi_write_jpg_to_func(appendToString, &out, W, H, C, buf.data(), 90);
    } else {
        throw Error("imwrite: unsupported extension '" + ext +
                    "' (try .png / .bmp / .tga / .jpg)",
                    0, 0, "imwrite", "", "numkit:imwrite:ext");
    }
    if (!rc)
        throw Error("imwrite: failed to encode '" + ext + "' image",
                    0, 0, "imwrite", "", "numkit:imwrite:write");
    return out;
}

// Public/native entry: encode then write the whole file (real FS). The
// engine entry (detail::imwrite_reg) instead writes via the VFS so it works
// on the IDE's virtual filesystem too.
void imwrite(const Value &A, const std::string &path, std::pmr::memory_resource *mr)
{
    const std::string ext = lowerExt(path);
    if (ext == "tif" || ext == "tiff") {
        writeTiff(A, path, "none", /*appendMode=*/false);
        return;
    }
    const std::string bytes = imwriteToBytes(A, ext, mr);
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f)
        throw Error("imwrite: cannot open '" + path + "' for write",
                    0, 0, "imwrite", "", "numkit:imwrite:write");
    f.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!f)
        throw Error("imwrite: failed to write '" + path + "'",
                    0, 0, "imwrite", "", "numkit:imwrite:write");
}

namespace {

// Sniff file format by inspecting the first ~12 bytes (magic numbers) of an
// in-memory buffer. Returns one of "png" / "jpg" / "bmp" / "gif" / "psd" /
// "pnm" / "hdr" / "tif" / "" (unknown).
std::string detectFormatBytes(const std::string &bytes) {
    unsigned char hdr[16] = {0};
    const size_t n = std::min<size_t>(bytes.size(), sizeof(hdr));
    for (size_t i = 0; i < n; ++i) hdr[i] = static_cast<unsigned char>(bytes[i]);
    if (n < 4) return {};

    // PNG: 89 50 4E 47 0D 0A 1A 0A
    if (n >= 8 && hdr[0] == 0x89 && hdr[1] == 'P' && hdr[2] == 'N' &&
        hdr[3] == 'G' && hdr[4] == 0x0D && hdr[5] == 0x0A)
        return "png";
    // TIFF (classic) little-endian: II + 0x2A 0x00; BigTIFF: II + 0x2B 0x00
    if (n >= 4 && hdr[0] == 'I' && hdr[1] == 'I' && hdr[3] == 0x00 &&
        (hdr[2] == 0x2A || hdr[2] == 0x2B))
        return "tif";
    // TIFF (classic) big-endian: MM + 0x00 0x2A; BigTIFF: MM + 0x00 0x2B
    if (n >= 4 && hdr[0] == 'M' && hdr[1] == 'M' && hdr[2] == 0x00 &&
        (hdr[3] == 0x2A || hdr[3] == 0x2B))
        return "tif";
    // JPEG: starts with FF D8 FF
    if (n >= 3 && hdr[0] == 0xFF && hdr[1] == 0xD8 && hdr[2] == 0xFF)
        return "jpg";
    // BMP: 'BM'
    if (hdr[0] == 'B' && hdr[1] == 'M') return "bmp";
    // GIF: 'GIF87a' / 'GIF89a'
    if (n >= 6 && hdr[0] == 'G' && hdr[1] == 'I' && hdr[2] == 'F' &&
        hdr[3] == '8' && (hdr[4] == '7' || hdr[4] == '9') && hdr[5] == 'a')
        return "gif";
    // PSD: '8BPS'
    if (n >= 4 && hdr[0] == '8' && hdr[1] == 'B' && hdr[2] == 'P' &&
        hdr[3] == 'S')
        return "psd";
    // PNM: 'P1' .. 'P6' (and 'P7' for PAM, 'PF' for HDR PFM).
    if (hdr[0] == 'P' && hdr[1] >= '1' && hdr[1] <= '7')
        return "pnm";
    // Radiance HDR: '#?RADIANCE' or '#?RGBE'
    if (n >= 8 && hdr[0] == '#' && hdr[1] == '?') return "hdr";
    // TGA has no magic; fall through to extension.
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

} // anonymous

Value imfinfoFromBytes(const std::string &bytes, const std::string &filename,
                       std::pmr::memory_resource *mr)
{
    int W = 0, H = 0, channels = 0;
    int bitsPerSample = 8;

    if (isTiffBytes(bytes)) {
        // TIFF route — stb doesn't peek TIFFs.
        std::vector<std::uint8_t> buf(bytes.begin(), bytes.end());
        std::uint32_t W32 = 0, H32 = 0;
        std::uint16_t bits = 8, chs = 1;
        peekTiff(buf, W32, H32, bits, chs);
        W = static_cast<int>(W32);
        H = static_cast<int>(H32);
        channels = static_cast<int>(chs);
        bitsPerSample = static_cast<int>(bits);
    } else if (!stbi_info_from_memory(
                   reinterpret_cast<const stbi_uc *>(bytes.data()),
                   static_cast<int>(bytes.size()), &W, &H, &channels)) {
        const char *err = stbi_failure_reason();
        throw Error(std::string("imfinfo: failed to read '") + filename + "'" +
                    (err ? std::string(" — ") + err : std::string()),
                    0, 0, "imfinfo", "", "numkit:imfinfo:read");
    }

    std::string fmt = detectFormatBytes(bytes);
    if (fmt.empty()) {
        const std::string ext = lowerExt(filename);
        if (ext == "tga") fmt = "tga";
        else fmt = ext;   // best effort fallback
    }

    Value s = Value::structure(mr);
    s.field("Filename")          = Value::fromString(filename, mr);
    s.field("Format")            = Value::fromString(fmt, mr);
    s.field("Width")             = Value::scalar(double(W), mr);
    s.field("Height")            = Value::scalar(double(H), mr);
    s.field("NumberOfChannels")  = Value::scalar(double(channels), mr);
    s.field("BitDepth")          = Value::scalar(double(bitsPerSample * channels), mr);
    s.field("ColorType")         =
        Value::fromString(colorTypeFromChannels(channels), mr);
    s.field("FileSize")          = Value::scalar(double(bytes.size()), mr);
    return s;
}

// Public/native entry: read the whole file (real FS) then peek the bytes.
// The engine entry (detail::imfinfo_reg) reads via the VFS instead.
Value imfinfo(const std::string &path, std::pmr::memory_resource *mr)
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
        throw Error("imfinfo: failed to read '" + path + "' — can't open",
                    0, 0, "imfinfo", "", "numkit:imfinfo:read");
    std::string bytes((std::istreambuf_iterator<char>(f)),
                      std::istreambuf_iterator<char>());
    return imfinfoFromBytes(bytes, path, mr);
}

} // namespace numkit::image
