// codegen/src/aot.cpp — see aot.hpp.

#include <numkit/codegen/aot.hpp>

#include <numkit/codegen/aot_config.hpp>

#include <cstdlib>
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

} // namespace

bool available() { return NUMKIT_CODEGEN_AOT_AVAILABLE != 0; }

CompileResult compileToExecutable(const std::string &cppSource,
                                  const std::string &exePath)
{
    CompileResult r;
    if (!available()) {
        r.status = CompileStatus::Unavailable;
        return r;
    }

    const std::string src = exePath + ".cpp";
    const std::string log = exePath + ".log";
    if (!writeFile(src, cppSource)) {
        r.status = CompileStatus::CompileError;
        r.log    = "aot: failed to write source file " + src;
        return r;
    }

    const bool  isMsvc = NUMKIT_CODEGEN_AOT_IS_MSVC != 0;
    std::string cmd;

#ifdef _WIN32
    // A .bat carries the (optional) vcvars call + compile, so the test
    // process needs no developer-prompt environment and we dodge cmd's
    // nested-quote hazards. Redirect the whole script to the log file.
    const std::string bat = exePath + ".build.bat";
    std::string       b   = "@echo off\r\n";
    if (isMsvc) {
        b += "call \"" + toNative(NUMKIT_CODEGEN_AOT_VCVARS) + "\" >nul 2>&1\r\n";
        b += "cl /nologo /O2 /EHsc /std:c++17 \"" + toNative(src) + "\" /Fe:\""
             + toNative(exePath) + "\" /Fo:\"" + toNative(exePath) + ".obj\"\r\n";
    } else {
        b += "\"" + toNative(NUMKIT_CODEGEN_AOT_CXX) + "\" -O2 -std=c++17 \""
             + toNative(src) + "\" -o \"" + toNative(exePath) + "\"\r\n";
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
    (void)isMsvc;
    cmd = std::string(NUMKIT_CODEGEN_AOT_CXX) + " -O2 -std=c++17 '" + src + "' -o '"
          + exePath + "' > '" + log + "' 2>&1";
#endif

    r.command      = cmd;
    const int rc   = std::system(cmd.c_str());
    r.log          = readFile(log);
    r.status = (rc == 0 && fileExists(exePath)) ? CompileStatus::Ok
                                                : CompileStatus::CompileError;
    return r;
}

} // namespace numkit::codegen::aot
