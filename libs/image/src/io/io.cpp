// libs/image/src/io/io.cpp
//
// imread — decode an image from disk via stb_image. Output layout
// matches MATLAB's imread:
//   gray  → H×W   uint8
//   color → H×W×C uint8 (channels = R, G, B [, A])
// numkit stores arrays in column-major, so a stb pixel at
// (y, x, c) maps to linear index  c·H·W + x·H + y.

#include <numkit/image/io/io.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <stb_image.h>
#include <stb_image_write.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace numkit::image {

namespace {

std::string lowerExt(const std::string &path) {
    auto dot = path.find_last_of('.');
    if (dot == std::string::npos) return {};
    std::string e = path.substr(dot + 1);
    std::transform(e.begin(), e.end(), e.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return e;
}

} // anonymous

Value imread(std::pmr::memory_resource *mr, const std::string &path)
{
    int W = 0, H = 0, channelsInFile = 0;
    // 0 = take whatever channel count the file has (1, 3, or 4).
    unsigned char *pixels = stbi_load(path.c_str(), &W, &H, &channelsInFile, 0);
    if (!pixels) {
        const char *err = stbi_failure_reason();
        throw Error(std::string("imread: failed to load '") + path + "'" +
                    (err ? std::string(" — ") + err : std::string()),
                    0, 0, "imread", "", "m:imread:load");
    }
    int C = channelsInFile;
    if (C != 1 && C != 3 && C != 4) {
        stbi_image_free(pixels);
        throw Error("imread: unsupported channel count " + std::to_string(C),
                    0, 0, "imread", "", "m:imread:channels");
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

void imwrite(std::pmr::memory_resource * /*mr*/,
             const Value &A, const std::string &path)
{
    // Accept H×W or H×W×{1,3,4}. Read shape via Dims.
    const size_t H = A.dims().rows();
    const size_t W = A.dims().cols();
    int C = 1;
    if (A.numel() == H * W) {
        C = 1;
    } else if (A.numel() == H * W * 3) {
        C = 3;
    } else if (A.numel() == H * W * 4) {
        C = 4;
    } else {
        throw Error("imwrite: input must be H×W or H×W×{1,3,4}",
                    0, 0, "imwrite", "", "m:imwrite:shape");
    }

    // Convert numkit column-major (y, x, c) → stb row-major
    // interleaved RGB[A] (y * W * C + x * C + c).
    std::vector<unsigned char> buf(static_cast<size_t>(H) *
                                    static_cast<size_t>(W) *
                                    static_cast<size_t>(C));
    if (C == 1) {
        for (size_t y = 0; y < H; ++y)
            for (size_t x = 0; x < W; ++x) {
                const double v = A.elemAsDouble(x * H + y);
                int b = static_cast<int>(v);
                if (b < 0) b = 0; if (b > 255) b = 255;
                buf[y * W + x] = static_cast<unsigned char>(b);
            }
    } else {
        const size_t plane = H * W;
        for (size_t y = 0; y < H; ++y)
            for (size_t x = 0; x < W; ++x)
                for (int c = 0; c < C; ++c) {
                    const size_t srcIdx = static_cast<size_t>(c) * plane +
                                          x * H + y;
                    const double v = A.elemAsDouble(srcIdx);
                    int b = static_cast<int>(v);
                    if (b < 0) b = 0; if (b > 255) b = 255;
                    buf[(y * W + x) * static_cast<size_t>(C) +
                        static_cast<size_t>(c)] =
                        static_cast<unsigned char>(b);
                }
    }

    const std::string ext = lowerExt(path);
    int rc = 0;
    if (ext == "png") {
        rc = stbi_write_png(path.c_str(), static_cast<int>(W),
                            static_cast<int>(H), C, buf.data(),
                            static_cast<int>(W) * C);
    } else if (ext == "bmp") {
        rc = stbi_write_bmp(path.c_str(), static_cast<int>(W),
                            static_cast<int>(H), C, buf.data());
    } else if (ext == "tga") {
        rc = stbi_write_tga(path.c_str(), static_cast<int>(W),
                            static_cast<int>(H), C, buf.data());
    } else if (ext == "jpg" || ext == "jpeg") {
        // Quality 90 — close to MATLAB's default writer.
        rc = stbi_write_jpg(path.c_str(), static_cast<int>(W),
                            static_cast<int>(H), C, buf.data(), 90);
    } else {
        throw Error("imwrite: unsupported extension '" + ext +
                    "' (try .png / .bmp / .tga / .jpg)",
                    0, 0, "imwrite", "", "m:imwrite:ext");
    }
    if (!rc)
        throw Error("imwrite: failed to write '" + path + "'",
                    0, 0, "imwrite", "", "m:imwrite:write");
}

namespace {

// Sniff file format by inspecting the first ~12 bytes (magic numbers).
// Returns one of "png" / "jpg" / "bmp" / "gif" / "psd" / "pnm" / "hdr"
// / "tga" / "" (unknown).
std::string detectFormat(const std::string &path) {
    FILE *f = std::fopen(path.c_str(), "rb");
    if (!f) return {};
    unsigned char hdr[16] = {0};
    const size_t n = std::fread(hdr, 1, sizeof(hdr), f);
    std::fclose(f);
    if (n < 4) return {};

    // PNG: 89 50 4E 47 0D 0A 1A 0A
    if (n >= 8 && hdr[0] == 0x89 && hdr[1] == 'P' && hdr[2] == 'N' &&
        hdr[3] == 'G' && hdr[4] == 0x0D && hdr[5] == 0x0A)
        return "png";
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

Value imfinfo(std::pmr::memory_resource *mr, const std::string &path)
{
    int W = 0, H = 0, channels = 0;
    if (!stbi_info(path.c_str(), &W, &H, &channels)) {
        const char *err = stbi_failure_reason();
        throw Error(std::string("imfinfo: failed to read '") + path + "'" +
                    (err ? std::string(" — ") + err : std::string()),
                    0, 0, "imfinfo", "", "m:imfinfo:read");
    }

    std::string fmt = detectFormat(path);
    if (fmt.empty()) {
        const std::string ext = lowerExt(path);
        if (ext == "tga") fmt = "tga";
        else fmt = ext;   // best effort fallback
    }

    // File size via std::filesystem (C++17 — already required by numkit).
    long long fileSize = 0;
    std::error_code ec;
    fileSize = static_cast<long long>(std::filesystem::file_size(path, ec));
    if (ec) fileSize = 0;

    Value s = Value::structure(mr);
    s.field("Filename")          = Value::fromString(path, mr);
    s.field("Format")            = Value::fromString(fmt, mr);
    s.field("Width")             = Value::scalar(double(W), mr);
    s.field("Height")            = Value::scalar(double(H), mr);
    s.field("NumberOfChannels")  = Value::scalar(double(channels), mr);
    s.field("ColorType")         =
        Value::fromString(colorTypeFromChannels(channels), mr);
    s.field("FileSize")          = Value::scalar(double(fileSize), mr);
    return s;
}

namespace detail {

void imread_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                CallContext &ctx)
{
    if (args.empty())
        throw Error("imread: requires a path string",
                    0, 0, "imread", "", "m:imread:nargin");
    if (!args[0].isChar() && !args[0].isString())
        throw Error("imread: path must be a string",
                    0, 0, "imread", "", "m:imread:type");
    outs[0] = imread(ctx.engine->resource(), args[0].toString());
}

void imwrite_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> /*outs*/,
                 CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imwrite: requires (A, path)",
                    0, 0, "imwrite", "", "m:imwrite:nargin");
    if (!args[1].isChar() && !args[1].isString())
        throw Error("imwrite: path must be a string",
                    0, 0, "imwrite", "", "m:imwrite:type");
    imwrite(ctx.engine->resource(), args[0], args[1].toString());
}

void imfinfo_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                 CallContext &ctx)
{
    if (args.empty())
        throw Error("imfinfo: requires a path string",
                    0, 0, "imfinfo", "", "m:imfinfo:nargin");
    if (!args[0].isChar() && !args[0].isString())
        throw Error("imfinfo: path must be a string",
                    0, 0, "imfinfo", "", "m:imfinfo:type");
    outs[0] = imfinfo(ctx.engine->resource(), args[0].toString());
}

} // namespace detail

} // namespace numkit::image
