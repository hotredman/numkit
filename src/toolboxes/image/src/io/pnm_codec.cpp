// toolboxes/image/src/io/pnm_codec.cpp
//
// In-tree Netpbm (.pnm, .ppm, .pgm, .pbm) decoder and encoder. Zero external dependencies.

#include "pnm_codec.hpp"
#include <numkit/value/error.hpp>

#include <cctype>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace numkit::image {

namespace {

// Helper to skip whitespace and comments in PNM header
std::size_t skipWhitespaceAndComments(const std::uint8_t *data, std::size_t len, std::size_t p) {
    while (p < len) {
        if (std::isspace(data[p])) {
            ++p;
        } else if (data[p] == '#') {
            while (p < len && data[p] != '\n' && data[p] != '\r') {
                ++p;
            }
        } else {
            break;
        }
    }
    return p;
}

// Read next integer token from ASCII PNM stream
bool readNextInt(const std::uint8_t *data, std::size_t len, std::size_t &p, std::uint32_t &val) {
    p = skipWhitespaceAndComments(data, len, p);
    if (p >= len || !std::isdigit(data[p])) return false;

    val = 0;
    while (p < len && std::isdigit(data[p])) {
        val = val * 10 + (data[p] - '0');
        ++p;
    }
    return true;
}

struct PnmHeader {
    int magic = 0; // 1..6
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t maxval = 255;
    std::size_t dataOffset = 0;
};

bool parsePnmHeader(const std::uint8_t *data, std::size_t len, PnmHeader &hdr) {
    if (len < 3) return false;
    if (data[0] != 'P' || data[1] < '1' || data[1] > '6') return false;

    hdr.magic = data[1] - '0';
    std::size_t p = 2;

    if (!readNextInt(data, len, p, hdr.width)) return false;
    if (!readNextInt(data, len, p, hdr.height)) return false;

    if (hdr.magic != 1 && hdr.magic != 4) {
        // PGM and PPM have maxval
        if (!readNextInt(data, len, p, hdr.maxval)) return false;
        if (hdr.maxval == 0 || hdr.maxval > 65535) return false;
    } else {
        hdr.maxval = 1;
    }

    // Skip single whitespace character after maxval (or comments)
    if (p < len && (std::isspace(data[p]) || data[p] == '#')) {
        p = skipWhitespaceAndComments(data, len, p);
    }
    hdr.dataOffset = p;

    return (hdr.width > 0 && hdr.height > 0);
}

} // anonymous namespace

bool peekPnm(const std::uint8_t *data, std::size_t len,
             std::uint32_t &W, std::uint32_t &H,
             std::uint16_t &bitsPerSample, std::uint16_t &channels)
{
    PnmHeader hdr;
    if (!parsePnmHeader(data, len, hdr)) return false;

    W = hdr.width;
    H = hdr.height;
    bitsPerSample = (hdr.maxval > 255) ? 16 : 8;

    if (hdr.magic == 1 || hdr.magic == 2 || hdr.magic == 4 || hdr.magic == 5) {
        channels = 1;
    } else {
        channels = 3;
    }
    return true;
}

