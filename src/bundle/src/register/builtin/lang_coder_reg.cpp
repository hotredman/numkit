// bundle/src/register/lang/coder_reg.cpp
//
// Registration adapters for the AOT codegen + native-process builtins:
//   coder(src, entry, argsSpec)      → generated C++ (char)        [pure C++]
//   coder_run(src, entry, argsSpec)  → program stdout (char)       [native only]
//   [status, cmdout] = system(cmd)   → shell command               [native only]
//   [status, out] = runNative(script)→ run a .m via NUMKIT_INTERP  [native only]
//
// Lives in bundle (L3), not runtime, because it calls the codegen driver
// (`numkit::codegen::driver::transpileSource` / `aot::compileToExecutable`),
// and `runtime` (L2) is forbidden by tools/check_layering.py from including
// <numkit/codegen/...>. codegen itself stays L2 and registers no builtin
// (DESIGN.md §9); this file is the bundle-side adapter that binds it into the
// engine — the same role other *_reg.cpp files here play for their compute
// libraries. `coder` (transpile) is pure C++ and works under WASM (the browser
// IDE's "generate C++" demo reaches it via the generateCpp embind); the other
// three spawn a native process and throw under Emscripten.

#include <numkit/codegen/aot.hpp>
#include <numkit/codegen/driver.hpp>
#include <numkit/codegen/emitter.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>
#include <numkit/fs/branding.hpp>     // for envGet (not used here, kept for parity with env.cpp)
#include <numkit/value/span.hpp>
#include <numkit/value/value.hpp>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>

namespace numkit::builtin {
namespace detail {

namespace {

bool isCharArg(const Value &v) { return v.isChar() || v.isString(); }
std::string charArg(const Value &v) { return v.toString(); }

[[noreturn]] void rethrowCodegen(const char *fn, const std::exception &e)
{
    throw Error(std::string(fn) + ": " + e.what(),
                0, 0, fn, "", "numkit:codegen");
}

std::string tempExePath(const char *tag)
{
    namespace fs = std::filesystem;
    static std::atomic<std::uint64_t> seq{0};
    const auto base = fs::temp_directory_path() / "numkit_coder";
    fs::create_directories(base);
    auto p = base / (std::string(tag) + "_" + std::to_string(++seq) +
#ifdef _WIN32
                     ".exe"
#else
                     ""
#endif
                    );
    return p.string();
}

struct SystemResult { int status = -1; std::string out; };

SystemResult captureProcess(const std::string &cmd)
{
    SystemResult r;
#ifdef _WIN32
    FILE *pipe = _popen(cmd.c_str(), "rt");
#else
    FILE *pipe = ::popen(cmd.c_str(), "r");
#endif
    if (!pipe) {
        r.status = -1;
        r.out    = "system: failed to launch command";
        return r;
    }
    std::ostringstream ss;
    char buf[4096];
    while (std::fgets(buf, sizeof(buf), pipe))
        ss << buf;
#ifdef _WIN32
    r.status = _pclose(pipe);
#else
    r.status = ::pclose(pipe);
#endif
    r.out = ss.str();
    return r;
}

} // namespace

// ════════════════════════════════════════════════════════════════════════
// coder(src, entry, argsSpec) → char row (the generated C++ TU).
// ════════════════════════════════════════════════════════════════════════
void coder_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    (void)nargout;
    if (args.size() < 1 || !isCharArg(args[0]))
        throw Error("coder: first argument must be the numkit source (char)",
                    0, 0, "coder", "", "numkit:coder:nargin");
    const std::string src   = charArg(args[0]);
    const std::string entry = (args.size() >= 2 && isCharArg(args[1])) ? charArg(args[1]) : "";
    const std::string spec  = (args.size() >= 3 && isCharArg(args[2])) ? charArg(args[2]) : "";
    try {
        const auto paramTypes = numkit::codegen::driver::parseTypeSpec(spec);
        numkit::codegen::BridgeOptions bridgeOpts;
        const numkit::codegen::EmittedFunction em =
            numkit::codegen::driver::transpileSource(src, entry, paramTypes, bridgeOpts);
        outs[0] = Value::fromString(em.source, ctx.engine->resource());
    } catch (const Error &) {
        throw;
    } catch (const std::exception &e) {
        rethrowCodegen("coder", e);
    }
}

// ════════════════════════════════════════════════════════════════════════
// coder_run(src, entry, argsSpec) → char row (the program's stdout).
// v1: nullary entry only; a main() harness (driver::buildHarnessMain) wraps
// the emitted function, calls it, and prints its scalar / complex return.
// ════════════════════════════════════════════════════════════════════════
void coder_run_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    (void)nargout;
#ifdef __EMSCRIPTEN__
    (void)args;
    throw Error("coder_run: not available in a WASM build — a native C++ "
                "compiler cannot be spawned from the browser. Use the desktop "
                "IDE or the native `numkit` CLI.",
                0, 0, "coder_run", "", "numkit:coder:nowasm");
#else
    if (args.size() < 1 || !isCharArg(args[0]))
        throw Error("coder_run: first argument must be the numkit source (char)",
                    0, 0, "coder_run", "", "numkit:coder:nargin");
    const std::string src   = charArg(args[0]);
    const std::string entry = (args.size() >= 2 && isCharArg(args[1])) ? charArg(args[1]) : "";
    const std::string spec  = (args.size() >= 3 && isCharArg(args[2])) ? charArg(args[2]) : "";

