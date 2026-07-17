// codegen/src/aot.cpp — see aot.hpp.

#include <numkit/codegen/aot.hpp>

#include <numkit/codegen/aot_config.hpp>

#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

namespace numkit::codegen::aot {

namespace {

std::string toNative(std::string s)
{
#ifdef _WIN32
    for (char &c : s)
        if (c == '/') c = '\\';
#endif
    return s;
}

bool writeFile(const std::string &path, const std::string &content)
{
    std::ofstream os(path, std::ios::binary);
    if (!os) return false;
    os.write(content.data(), static_cast<std::streamsize>(content.size()));
    return static_cast<bool>(os);
}

std::string readFile(const std::string &path)
{
    std::ifstream is(path, std::ios::binary);
    if (!is) return {};
    std::ostringstream ss;
    ss << is.rdbuf();
    return ss.str();
}

bool fileExists(const std::string &path)
{
    std::ifstream is(path, std::ios::binary);
    return static_cast<bool>(is);
}

// The C++ compiler used at runtime. An env override (NUMKIT_CXX) wins over the
// build-time-captured compiler (NUMKIT_CODEGEN_AOT_CXX): a user (or the IDE)
// can `setenv('NUMKIT_CXX', ...)` to redirect AOT compilation to a different
// toolchain without reconfiguring CMake. An env-overridden compiler is treated
// as self-sufficient (no vcvars shell) — the build-time MSVC+vcvars path only
// applies when the build-time compiler is in use. Returns empty when neither
// is set, in which case compileImpl reports Unavailable (as before).
std::string resolveCxx()
{
    if (const char *env = std::getenv("NUMKIT_CXX"))
        if (env[0] != '\0')
            return env;
    return NUMKIT_CODEGEN_AOT_CXX;
}

// True when the build-time compiler is MSVC AND the user did not override it
// (an env-overridden compiler is always driven directly, never via vcvars).
bool useMsvcVcvars(const std::string &cxx)
{
    return NUMKIT_CODEGEN_AOT_IS_MSVC != 0 && cxx == NUMKIT_CODEGEN_AOT_CXX;
}

} // namespace

bool available() { return NUMKIT_CODEGEN_AOT_AVAILABLE != 0; }

namespace {

// Shared compile core for the executable and shared-library forms; the
// only difference is the output kind (an extra /LD for MSVC, -shared
// -fPIC otherwise).
CompileResult compileImpl(const std::string &cppSource, const std::string &outPath,
                          bool sharedLib, const CompileOptions &opts)
{
    CompileResult r;
    const std::string cxx = resolveCxx();
    if (cxx.empty() || !available()) {
        r.status = CompileStatus::Unavailable;
        return r;
    }

    const std::string src = outPath + ".cpp";
    const std::string log = outPath + ".log";
    if (!writeFile(src, cppSource)) {
        r.status = CompileStatus::CompileError;
        r.log    = "aot: failed to write source file " + src;
        return r;
    }

    const bool  isMsvc = useMsvcVcvars(cxx);
    std::string cmd;

#ifdef _WIN32
    // A .bat carries the (optional) vcvars call + compile, so the test
    // process needs no developer-prompt environment and we dodge cmd's
    // nested-quote hazards. Redirect the whole script to the log file.
    const std::string bat = outPath + ".build.bat";
    std::string       b   = "@echo off\r\n";
    if (isMsvc) {
        std::string inc, def, libs;
        for (const auto &d : opts.includeDirs) inc += " /I\"" + toNative(d) + "\"";
        for (const auto &d : opts.defines) def += " /D" + d;
        for (const auto &l : opts.linkLibs) libs += " \"" + toNative(l) + "\"";
        b += "call \"" + toNative(NUMKIT_CODEGEN_AOT_VCVARS) + "\" >nul 2>&1\r\n";
        b += std::string("cl /nologo /O2 /EHsc /std:c++17 ") + (sharedLib ? "/LD " : "") + inc
             + def + " \"" + toNative(src) + "\" /Fe:\"" + toNative(outPath) + "\" /Fo:\""
             + toNative(outPath) + ".obj\"" + (libs.empty() ? "" : " /link" + libs) + "\r\n";
    } else {
        std::string inc, def, libs;
        for (const auto &d : opts.includeDirs) inc += " -I\"" + toNative(d) + "\"";
        for (const auto &d : opts.defines) def += " -D" + d;
        for (const auto &l : opts.linkLibs) libs += " \"" + toNative(l) + "\"";
        b += "\"" + toNative(cxx) + "\" -O2 -std=c++17 "
             + (sharedLib ? "-shared -fPIC " : "") + inc + def + " \"" + toNative(src) + "\" -o \""
             + toNative(outPath) + "\"" + libs + "\r\n";
    }
    b += "exit /b %ERRORLEVEL%\r\n";
    if (!writeFile(bat, b)) {
        r.status = CompileStatus::CompileError;
        r.log    = "aot: failed to write build script " + bat;
        return r;
    }
    // `cmd /c` (what std::system uses) strips the first and last quote of
    // the whole line, which would mangle a command that itself begins and
    // ends with a quoted path. Wrap the entire command in one extra pair
    // so the inner quoting survives.
    cmd = "\"\"" + toNative(bat) + "\" > \"" + toNative(log) + "\" 2>&1\"";
#else
    std::string inc, def, libs;
    for (const auto &d : opts.includeDirs) inc += " -I'" + d + "'";
    for (const auto &d : opts.defines) def += " -D" + d;
    for (const auto &l : opts.linkLibs) libs += " '" + l + "'";
    cmd = cxx + " -O2 -std=c++17 "
          + (sharedLib ? "-shared -fPIC " : "") + inc + def + " '" + src + "' -o '" + outPath
          + "'" + libs + " > '" + log + "' 2>&1";
#endif

    r.command    = cmd;
    const int rc = std::system(cmd.c_str());
    r.log        = readFile(log);
    r.status = (rc == 0 && fileExists(outPath)) ? CompileStatus::Ok
                                                 : CompileStatus::CompileError;
    return r;
}

} // namespace

CompileResult compileToExecutable(const std::string &cppSource, const std::string &exePath,
                                  const CompileOptions &opts)
{
    return compileImpl(cppSource, exePath, /*sharedLib=*/false, opts);
}

CompileResult compileToSharedLibrary(const std::string &cppSource, const std::string &libPath,
                                      const CompileOptions &opts)
{
    return compileImpl(cppSource, libPath, /*sharedLib=*/true, opts);
}

RunResult runExecutable(const std::string &exePath)
{
    RunResult r;
#ifdef __EMSCRIPTEN__
    (void)exePath;
    r.log = "aot: runExecutable is not available under Emscripten — a native "
            "process cannot be spawned from a WASM module. Use the desktop IDE "
            "or the native `numkit` CLI for compile-and-run.";
    return r;
#else
    if (exePath.empty() || !fileExists(exePath)) {
        r.log = "aot: executable not found: " + exePath;
        return r;
    }
    // Capture combined stdout+stderr via a pipe. popen returns the child's
    // stdout; `2>&1` merges stderr into that stream. Quote the exe path on
    // Windows (cmd /c strips the outer quotes, so wrap once more — same
    // trick as compileImpl).
    std::string cmd;
#  ifdef _WIN32
    cmd = "\"\"" + toNative(exePath) + "\" 2>&1\"";
#  else
    cmd = "'" + exePath + "' 2>&1";
#  endif
#  ifdef _WIN32
    FILE *pipe = _popen(cmd.c_str(), "rt");
#  else
    FILE *pipe = ::popen(cmd.c_str(), "r");
#  endif
    if (!pipe) {
        r.log = "aot: failed to launch executable: " + exePath;
        return r;
    }
    std::ostringstream ss;
    char buf[4096];
    while (std::fgets(buf, sizeof(buf), pipe))
        ss << buf;
#  ifdef _WIN32
    r.exitCode = _pclose(pipe);
#  else
    r.exitCode = ::pclose(pipe);
#  endif
    r.log = ss.str();
    return r;
#endif
}

} // namespace numkit::codegen::aot