Value readPnm(const std::uint8_t *data, std::size_t len, std::pmr::memory_resource *mr) {
    PnmHeader hdr;
    if (!parsePnmHeader(data, len, hdr)) {
        throw Error("imread: failed to decode PNM — invalid header",
                    0, 0, "imread", "", "numkit:imread:pnmHeader");
    }

    const std::size_t W = hdr.width;
    const std::size_t H = hdr.height;
    const bool is16Bit = (hdr.maxval > 255);
    const bool isColor = (hdr.magic == 3 || hdr.magic == 6);
    const bool isBinary = (hdr.magic == 4 || hdr.magic == 5 || hdr.magic == 6);

    // 1. Binary Formats (P4, P5, P6)
    if (isBinary) {
        if (hdr.magic == 4) {
            // P4: PBM Binary 1-bit (MSB-first)
            const std::size_t rowBytes = (W + 7) / 8;
            if (len < hdr.dataOffset + rowBytes * H) {
                throw Error("imread: truncated PBM binary data",
                            0, 0, "imread", "", "numkit:imread:pbmTruncated");
            }
            Value out = Value::matrix(H, W, ValueType::UINT8, mr);
            std::uint8_t *dst = out.uint8DataMut();

            for (std::size_t r = 0; r < H; ++r) {
                const std::uint8_t *row = data + hdr.dataOffset + r * rowBytes;
                for (std::size_t c = 0; c < W; ++c) {
                    // In PBM, 1 is black (0) and 0 is white (255) or logical 0/1
                    std::uint8_t bit = (row[c / 8] >> (7 - (c % 8))) & 1;
                    dst[c * H + r] = bit ? 0 : 255;
                }
            }
            return out;
        }

        if (hdr.magic == 5) {
            // P5: PGM Binary Grayscale (8-bit or 16-bit)
            const std::size_t bytesPerSample = is16Bit ? 2 : 1;
            if (len < hdr.dataOffset + W * H * bytesPerSample) {
                throw Error("imread: truncated PGM binary data",
                            0, 0, "imread", "", "numkit:imread:pgmTruncated");
            }
            const std::uint8_t *pixels = data + hdr.dataOffset;

            if (!is16Bit) {
                Value out = Value::matrix(H, W, ValueType::UINT8, mr);
                std::uint8_t *dst = out.uint8DataMut();
                for (std::size_t r = 0; r < H; ++r) {
                    for (std::size_t c = 0; c < W; ++c) {
                        dst[c * H + r] = pixels[r * W + c];
                    }
                }
                return out;
            } else {
                Value out = Value::matrix(H, W, ValueType::UINT16, mr);
                std::uint16_t *dst = out.uint16DataMut();
                for (std::size_t r = 0; r < H; ++r) {
                    for (std::size_t c = 0; c < W; ++c) {
                        std::size_t idx = (r * W + c) * 2;
                        std::uint16_t v = static_cast<std::uint16_t>((pixels[idx] << 8) | pixels[idx + 1]);
                        dst[c * H + r] = v;
                    }
                }
                return out;
            }
        }

        if (hdr.magic == 6) {
            // P6: PPM Binary RGB (8-bit or 16-bit)
            const std::size_t bytesPerSample = is16Bit ? 2 : 1;
            if (len < hdr.dataOffset + W * H * 3 * bytesPerSample) {
                throw Error("imread: truncated PPM binary data",
                            0, 0, "imread", "", "numkit:imread:ppmTruncated");
            }
            const std::uint8_t *pixels = data + hdr.dataOffset;
            const std::size_t plane = H * W;

            if (!is16Bit) {
                Value out = Value::matrix3d(H, W, 3, ValueType::UINT8, mr);
                std::uint8_t *dst = out.uint8DataMut();
                for (std::size_t r = 0; r < H; ++r) {
                    for (std::size_t c = 0; c < W; ++c) {
                        std::size_t idx = (r * W + c) * 3;
                        dst[c * H + r]             = pixels[idx];     // R
                        dst[plane + c * H + r]     = pixels[idx + 1]; // G
                        dst[2 * plane + c * H + r] = pixels[idx + 2]; // B
                    }
                }
                return out;
            } else {
                Value out = Value::matrix3d(H, W, 3, ValueType::UINT16, mr);
                std::uint16_t *dst = out.uint16DataMut();
                for (std::size_t r = 0; r < H; ++r) {
                    for (std::size_t c = 0; c < W; ++c) {
                        std::size_t idx = (r * W + c) * 6;
                        std::uint16_t red   = static_cast<std::uint16_t>((pixels[idx] << 8) | pixels[idx + 1]);
                        std::uint16_t green = static_cast<std::uint16_t>((pixels[idx + 2] << 8) | pixels[idx + 3]);
                        std::uint16_t blue  = static_cast<std::uint16_t>((pixels[idx + 4] << 8) | pixels[idx + 5]);

                        dst[c * H + r]             = red;
                        dst[plane + c * H + r]     = green;
                        dst[2 * plane + c * H + r] = blue;
                    }
                }
                return out;
            }
        }
    }

    // 2. ASCII Formats (P1, P2, P3)
    std::size_t p = hdr.dataOffset;
    if (!isColor) {
        // Grayscale / Monochrome ASCII
        Value out = is16Bit ? Value::matrix(H, W, ValueType::UINT16, mr)
                            : Value::matrix(H, W, ValueType::UINT8, mr);
        for (std::size_t r = 0; r < H; ++r) {
            for (std::size_t c = 0; c < W; ++c) {
                std::uint32_t val = 0;
                if (!readNextInt(data, len, p, val)) {
                    throw Error("imread: premature end of ASCII PNM stream",
                                0, 0, "imread", "", "numkit:imread:pnmAsciiEof");
                }
                if (hdr.magic == 1) val = val ? 0 : 255;
                if (is16Bit) out.uint16DataMut()[c * H + r] = static_cast<std::uint16_t>(val);
                else out.uint8DataMut()[c * H + r] = static_cast<std::uint8_t>(val);
            }
        }
        return out;
    } else {
        // PPM ASCII (P3)
        Value out = is16Bit ? Value::matrix3d(H, W, 3, ValueType::UINT16, mr)
                            : Value::matrix3d(H, W, 3, ValueType::UINT8, mr);
        const std::size_t plane = H * W;

        for (std::size_t r = 0; r < H; ++r) {
            for (std::size_t c = 0; c < W; ++c) {
                std::uint32_t red = 0, green = 0, blue = 0;
                if (!readNextInt(data, len, p, red) ||
                    !readNextInt(data, len, p, green) ||
                    !readNextInt(data, len, p, blue)) {
                    throw Error("imread: premature end of PPM ASCII stream",
                                0, 0, "imread", "", "numkit:imread:ppmAsciiEof");
                }
                if (is16Bit) {
                    out.uint16DataMut()[c * H + r]             = static_cast<std::uint16_t>(red);
                    out.uint16DataMut()[plane + c * H + r]     = static_cast<std::uint16_t>(green);
                    out.uint16DataMut()[2 * plane + c * H + r] = static_cast<std::uint16_t>(blue);
                } else {
                    out.uint8DataMut()[c * H + r]             = static_cast<std::uint8_t>(red);
                    out.uint8DataMut()[plane + c * H + r]     = static_cast<std::uint8_t>(green);
                    out.uint8DataMut()[2 * plane + c * H + r] = static_cast<std::uint8_t>(blue);
                }
            }
        }
        return out;
    }
}

