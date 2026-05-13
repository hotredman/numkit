// libs/io/src/paths/paths.cpp
//
// filesep / fullfile / fileparts / tempdir / tempname.

#include <numkit/io/paths/paths.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>
#include <numkit/core/vfs.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <random>
#include <sstream>
#include <string>

namespace numkit::io {

namespace {

constexpr char kPathSepChar =
#ifdef _WIN32
    '\\';
#else
    '/';
#endif

bool isPathSep(char c) { return c == '/' || c == '\\'; }

// Strip trailing separators from the right edge.
std::string rtrimSep(const std::string &s)
{
    size_t end = s.size();
    while (end > 0 && isPathSep(s[end - 1])) --end;
    return s.substr(0, end);
}

// Strip leading separators from the left edge.
std::string ltrimSep(const std::string &s)
{
    size_t start = 0;
    while (start < s.size() && isPathSep(s[start])) ++start;
    return s.substr(start);
}

Value charValue(std::pmr::memory_resource *mr, const std::string &s)
{
    auto v = Value::matrix(1, s.size(), ValueType::CHAR, mr);
    for (size_t i = 0; i < s.size(); ++i)
        v.charDataMut()[i] = static_cast<char>(s[i]);
    return v;
}

} // namespace

Value filesep(std::pmr::memory_resource *mr)
{
    // (mr-last by convention; only one arg here.)
    return charValue(mr, std::string(1, kPathSepChar));
}

Value fullfile(Span<const std::string> parts,
               std::pmr::memory_resource *mr)
{
    const size_t n = parts.size();
    if (n == 0)
        return charValue(mr, std::string{});
    std::string out = parts[0];
    for (size_t i = 1; i < n; ++i) {
        const std::string &p = parts[i];
        if (p.empty()) continue;
        if (out.empty()) {
            out = p;
            continue;
        }
        out = rtrimSep(out);
        out += kPathSepChar;
        out += ltrimSep(p);
    }
    return charValue(mr, out);
}

std::tuple<Value, Value, Value>
fileparts(const std::string &path, std::pmr::memory_resource *mr)
{
    // Find last separator.
    size_t lastSep = std::string::npos;
    for (size_t i = path.size(); i-- > 0;) {
        if (isPathSep(path[i])) { lastSep = i; break; }
    }
    std::string folder, base;
    if (lastSep == std::string::npos) {
        folder = "";
        base = path;
    } else {
        folder = (lastSep == 0)
                    ? std::string(1, path[0])              // root
                    : path.substr(0, lastSep);
        base = path.substr(lastSep + 1);
    }
    // Find last '.' in base — but ignore a leading dot ('.bashrc').
    std::string name = base, ext;
    if (!base.empty()) {
        size_t dot = base.find_last_of('.');
        if (dot != std::string::npos && dot > 0) {
            name = base.substr(0, dot);
            ext = base.substr(dot);
        }
    }
    return std::make_tuple(charValue(mr, folder),
                            charValue(mr, name),
                            charValue(mr, ext));
}

Value tempdir(Engine &engine, std::pmr::memory_resource *mr)
{
    // Prefer the resolved-FS temp area when available (lets hosts hook
    // a virtual temp area — IDE / WASM use this). Fall back to host OS.
    std::string td;
    auto resolved = engine.resolvePath(".");
    if (resolved.fs) {
        try { td = resolved.fs->tempArea(); } catch (...) { td.clear(); }
    }
    if (td.empty()) {
        std::error_code ec;
        auto p = std::filesystem::temp_directory_path(ec);
        if (!ec) td = p.string();
    }
    if (!td.empty() && !isPathSep(td.back()))
        td.push_back(kPathSepChar);
    return charValue(mr, td);
}

Value tempname(Engine &engine, std::pmr::memory_resource *mr)
{
    static std::atomic<uint64_t> counter{0};
    const uint64_t n = counter.fetch_add(1, std::memory_order_relaxed);
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    const uint64_t ns = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());

    static thread_local std::mt19937_64 rng{
        std::random_device{}() ^ static_cast<uint64_t>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count())};
    const uint64_t r = rng();

    auto td = tempdir(engine, mr).toString();
    std::ostringstream os;
    os << td << "tp"
       << std::hex << ns << "_" << r << "_" << n;
    return charValue(mr, os.str());
}

namespace detail {

void filesep_reg(Span<const Value> /*args*/, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    outs[0] = filesep(ctx.engine->resource());
}

void fullfile_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty()) {
        outs[0] = Value::matrix(1, 0, ValueType::CHAR, ctx.engine->resource());
        return;
    }
    std::vector<std::string> parts;
    parts.reserve(args.size());
    for (const auto &a : args) {
        if (!a.isChar() && !a.isString())
            throw Error("fullfile: all arguments must be strings",
                         0, 0, "fullfile", "", "m:fullfile:badArg");
        parts.push_back(a.toString());
    }
    outs[0] = fullfile(Span<const std::string>(parts.data(), parts.size()), ctx.engine->resource());
}

void fileparts_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty() || (!args[0].isChar() && !args[0].isString()))
        throw Error("fileparts: requires a string path",
                     0, 0, "fileparts", "", "m:fileparts:nargin");
    auto [folder, name, ext] = fileparts(args[0].toString(), ctx.engine->resource());
    outs[0] = std::move(folder);
    if (nargout > 1) outs[1] = std::move(name);
    if (nargout > 2) outs[2] = std::move(ext);
}

void tempdir_reg(Span<const Value> /*args*/, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    outs[0] = tempdir(*ctx.engine, ctx.engine->resource());
}

void tempname_reg(Span<const Value> /*args*/, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    outs[0] = tempname(*ctx.engine, ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::io