    std::string cppSource;
    try {
        const auto paramTypes = numkit::codegen::driver::parseTypeSpec(spec);
        if (!paramTypes.empty())
            throw Error("coder_run: v1 supports only a nullary entry (no input args). "
                        "For functions with args, use `coder` to get the C++ and write "
                        "your own main() harness.",
                        0, 0, "coder_run", "", "numkit:coder:args");
        numkit::codegen::BridgeOptions bridgeOpts;
        const numkit::codegen::EmittedFunction em =
            numkit::codegen::driver::transpileSource(src, entry, paramTypes, bridgeOpts);
        cppSource = em.source + numkit::codegen::driver::buildHarnessMain(em);
    } catch (const Error &) {
        throw;
    } catch (const std::exception &e) {
        rethrowCodegen("coder_run", e);
    }

    if (!numkit::codegen::aot::available())
        throw Error("coder_run: no C++ compiler configured. Set NUMKIT_CXX "
                    "(e.g. setenv('NUMKIT_CXX','clang++')) or rebuild with a "
                    "build-time compiler.",
                    0, 0, "coder_run", "", "numkit:coder:nocompiler");

    const std::string exePath = tempExePath("coder_run");
    const auto cr = numkit::codegen::aot::compileToExecutable(cppSource, exePath);
    if (!cr.ok())
        throw Error("coder_run: compilation failed.\n--- compiler log ---\n" +
                    cr.log + "\n--- end log ---",
                    0, 0, "coder_run", "", "numkit:coder:compile");

    const auto rr = numkit::codegen::aot::runExecutable(exePath);
    std::error_code ec;
    std::filesystem::remove(exePath, ec);
    std::filesystem::remove(exePath + ".cpp", ec);
    std::filesystem::remove(exePath + ".log", ec);
#ifdef _WIN32
    std::filesystem::remove(exePath + ".obj", ec);
    std::filesystem::remove(exePath + ".build.bat", ec);
#endif
    outs[0] = Value::fromString(rr.log, ctx.engine->resource());
#endif
}

// ════════════════════════════════════════════════════════════════════════
// [status, cmdout] = system(cmd) — run a shell command, capture stdout+stderr.
// ════════════════════════════════════════════════════════════════════════
void system_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
#ifdef __EMSCRIPTEN__
    (void)args; (void)ctx;
    if (nargout <= 1)
        outs[0] = Value::scalar(-1.0, ctx.engine->resource());
    else {
        outs[0] = Value::scalar(-1.0, ctx.engine->resource());
        outs[1] = Value::fromString("system: not available in a WASM build — a native "
                                    "process cannot be spawned from the browser.",
                                    ctx.engine->resource());
    }
    return;
#else
    if (args.empty() || !isCharArg(args[0]))
        throw Error("system: argument must be a char command string",
                    0, 0, "system", "", "numkit:system:nargin");
    const SystemResult r = captureProcess(charArg(args[0]) + " 2>&1");
    // MATLAB: status = system(cmd) returns just the exit code; with two
    // outputs [status, cmdout] returns both. Single-output → status.
    outs[0] = Value::scalar(static_cast<double>(r.status), ctx.engine->resource());
    if (nargout >= 2)
        outs[1] = Value::fromString(r.out, ctx.engine->resource());
#endif
}

// ════════════════════════════════════════════════════════════════════════
// [status, out] = runNative(script) — run a .m file with the native numkit
// interpreter whose path is read from NUMKIT_INTERP (set via setenv).
// ════════════════════════════════════════════════════════════════════════
void runNative_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
#ifdef __EMSCRIPTEN__
    (void)args; (void)ctx;
    if (nargout <= 1)
        outs[0] = Value::scalar(-1.0, ctx.engine->resource());
    else {
        outs[0] = Value::scalar(-1.0, ctx.engine->resource());
        outs[1] = Value::fromString("runNative: not available in a WASM build — a native "
                                    "process cannot be spawned from the browser.",
                                    ctx.engine->resource());
    }
    return;
#else
    if (args.empty() || !isCharArg(args[0]))
        throw Error("runNative: argument must be a char script path",
                    0, 0, "runNative", "", "numkit:runNative:nargin");
    const char *interp = std::getenv("NUMKIT_INTERP");
    if (!interp || interp[0] == '\0')
        throw Error("runNative: NUMKIT_INTERP is not set — set it to the path "
                    "of the native numkit interpreter (e.g. "
                    "setenv('NUMKIT_INTERP','numkit')).",
                    0, 0, "runNative", "", "numkit:runNative:nointerp");
    const std::string script = charArg(args[0]);
    std::string cmd;
#ifdef _WIN32
    cmd = "\"\"" + std::string(interp) + "\" \"" + script + "\" 2>&1\"";
#else
    cmd = "'" + std::string(interp) + "' '" + script + "' 2>&1";
#endif
    const SystemResult r = captureProcess(cmd);
    outs[0] = Value::scalar(static_cast<double>(r.status), ctx.engine->resource());
    if (nargout >= 2)
        outs[1] = Value::fromString(r.out, ctx.engine->resource());
#endif
}

} // namespace detail
} // namespace numkit::builtin
