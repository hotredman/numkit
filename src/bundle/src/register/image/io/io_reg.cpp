// toolboxes/image/src/io/io_reg.cpp
//
// Register half of the image io builtins: the CallContext wrappers
// delegating to the engine-free compute in io.cpp. library.cpp
// forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/image/io/io.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include "io/io_detail.hpp"
#include "io/png_codec.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::image {

namespace detail {

void imread_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                CallContext &ctx)
{
    if (args.empty())
        throw Error("imread: requires a path string",
                    0, 0, "imread", "", "numkit:imread:nargin");
    if (!args[0].isChar() && !args[0].isString())
        throw Error("imread: path must be a string",
                    0, 0, "imread", "", "numkit:imread:type");
    const std::string path = args[0].toString();

    // Read the file through the engine's VFS prosloyka — the WASM engine
    // has no direct file access; the bytes come from the IDE's virtual or
    // real filesystem (resolvePath picks the backend, incl. the script's
    // own directory). Never fopen here.
    auto rp = ctx.engine->resolvePath(path);
    if (!rp.fs || !rp.fs->exists(rp.path))
        throw Error("imread: failed to load '" + path + "' — file not found",
                    0, 0, "imread", "", "numkit:imread:load");
    // Binary-safe read (an image is raw bytes, not UTF-8 text).
    const std::string bytes = rp.fs->readFileBytes(rp.path);
    const bool tiff = isTiffBytes(bytes);

    // Optional 2nd numeric arg = page index (TIFF multi-page).
    std::uint32_t page = 1;
    if (args.size() >= 2 && !args[1].isEmpty()
        && !args[1].isChar() && !args[1].isString()) {
        page = static_cast<std::uint32_t>(args[1].toScalar());
        if (!tiff)
            throw Error("imread: page index only supported for TIFF files",
                        0, 0, "imread", "", "numkit:imread:notTiff");
    }

    if (tiff) {
        std::vector<std::uint8_t> buf(bytes.begin(), bytes.end());
        // Two-output form `[A, map] = imread(file)` — palette TIFFs.
        if (nargout >= 2) {
            auto pair = readTiffWithMap(std::move(buf), page, ctx.engine->resource());
            outs[0] = std::move(pair.first);
            outs[1] = std::move(pair.second);
        } else {
            outs[0] = readTiff(std::move(buf), page, ctx.engine->resource());
        }
        return;
    }

    const auto *data = reinterpret_cast<const std::uint8_t *>(bytes.data());
    const std::size_t len = bytes.size();
    if (len >= 8 && data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G') {
        if (nargout >= 2) {
            auto pair = readPngWithMap(data, len, ctx.engine->resource());
            outs[0] = std::move(pair.first);
            outs[1] = std::move(pair.second);
            return;
        }
    }

    outs[0] = imreadFromBytes(bytes, ctx.engine->resource());
}

void imwrite_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> /*outs*/,
                 CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imwrite: requires (A, path)",
                    0, 0, "imwrite", "", "numkit:imwrite:nargin");
    if (!args[1].isChar() && !args[1].isString())
        throw Error("imwrite: path must be a string",
                    0, 0, "imwrite", "", "numkit:imwrite:type");

    const std::string path = args[1].toString();
    const std::string ext = lowerExt(path);

    // TIFF route — collect optional 3rd positional 'tif' format string,
    // then NV-pairs ('Compression', 'none'|'packbits'|'lzw'|'deflate';
    // 'WriteMode', 'overwrite'|'append').
    if (ext == "tif" || ext == "tiff") {
        std::string compression = "none";
        bool appendMode = false;
        // Optional 3rd positional 'tif'/'tiff' format keyword (MATLAB
        // syntax `imwrite(A, path, 'tif', ...)`). Skip it as NV-pair start
        // and tolerate.
        size_t nvStart = 2;
        if (args.size() >= 3 && (args[2].isChar() || args[2].isString())) {
            std::string s = args[2].toString();
            std::string lo;
            lo.reserve(s.size());
            for (char c : s) lo.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            if (lo == "tif" || lo == "tiff") nvStart = 3;
        }
        for (size_t i = nvStart; i + 1 < args.size(); i += 2) {
            if (!args[i].isChar() && !args[i].isString())
                throw Error("imwrite TIFF: NV name must be a string",
                            0, 0, "imwrite", "", "numkit:imwrite:badNVName");
            std::string key = args[i].toString();
            std::string lkey;
            lkey.reserve(key.size());
            for (char c : key) lkey.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            const Value &v = args[i + 1];
            if (lkey == "compression") {
                compression = v.toString();
                for (auto &c : compression)
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            } else if (lkey == "writemode") {
                std::string m = v.toString();
                std::string lo;
                for (char c : m) lo.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
                appendMode = (lo == "append");
            } else {
                throw Error("imwrite TIFF: unknown NV key '" + key + "'",
                            0, 0, "imwrite", "", "numkit:imwrite:badNVKey");
            }
        }
        // Encode to bytes and write through the VFS prosloyka. For append
        // mode, the existing pages are read back through the same VFS so
        // multi-page TIFF works on the virtual filesystem too. Never fopen.
        auto rp = ctx.engine->resolvePath(path);
        if (!rp.fs)
            throw Error("imwrite: no filesystem for '" + path + "'",
                        0, 0, "imwrite", "", "numkit:imwrite:write");
        std::vector<std::uint8_t> existing;
        const std::vector<std::uint8_t> *exptr = nullptr;
        if (appendMode && rp.fs->exists(rp.path)) {
            const std::string eb = rp.fs->readFileBytes(rp.path);
            existing.assign(eb.begin(), eb.end());
            exptr = &existing;
        }
        const std::vector<std::uint8_t> buf =
            writeTiffToBytes(args[0], compression, exptr);
        rp.fs->writeFileBytes(rp.path, std::string(buf.begin(), buf.end()));
        return;
    }

    // stb-encodable formats — encode to bytes, write through the VFS.
    auto rp = ctx.engine->resolvePath(path);
    if (!rp.fs)
        throw Error("imwrite: no filesystem for '" + path + "'",
                    0, 0, "imwrite", "", "numkit:imwrite:write");
    const std::string bytes = imwriteToBytes(args[0], ext, ctx.engine->resource());
    rp.fs->writeFileBytes(rp.path, bytes);
}

void imfinfo_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                 CallContext &ctx)
{
    if (args.empty())
        throw Error("imfinfo: requires a path string",
                    0, 0, "imfinfo", "", "numkit:imfinfo:nargin");
    if (!args[0].isChar() && !args[0].isString())
        throw Error("imfinfo: path must be a string",
                    0, 0, "imfinfo", "", "numkit:imfinfo:type");
    const std::string path = args[0].toString();

    // Read through the VFS prosloyka (virtual or real FS); never fopen.
    auto rp = ctx.engine->resolvePath(path);
    if (!rp.fs || !rp.fs->exists(rp.path))
        throw Error("imfinfo: failed to read '" + path + "' — file not found",
                    0, 0, "imfinfo", "", "numkit:imfinfo:read");
    const std::string bytes = rp.fs->readFileBytes(rp.path);
    outs[0] = imfinfoFromBytes(bytes, path, ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::image
