// toolboxes/image/src/io/bmp_codec.cpp
//
// In-tree Windows Bitmap (.bmp) decoder and encoder. Zero external dependencies.

#include "bmp_codec.hpp"
#include <numkit/value/error.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace numkit::image {

namespace {

inline std::uint16_t readU16LE(const std::uint8_t *p) {
    return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}

inline std::uint32_t readU32LE(const std::uint8_t *p) {
    return static_cast<std::uint32_t>(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

inline std::int32_t readI32LE(const std::uint8_t *p) {
    return static_cast<std::int32_t>(readU32LE(p));
}

inline void writeU16LE(std::vector<std::uint8_t> &buf, std::uint16_t v) {
    buf.push_back(static_cast<std::uint8_t>(v & 0xFF));
    buf.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
}

inline void writeU32LE(std::vector<std::uint8_t> &buf, std::uint32_t v) {
    buf.push_back(static_cast<std::uint8_t>(v & 0xFF));
    buf.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    buf.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
    buf.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
}

struct BmpHeader {
    std::uint32_t fileSize = 0;
    std::uint32_t offBits = 0;
    std::uint32_t headerSize = 0;
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::uint16_t bitCount = 0;
    std::uint32_t compression = 0;
    std::uint32_t numColors = 0;
    bool topDown = false;
};

bool parseBmpHeader(const std::uint8_t *data, std::size_t len, BmpHeader &hdr) {
    if (len < 14 + 12) return false;
    if (data[0] != 'B' || data[1] != 'M') return false;

    hdr.fileSize = readU32LE(data + 2);
    hdr.offBits = readU32LE(data + 10);
    hdr.headerSize = readU32LE(data + 14);

    if (hdr.headerSize < 12 || len < 14 + hdr.headerSize) return false;

    if (hdr.headerSize == 12) {
        // OS/2 BITMAPCOREHEADER
        hdr.width = readU16LE(data + 18);
        hdr.height = readU16LE(data + 20);
        hdr.bitCount = readU16LE(data + 24);
        hdr.compression = 0;
        hdr.numColors = 0;
        hdr.topDown = false;
    } else {
        // BITMAPINFOHEADER and newer
        if (hdr.headerSize < 40) return false;
        hdr.width = readI32LE(data + 18);
        hdr.height = readI32LE(data + 22);
        hdr.bitCount = readU16LE(data + 28);
        hdr.compression = readU32LE(data + 30);
        hdr.numColors = readU32LE(data + 46);
        hdr.topDown = (hdr.height < 0);
        if (hdr.height < 0) hdr.height = -hdr.height;
    }

    if (hdr.width <= 0 || hdr.height <= 0) return false;
    if (hdr.bitCount != 1 && hdr.bitCount != 4 && hdr.bitCount != 8 &&
        hdr.bitCount != 16 && hdr.bitCount != 24 && hdr.bitCount != 32) return false;

    return true;
}

} // anonymous namespace

bool peekBmp(const std::uint8_t *data, std::size_t len,
             std::uint32_t &W, std::uint32_t &H,
             std::uint16_t &bitsPerSample, std::uint16_t &channels)
{
    BmpHeader hdr;
    if (!parseBmpHeader(data, len, hdr)) return false;

    W = static_cast<std::uint32_t>(hdr.width);
    H = static_cast<std::uint32_t>(hdr.height);
    bitsPerSample = 8;

    if (hdr.bitCount <= 8) {
        // For palette/indexed BMP, report 1 channel (or 3 if non-gray)
        channels = 1;
    } else if (hdr.bitCount == 24 || hdr.bitCount == 16) {
        channels = 3;
    } else if (hdr.bitCount == 32) {
        channels = 4;
    } else {
        channels = 1;
    }
    return true;
}

Value readBmp(const std::uint8_t *data, std::size_t len, std::pmr::memory_resource *mr) {
    BmpHeader hdr;
    if (!parseBmpHeader(data, len, hdr)) {
        throw Error("imread: failed to decode BMP — invalid or corrupted header",
                    0, 0, "imread", "", "numkit:imread:bmpHeader");
    }

    const std::size_t W = static_cast<std::size_t>(hdr.width);
    const std::size_t H = static_cast<std::size_t>(hdr.height);

    // Read palette if present
    std::vector<std::array<std::uint8_t, 3>> palette;
    if (hdr.bitCount <= 8) {
        std::size_t palSize = hdr.numColors;
        if (palSize == 0) palSize = (1u << hdr.bitCount);
        const std::size_t palOff = 14 + hdr.headerSize;
        const std::size_t entryBytes = (hdr.headerSize == 12) ? 3 : 4;

        if (len < palOff + palSize * entryBytes) {
            throw Error("imread: BMP palette truncated",
                        0, 0, "imread", "", "numkit:imread:bmpPalette");
        }

        palette.resize(palSize);
        for (std::size_t i = 0; i < palSize; ++i) {
            const std::size_t p = palOff + i * entryBytes;
            palette[i][2] = data[p];     // Blue
            palette[i][1] = data[p + 1]; // Green
            palette[i][0] = data[p + 2]; // Red
        }
    }

    // Check if palette is pure grayscale identity (0..255)
    bool isPaletteGray = false;
    if (hdr.bitCount == 8 && palette.size() == 256) {
        isPaletteGray = true;
        for (std::size_t i = 0; i < 256; ++i) {
            if (palette[i][0] != i || palette[i][1] != i || palette[i][2] != i) {
                isPaletteGray = false;
                break;
            }
        }
    }

    if (len < hdr.offBits) {
        throw Error("imread: BMP pixel offset out of bounds",
                    0, 0, "imread", "", "numkit:imread:bmpOffset");
    }

    const std::uint8_t *pixelData = data + hdr.offBits;
    const std::size_t pixelDataLen = len - hdr.offBits;

    if (hdr.compression == 1 && hdr.bitCount == 8) {
        // BI_RLE8 decoding
        std::vector<std::uint8_t> uncompressed(W * H, 0);
        std::size_t x = 0;
        std::size_t y = 0;
        std::size_t p = 0;

        while (p + 1 < pixelDataLen && y < H) {
            std::uint8_t count = pixelData[p++];
            std::uint8_t val = pixelData[p++];

            if (count > 0) {
                for (std::size_t k = 0; k < count && x < W; ++k) {
                    uncompressed[y * W + (x++)] = val;
                }
            } else {
                if (val == 0) {
                    // End of line
                    x = 0;
                    ++y;
                } else if (val == 1) {
                    // End of bitmap
                    break;
                } else if (val == 2) {
                    // Delta escape
                    if (p + 1 < pixelDataLen) {
                        x += pixelData[p++];
                        y += pixelData[p++];
                    }
                } else {
                    // Literal run of `val` bytes
                    std::size_t run = val;
                    for (std::size_t k = 0; k < run && p < pixelDataLen && x < W; ++k) {
                        uncompressed[y * W + (x++)] = pixelData[p++];
                    }
                    if (run % 2 == 1 && p < pixelDataLen) ++p; // 2-byte alignment
                }
            }
        }

        // Return 1-channel or 3-channel
        if (isPaletteGray) {
            Value out = Value::matrix(H, W, ValueType::UINT8, mr);
            std::uint8_t *dst = out.uint8DataMut();
            for (std::size_t r = 0; r < H; ++r) {
                std::size_t srcY = hdr.topDown ? r : (H - 1 - r);
                for (std::size_t c = 0; c < W; ++c) {
                    dst[c * H + r] = uncompressed[srcY * W + c];
                }
            }
            return out;
        } else {
            Value out = Value::matrix3d(H, W, 3, ValueType::UINT8, mr);
            std::uint8_t *dst = out.uint8DataMut();
            const std::size_t plane = H * W;
            for (std::size_t r = 0; r < H; ++r) {
                std::size_t srcY = hdr.topDown ? r : (H - 1 - r);
                for (std::size_t c = 0; c < W; ++c) {
                    std::uint8_t idx = uncompressed[srcY * W + c];
                    const auto &rgb = (idx < palette.size()) ? palette[idx] : std::array<std::uint8_t, 3>{0, 0, 0};
                    dst[c * H + r]             = rgb[0];
                    dst[plane + c * H + r]     = rgb[1];
                    dst[2 * plane + c * H + r] = rgb[2];
                }
            }
            return out;
        }
    }

    if (hdr.compression != 0 && hdr.compression != 3) {
        throw Error("imread: unsupported BMP compression " + std::to_string(hdr.compression),
                    0, 0, "imread", "", "numkit:imread:bmpComp");
    }

    // Uncompressed BMP row stride aligned to 4 bytes
    const std::size_t rowStride = ((W * hdr.bitCount + 31) / 32) * 4;
    if (pixelDataLen < rowStride * H) {
        throw Error("imread: truncated BMP pixel buffer",
                    0, 0, "imread", "", "numkit:imread:bmpTruncated");
    }

    // 8-bit Grayscale
    if (hdr.bitCount == 8 && isPaletteGray) {
        Value out = Value::matrix(H, W, ValueType::UINT8, mr);
        std::uint8_t *dst = out.uint8DataMut();
        for (std::size_t r = 0; r < H; ++r) {
            std::size_t srcY = hdr.topDown ? r : (H - 1 - r);
            const std::uint8_t *row = pixelData + srcY * rowStride;
            for (std::size_t c = 0; c < W; ++c) {
                dst[c * H + r] = row[c];
            }
        }
        return out;
    }

    // 1-bit, 4-bit, 8-bit with color palette -> RGB
    if (hdr.bitCount <= 8) {
        Value out = Value::matrix3d(H, W, 3, ValueType::UINT8, mr);
        std::uint8_t *dst = out.uint8DataMut();
        const std::size_t plane = H * W;

        for (std::size_t r = 0; r < H; ++r) {
            std::size_t srcY = hdr.topDown ? r : (H - 1 - r);
            const std::uint8_t *row = pixelData + srcY * rowStride;

            for (std::size_t c = 0; c < W; ++c) {
                std::uint8_t idx = 0;
                if (hdr.bitCount == 8) {
                    idx = row[c];
                } else if (hdr.bitCount == 4) {
                    std::uint8_t byteVal = row[c / 2];
                    idx = (c % 2 == 0) ? (byteVal >> 4) : (byteVal & 0x0F);
                } else if (hdr.bitCount == 1) {
                    std::uint8_t byteVal = row[c / 8];
                    idx = (byteVal >> (7 - (c % 8))) & 1;
                }

                const auto &rgb = (idx < palette.size()) ? palette[idx] : std::array<std::uint8_t, 3>{0, 0, 0};
                dst[c * H + r]             = rgb[0];
                dst[plane + c * H + r]     = rgb[1];
                dst[2 * plane + c * H + r] = rgb[2];
            }
        }
        return out;
    }

    // 24-bit BGR
    if (hdr.bitCount == 24) {
        Value out = Value::matrix3d(H, W, 3, ValueType::UINT8, mr);
        std::uint8_t *dst = out.uint8DataMut();
        const std::size_t plane = H * W;

        for (std::size_t r = 0; r < H; ++r) {
            std::size_t srcY = hdr.topDown ? r : (H - 1 - r);
            const std::uint8_t *row = pixelData + srcY * rowStride;
            for (std::size_t c = 0; c < W; ++c) {
                dst[c * H + r]             = row[c * 3 + 2]; // Red
                dst[plane + c * H + r]     = row[c * 3 + 1]; // Green
                dst[2 * plane + c * H + r] = row[c * 3];     // Blue
            }
        }
        return out;
    }

    // 32-bit BGRA
    if (hdr.bitCount == 32) {
        Value out = Value::matrix3d(H, W, 4, ValueType::UINT8, mr);
        std::uint8_t *dst = out.uint8DataMut();
        const std::size_t plane = H * W;

        for (std::size_t r = 0; r < H; ++r) {
            std::size_t srcY = hdr.topDown ? r : (H - 1 - r);
            const std::uint8_t *row = pixelData + srcY * rowStride;
            for (std::size_t c = 0; c < W; ++c) {
                dst[c * H + r]             = row[c * 4 + 2]; // Red
                dst[plane + c * H + r]     = row[c * 4 + 1]; // Green
                dst[2 * plane + c * H + r] = row[c * 4];     // Blue
                dst[3 * plane + c * H + r] = row[c * 4 + 3]; // Alpha
            }
        }
        return out;
    }

    // 16-bit RGB555 / RGB565 fallback
    if (hdr.bitCount == 16) {
        Value out = Value::matrix3d(H, W, 3, ValueType::UINT8, mr);
        std::uint8_t *dst = out.uint8DataMut();
        const std::size_t plane = H * W;

        for (std::size_t r = 0; r < H; ++r) {
            std::size_t srcY = hdr.topDown ? r : (H - 1 - r);
            const std::uint8_t *row = pixelData + srcY * rowStride;
            for (std::size_t c = 0; c < W; ++c) {
                std::uint16_t v = readU16LE(row + c * 2);
                // Assume standard RGB555
                std::uint8_t red   = static_cast<std::uint8_t>(((v >> 10) & 0x1F) * 255 / 31);
                std::uint8_t green = static_cast<std::uint8_t>(((v >> 5)  & 0x1F) * 255 / 31);
                std::uint8_t blue  = static_cast<std::uint8_t>((v & 0x1F) * 255 / 31);

                dst[c * H + r]             = red;
                dst[plane + c * H + r]     = green;
                dst[2 * plane + c * H + r] = blue;
            }
        }
        return out;
    }

    throw Error("imread: unsupported BMP bit depth " + std::to_string(hdr.bitCount),
                0, 0, "imread", "", "numkit:imread:bmpBits");
}

std::string writeBmpToBytes(const Value &A) {
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

    const std::uint16_t bitCount = (channels == 1) ? 8 : ((channels == 3) ? 24 : 32);
    const std::size_t rowStride = ((W * bitCount + 31) / 32) * 4;
    const std::size_t palBytes = (channels == 1) ? 1024 : 0;
    const std::size_t offBits = 14 + 40 + palBytes;
    const std::size_t imageSize = rowStride * H;
    const std::size_t fileSize = offBits + imageSize;

    std::vector<std::uint8_t> buf;
    buf.reserve(fileSize);

    // 1. BITMAPFILEHEADER (14 bytes)
    buf.push_back('B');
    buf.push_back('M');
    writeU32LE(buf, static_cast<std::uint32_t>(fileSize));
    writeU16LE(buf, 0); // reserved1
    writeU16LE(buf, 0); // reserved2
    writeU32LE(buf, static_cast<std::uint32_t>(offBits));

    // 2. BITMAPINFOHEADER (40 bytes)
    writeU32LE(buf, 40);                                     // biSize
    writeU32LE(buf, static_cast<std::uint32_t>(W));          // biWidth
    writeU32LE(buf, static_cast<std::uint32_t>(H));          // biHeight (positive = bottom-up)
    writeU16LE(buf, 1);                                      // biPlanes
    writeU16LE(buf, bitCount);                               // biBitCount
    writeU32LE(buf, 0);                                      // biCompression (BI_RGB)
    writeU32LE(buf, static_cast<std::uint32_t>(imageSize));  // biSizeImage
    writeU32LE(buf, 2835);                                   // biXPelsPerMeter (~72 DPI)
    writeU32LE(buf, 2835);                                   // biYPelsPerMeter
    writeU32LE(buf, (channels == 1) ? 256 : 0);              // biClrUsed
    writeU32LE(buf, 0);                                      // biClrImportant

    // 3. Palette (for 8-bit grayscale)
    if (channels == 1) {
        for (int i = 0; i < 256; ++i) {
            buf.push_back(static_cast<std::uint8_t>(i)); // B
            buf.push_back(static_cast<std::uint8_t>(i)); // G
            buf.push_back(static_cast<std::uint8_t>(i)); // R
            buf.push_back(0);                            // Reserved
        }
    }

    // 4. Pixel data (bottom-up: row H-1 down to 0)
    std::vector<std::uint8_t> rowBuf(rowStride, 0);
    const std::size_t plane = H * W;

    for (std::int64_t r = static_cast<std::int64_t>(H) - 1; r >= 0; --r) {
        std::fill(rowBuf.begin(), rowBuf.end(), 0);
        std::size_t y = static_cast<std::size_t>(r);

        if (channels == 1) {
            for (std::size_t x = 0; x < W; ++x) {
                int b = static_cast<int>(A.elemAsDouble(x * H + y));
                if (b < 0) b = 0; else if (b > 255) b = 255;
                rowBuf[x] = static_cast<std::uint8_t>(b);
            }
        } else if (channels == 3) {
            for (std::size_t x = 0; x < W; ++x) {
                int red   = static_cast<int>(A.elemAsDouble(x * H + y));
                int green = static_cast<int>(A.elemAsDouble(plane + x * H + y));
                int blue  = static_cast<int>(A.elemAsDouble(2 * plane + x * H + y));
                if (red < 0) red = 0; else if (red > 255) red = 255;
                if (green < 0) green = 0; else if (green > 255) green = 255;
                if (blue < 0) blue = 0; else if (blue > 255) blue = 255;

                rowBuf[x * 3]     = static_cast<std::uint8_t>(blue);
                rowBuf[x * 3 + 1] = static_cast<std::uint8_t>(green);
                rowBuf[x * 3 + 2] = static_cast<std::uint8_t>(red);
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

                rowBuf[x * 4]     = static_cast<std::uint8_t>(blue);
                rowBuf[x * 4 + 1] = static_cast<std::uint8_t>(green);
                rowBuf[x * 4 + 2] = static_cast<std::uint8_t>(red);
                rowBuf[x * 4 + 3] = static_cast<std::uint8_t>(alpha);
            }
        }
        buf.insert(buf.end(), rowBuf.begin(), rowBuf.end());
    }

    return std::string(buf.begin(), buf.end());
}

} // namespace numkit::image
