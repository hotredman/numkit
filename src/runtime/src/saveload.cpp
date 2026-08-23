// runtime/src/saveload.cpp
//
// Workspace-persistence builtins (save / load). Dispatches between the
// ASCII backend (saveload_ascii.cpp) and the binary .mat backend
// (saveload_mat.cpp) based on flags and the filename's extension —
// matching MATLAB's defaults (binary .mat unless `-ascii` is given).

#include <numkit/runtime/saveload.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/environment.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace numkit::runtime {

// ── Backends ─────────────────────────────────────────────────────────

void saveAscii(Engine &engine, Environment &env,
               const std::string &filename,
               const std::vector<std::string> &varnames);

void loadAscii(Engine &engine, Environment &env,
               const std::string &filename,
               size_t nargout, Span<Value> outs);

void saveMat(Engine &engine, Environment &env,
             const std::string &filename,
             const std::vector<std::string> &varnames,
             int matVersion);

void loadMat(Engine &engine, Environment &env,
             const std::string &filename,
             size_t nargout, Span<Value> outs);

namespace {

bool endsWithIgnoreCase(const std::string &s, const std::string &suffix)
{
    if (s.size() < suffix.size()) return false;
    for (size_t i = 0; i < suffix.size(); ++i) {
        char a = s[s.size() - suffix.size() + i];
        char b = suffix[i];
        if (std::tolower(static_cast<unsigned char>(a))
            != std::tolower(static_cast<unsigned char>(b)))
            return false;
    }
    return true;
}

} // namespace

// ════════════════════════════════════════════════════════════════════════
// save / load Entry Points
// ════════════════════════════════════════════════════════════════════════

void save(Engine &engine, Environment &env, Span<const Value> args)
{
    if (args.empty() || !args[0].isChar())
        throw Error("save: filename required");
    std::string filename = args[0].toString();

    // Three-way mode pick: explicit flag wins; otherwise extension hint;
    // otherwise MATLAB default (binary mat).
    enum class Mode { Auto, Ascii, Mat };
    Mode mode = Mode::Auto;
    // matVersion follows MATLAB-flag convention: 4 = MAT4, 5/6 = MAT5,
    // 7 = MAT5 + zlib. `-mat` alone (no version suffix) defaults to 5.
    // Whichever `-vN` appears last wins (mirrors MATLAB).
    int matVersion = 5;

    ScratchArena scratch(engine.resource());
    ScratchVec<std::string> varnames(&scratch);
    for (size_t i = 1; i < args.size(); ++i) {
        if (!args[i].isChar())
            continue;
        std::string s = args[i].toString();
        if (s == "-ascii") { mode = Mode::Ascii; continue; }
        if (s == "-mat") { mode = Mode::Mat; continue; }
        if (s == "-v4") { mode = Mode::Mat; matVersion = 4; continue; }
        if (s == "-v6") { mode = Mode::Mat; matVersion = 6; continue; }
        if (s == "-v7") { mode = Mode::Mat; matVersion = 7; continue; }
        if (s == "-v7.3")
            throw Error("save: -v7.3 (HDF5) is not supported in this build");
        if (s == "-append" || s == "-nocompression" || s == "-struct") {
            // accepted no-ops for now; -append is binary-only in MATLAB.
            if (s == "-append") mode = Mode::Mat;
            continue;
        }
        if (!s.empty() && s.front() == '-')
            throw Error("save: unsupported flag '" + s + "'");
        varnames.push_back(s);
    }

    if (mode == Mode::Auto)
        mode = endsWithIgnoreCase(filename, ".mat") ? Mode::Mat : Mode::Ascii;

    std::vector<std::string> names(varnames.begin(), varnames.end());
    if (mode == Mode::Mat) {
        saveMat(engine, env, filename, names, matVersion);
    } else {
        saveAscii(engine, env, filename, names);
    }
}

void load(Engine &engine, Environment &env, Span<const Value> args,
          size_t nargout, Span<Value> outs)
{
    if (args.empty() || !args[0].isChar())
        throw Error("load: filename required");
    std::string filename = args[0].toString();

    enum class Mode { Auto, Ascii, Mat };
    Mode mode = Mode::Auto;
    for (size_t i = 1; i < args.size(); ++i) {
        if (!args[i].isChar()) continue;
        std::string s = args[i].toString();
        if (s == "-ascii") { mode = Mode::Ascii; continue; }
        if (s == "-mat" || s == "-v4" || s == "-v6" || s == "-v7") {
            mode = Mode::Mat; continue;
        }
        if (s == "-v7.3")
            throw Error("load: -v7.3 (HDF5) is not supported in this build");
        if (!s.empty() && s.front() == '-')
            throw Error("load: unsupported flag '" + s + "'");
    }

    if (mode == Mode::Auto)
        mode = endsWithIgnoreCase(filename, ".mat") ? Mode::Mat : Mode::Ascii;

    if (mode == Mode::Mat) {
        loadMat(engine, env, filename, nargout, outs);
    } else {
        loadAscii(engine, env, filename, nargout, outs);
    }
}

// ════════════════════════════════════════════════════════════════════════
// Adapters
// ════════════════════════════════════════════════════════════════════════

namespace detail {

void save_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    (void)nargout;
    (void)outs;
    save(*ctx.engine, *ctx.env, args);
}

void load_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    load(*ctx.engine, *ctx.env, args, nargout, outs);
}

} // namespace detail

} // namespace numkit::runtime