std::string writePnmToBytes(const Value &A, const std::string &/*ext*/) {
    const std::size_t H = A.dims().rows();
    const std::size_t W = A.dims().cols();
    if (H == 0 || W == 0) {
        throw Error("imwrite: empty image matrix",
                    0, 0, "imwrite", "", "numkit:imwrite:empty");
    }

    int channels = 1;
    if (A.numel() == H * W)          channels = 1;
    else if (A.numel() == H * W * 3) channels = 3;
    else {
        throw Error("imwrite: PNM only supports 1 (PGM) or 3 (PPM) channels",
                    0, 0, "imwrite", "", "numkit:imwrite:pnmChannels");
    }

    const bool is16Bit = (A.type() == ValueType::UINT16);
    const std::string magic = (channels == 1) ? "P5\n" : "P6\n";
    const std::uint32_t maxval = is16Bit ? 65535 : 255;

    std::ostringstream ss;
    ss << magic << W << " " << H << "\n" << maxval << "\n";
    std::string header = ss.str();

    std::vector<std::uint8_t> buf;
    const std::size_t bytesPerSample = is16Bit ? 2 : 1;
    buf.reserve(header.size() + W * H * channels * bytesPerSample);
    buf.insert(buf.end(), header.begin(), header.end());

    const std::size_t plane = H * W;

    for (std::size_t r = 0; r < H; ++r) {
        for (std::size_t c = 0; c < W; ++c) {
            if (channels == 1) {
                if (!is16Bit) {
                    int b = static_cast<int>(A.elemAsDouble(c * H + r));
                    if (b < 0) b = 0; else if (b > 255) b = 255;
                    buf.push_back(static_cast<std::uint8_t>(b));
                } else {
                    double d = A.elemAsDouble(c * H + r);
                    if (d < 0) d = 0; else if (d > 65535) d = 65535;
                    std::uint16_t v = static_cast<std::uint16_t>(d);
                    buf.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
                    buf.push_back(static_cast<std::uint8_t>(v & 0xFF));
                }
            } else {
                for (int ch = 0; ch < 3; ++ch) {
                    if (!is16Bit) {
                        int b = static_cast<int>(A.elemAsDouble(ch * plane + c * H + r));
                        if (b < 0) b = 0; else if (b > 255) b = 255;
                        buf.push_back(static_cast<std::uint8_t>(b));
                    } else {
                        double d = A.elemAsDouble(ch * plane + c * H + r);
                        if (d < 0) d = 0; else if (d > 65535) d = 65535;
                        std::uint16_t v = static_cast<std::uint16_t>(d);
                        buf.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
                        buf.push_back(static_cast<std::uint8_t>(v & 0xFF));
                    }
                }
            }
        }
    }

    return std::string(buf.begin(), buf.end());
}

} // namespace numkit::image
