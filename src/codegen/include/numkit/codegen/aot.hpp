// codegen/include/numkit/codegen/aot.hpp
//
// The AOT compile harness (build plan brick 4): take a self-contained C++
// source string produced by emitFunction() and compile it to a native
// executable with the external compiler captured at CMake configure time
// (aot_config.hpp). This is the bridge from "we emitted C++" to "we ran
// it" — the end-to-end value-differential gate (brick 5) builds on it.
//
// Design: the harness NEVER fails the test suite when no compiler is
// configured — it returns Unavailable and the caller skips. When a
// compiler IS configured, a non-zero exit (or a missing artifact) is a
// CompileError with the captured compiler log. MSVC is driven through its
// vcvars environment via a generated .bat (so INCLUDE/LIB are set without
// the test process inheriting a developer prompt).

#pragma once

#include <string>

namespace numkit::codegen::aot {

enum class CompileStatus {
    Ok,            // compiled; the executable exists at exePath
    CompileError,  // the compiler ran but failed (see CompileResult::log)
    Unavailable,   // no external compiler configured for this build
};

struct CompileResult {
    CompileStatus status = CompileStatus::Unavailable;
    std::string   log;      // compiler stdout+stderr (best effort)
    std::string   command;  // the command line used (for diagnostics)

    bool ok() const { return status == CompileStatus::Ok; }
};

// True when an external C++ compiler was configured at CMake-configure
// time (so compileToExecutable can actually build something).
bool available();

// Compile `cppSource` (a self-contained TU) into a native executable at
// `exePath`. Writes intermediate files (`<exePath>.cpp`, a build script,
// a log) next to `exePath`. Synchronous. Returns Unavailable without
// touching the filesystem when no compiler is configured.
CompileResult compileToExecutable(const std::string &cppSource,
                                  const std::string &exePath);

// As above but emits a shared library (DLL / .so) at `libPath` — for
// loading the transpiler's output and benchmarking it against the
// hand-written reference (brick 7). The source should export a symbol
// (e.g. extern "C" __declspec(dllexport) on Windows).
CompileResult compileToSharedLibrary(const std::string &cppSource,
                                     const std::string &libPath);

} // namespace numkit::codegen::aot
