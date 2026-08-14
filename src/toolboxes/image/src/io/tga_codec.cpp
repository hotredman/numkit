// toolboxes/image/src/io/tga_codec.cpp
//
// In-tree Truevision TGA (.tga) decoder and encoder. Zero external dependencies.

#include "tga_codec.hpp"
#include <numkit/value/error.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

namespace numkit::image {

namespace {

inline std::uint16_t readU16LE(const std::uint8_t *p) {
    return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}

inline void writeU16LE(std::vector<std::uint8_t> &buf, std::uint16_t v) {
    buf.push_back(static_cast<std::uint8_t>(v & 0xFF));
    buf.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
}

struct TgaHeader {
    std::uint8_t idLength = 0;
    std::uint8_t colorMapType = 0;
    std::uint8_t imageType = 0;
    std::uint16_t cMapStart = 0;
    std::uint16_t cMapLength = 0;
    std::uint8_t cMapDepth = 0;
    std::uint16_t xOrigin = 0;
    std::uint16_t yOrigin = 0;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::uint8_t pixelDepth = 0;
    std::uint8_t imageDesc = 0;
};

bool parseTgaHeader(const std::uint8_t *data, std::size_t len, TgaHeader &hdr) {
    if (len < 18) return false;

    hdr.idLength     = data[0];
    hdr.colorMapType = data[1];
    hdr.imageType    = data[2];
    hdr.cMapStart    = readU16LE(data + 3);
    hdr.cMapLength   = readU16LE(data + 5);
    hdr.cMapDepth    = data[7];
    hdr.xOrigin      = readU16LE(data + 8);
    hdr.yOrigin      = readU16LE(data + 10);
    hdr.width        = readU16LE(data + 12);
    hdr.height       = readU16LE(data + 14);
    hdr.pixelDepth   = data[16];
    hdr.imageDesc    = data[17];

    if (hdr.width == 0 || hdr.height == 0) return false;
    if (hdr.pixelDepth != 8 && hdr.pixelDepth != 16 && hdr.pixelDepth != 24 && hdr.pixelDepth != 32) {
        return false;
    }
    return true;
}

} // anonymous namespace

bool peekTga(const std::uint8_t *data, std::size_t len,
             std::uint32_t &W, std::uint32_t &H,
             std::uint16_t &bitsPerSample, std::uint16_t &channels)
{
    TgaHeader hdr;
    if (!parseTgaHeader(data, len, hdr)) return false;

    W = hdr.width;
    H = hdr.height;
    bitsPerSample = 8;

    if (hdr.imageType == 3 || hdr.imageType == 11) {
        channels = 1;
    } else if (hdr.pixelDepth == 24 || hdr.pixelDepth == 16) {
        channels = 3;
    } else if (hdr.pixelDepth == 32) {
        channels = 4;
    } else {
        channels = 1;
    }
    return true;
}

Value readTga(const std::uint8_t *data, std::size_t len, std::pmr::memory_resource *mr) {
    TgaHeader hdr;
    if (!parseTgaHeader(data, len, hdr)) {
        throw Error("imread: failed to decode TGA — invalid header",
                    0, 0, "imread", "", "numkit:imread:tgaHeader");
    }

    const std::size_t W = hdr.width;
    const std::size_t H = hdr.height;
    const int bytesPerPixel = hdr.pixelDepth / 8;
    const bool isRle = (hdr.imageType == 9 || hdr.imageType == 10 || hdr.imageType == 11);
    const bool isGray = (hdr.imageType == 3 || hdr.imageType == 11 || (hdr.imageType == 2 && bytesPerPixel == 1));
    const bool isTopDown = (hdr.imageDesc & 0x20) != 0;

    std::size_t offset = 18 + hdr.idLength;

    // Read color map if present (type 1 or 9)
    std::vector<std::array<std::uint8_t, 3>> palette;
    if (hdr.colorMapType == 1 && hdr.cMapLength > 0) {
        const int cMapBytes = hdr.cMapDepth / 8;
        if (len < offset + static_cast<std::size_t>(hdr.cMapLength) * cMapBytes) {
            throw Error("imread: TGA color map truncated",
                        0, 0, "imread", "", "numkit:imread:tgaColorMap");
        }
        palette.resize(hdr.cMapLength);
        for (std::size_t i = 0; i < hdr.cMapLength; ++i) {
            const std::size_t p = offset + i * cMapBytes;
            if (cMapBytes >= 3) {
                palette[i][2] = data[p];     // B
                palette[i][1] = data[p + 1]; // G
                palette[i][0] = data[p + 2]; // R
            }
        }
        offset += static_cast<std::size_t>(hdr.cMapLength) * cMapBytes;
    }

    const std::size_t totalPixels = W * H;
    std::vector<std::uint8_t> rawPixels;
    rawPixels.resize(totalPixels * bytesPerPixel);

    if (!isRle) {
        // Uncompressed pixels
        if (len < offset + totalPixels * bytesPerPixel) {
            throw Error("imread: TGA image data truncated",
                        0, 0, "imread", "", "numkit:imread:tgaTruncated");
        }
        std::memcpy(rawPixels.data(), data + offset, totalPixels * bytesPerPixel);
    } else {
        // RLE packet decoder
        std::size_t pixelIdx = 0;
        std::size_t p = offset;

        while (pixelIdx < totalPixels && p < len) {
            std::uint8_t h = data[p++];
            std::size_t count = (h & 0x7F) + 1;

            if (h & 0x80) {
                // RLE run packet
                if (p + bytesPerPixel > len) break;
                const std::uint8_t *px = data + p;
                p += bytesPerPixel;
                for (std::size_t k = 0; k < count && pixelIdx < totalPixels; ++k) {
                    for (int b = 0; b < bytesPerPixel; ++b) {
                        rawPixels[pixelIdx * bytesPerPixel + b] = px[b];
                    }
                    ++pixelIdx;
                }
            } else {
                // Raw packet
                if (p + count * bytesPerPixel > len) break;
                for (std::size_t k = 0; k < count && pixelIdx < totalPixels; ++k) {
                    for (int b = 0; b < bytesPerPixel; ++b) {
                        rawPixels[pixelIdx * bytesPerPixel + b] = data[p++];
                    }
                    ++pixelIdx;
                }
            }
        }

        if (pixelIdx < totalPixels) {
            throw Error("imread: TGA RLE stream ended prematurely",
                        0, 0, "imread", "", "numkit:imread:tgaRleEof");
        }
    }

    // Output to Value in column-major order
    if (isGray && bytesPerPixel == 1) {
        Value out = Value::matrix(H, W, ValueType::UINT8, mr);
        std::uint8_t *dst = out.uint8DataMut();
        for (std::size_t r = 0; r < H; ++r) {
            std::size_t srcY = isTopDown ? r : (H - 1 - r);
            const std::uint8_t *row = rawPixels.data() + srcY * W;
            for (std::size_t c = 0; c < W; ++c) {
                dst[c * H + r] = row[c];
            }
        }
        return out;
    }

    if (bytesPerPixel == 3) {
        Value out = Value::matrix3d(H, W, 3, ValueType::UINT8, mr);
        std::uint8_t *dst = out.uint8DataMut();
        const std::size_t plane = H * W;

        for (std::size_t r = 0; r < H; ++r) {
            std::size_t srcY = isTopDown ? r : (H - 1 - r);
            const std::uint8_t *row = rawPixels.data() + srcY * W * 3;
            for (std::size_t c = 0; c < W; ++c) {
                dst[c * H + r]             = row[c * 3 + 2]; // Red
                dst[plane + c * H + r]     = row[c * 3 + 1]; // Green
                dst[2 * plane + c * H + r] = row[c * 3];     // Blue
            }
        }
        return out;
    }

    if (bytesPerPixel == 4) {
        Value out = Value::matrix3d(H, W, 4, ValueType::UINT8, mr);
        std::uint8_t *dst = out.uint8DataMut();
        const std::size_t plane = H * W;

        for (std::size_t r = 0; r < H; ++r) {
            std::size_t srcY = isTopDown ? r : (H - 1 - r);
            const std::uint8_t *row = rawPixels.data() + srcY * W * 4;
            for (std::size_t c = 0; c < W; ++c) {
                dst[c * H + r]             = row[c * 4 + 2]; // Red
                dst[plane + c * H + r]     = row[c * 4 + 1]; // Green
                dst[2 * plane + c * H + r] = row[c * 4];     // Blue
                dst[3 * plane + c * H + r] = row[c * 4 + 3]; // Alpha
            }
        }
        return out;
    }

    throw Error("imread: unsupported TGA pixel format (" + std::to_string(bytesPerPixel) + " bytes/pixel)",
                0, 0, "imread", "", "numkit:imread:tgaFormat");
}

std::string writeTgaToBytes(const Value &A) {
    const std::size_t H = A.dims().rows();
    const std::size_t W = A.dims().cols();
    if (H == 0 || W == 0) {
        throw Error("imwrite: empty image matrix",
                    0, 0, "imwrite", "", "numkit:imwrite:empty");
    }

    int channels = 1;
    if (A.numel() == H * W)          channels = 1;
    else if (A.numel() == H * W * 3) channels = 3;
    else if (A.numel() == H * W * 4) channels = 4;
    else {
        throw Error("imwrite: input must be H×W or H×W×{1,3,4}",
                    0, 0, "imwrite", "", "numkit:imwrite:shape");
    }

    const std::uint8_t imageType = (channels == 1) ? 3 : 2; // Uncompressed Grayscale or Truecolor
    const std::uint8_t pixelDepth = static_cast<std::uint8_t>(channels * 8);

    std::vector<std::uint8_t> buf;
    buf.reserve(18 + W * H * channels);

    // 18-byte TGA Header (top-left origin descriptor bit 5 = 1 -> 0x20)
    buf.push_back(0);                              // idLength
    buf.push_back(0);                              // colorMapType
    buf.push_back(imageType);                      // imageType
    writeU16LE(buf, 0);                            // cMapStart
    writeU16LE(buf, 0);                            // cMapLength
    buf.push_back(0);                              // cMapDepth
    writeU16LE(buf, 0);                            // xOrigin
    writeU16LE(buf, 0);                            // yOrigin
    writeU16LE(buf, static_cast<std::uint16_t>(W)); // width
    writeU16LE(buf, static_cast<std::uint16_t>(H)); // height
    buf.push_back(pixelDepth);                     // pixelDepth
    buf.push_back(0x20);                           // imageDesc (top-left origin)

    // Write pixels (row by row from top y=0 to H-1)
    const std::size_t plane = H * W;

    for (std::size_t y = 0; y < H; ++y) {
        if (channels == 1) {
            for (std::size_t x = 0; x < W; ++x) {
                int b = static_cast<int>(A.elemAsDouble(x * H + y));
                if (b < 0) b = 0; else if (b > 255) b = 255;
                buf.push_back(static_cast<std::uint8_t>(b));
            }
        } else if (channels == 3) {
            for (std::size_t x = 0; x < W; ++x) {
                int red   = static_cast<int>(A.elemAsDouble(x * H + y));
                int green = static_cast<int>(A.elemAsDouble(plane + x * H + y));
                int blue  = static_cast<int>(A.elemAsDouble(2 * plane + x * H + y));
                if (red < 0) red = 0; else if (red > 255) red = 255;
                if (green < 0) green = 0; else if (green > 255) green = 255;
                if (blue < 0) blue = 0; else if (blue > 255) blue = 255;

                buf.push_back(static_cast<std::uint8_t>(blue));
                buf.push_back(static_cast<std::uint8_t>(green));
                buf.push_back(static_cast<std::uint8_t>(red));
            }
        } else if (channels == 4) {
            for (std::size_t x = 0; x < W; ++x) {
                int red   = static_cast<int>(A.elemAsDouble(x * H + y));
                int green = static_cast<int>(A.elemAsDouble(plane + x * H + y));
                int blue  = static_cast<int>(A.elemAsDouble(2 * plane + x * H + y));
                int alpha = static_cast<int>(A.elemAsDouble(3 * plane + x * H + y));
                if (red < 0) red = 0; else if (red > 255) red = 255;
                if (green < 0) green = 0; else if (green > 255) green = 255;
                if (blue < 0) blue = 0; else if (blue > 255) blue = 255;
                if (alpha < 0) alpha = 0; else if (alpha > 255) alpha = 255;

                buf.push_back(static_cast<std::uint8_t>(blue));
                buf.push_back(static_cast<std::uint8_t>(green));
                buf.push_back(static_cast<std::uint8_t>(red));
                buf.push_back(static_cast<std::uint8_t>(alpha));
            }
        }
    }

    return std::string(buf.begin(), buf.end());
}

} // namespace numkit::image
