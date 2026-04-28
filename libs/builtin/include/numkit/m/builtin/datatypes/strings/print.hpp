// libs/builtin/include/numkit/m/builtin/datatypes/strings/print.hpp
#pragma once

#include <numkit/m/core/MSpan.hpp>
#include <numkit/m/core/MValue.hpp>

#include <string>

namespace numkit::m {
class Engine;
}

namespace numkit::m::builtin {

/// Format one MValue as MATLAB disp() would, INCLUDING the trailing
/// newline. Exposed so embedders can reuse the MATLAB-style renderer
/// without needing an Engine.
std::string dispFormat(const MValue &a);

/// MATLAB disp(a1, a2, ...) — renders each argument and writes it to
/// engine.outputText(). Engine-stateful.
void disp(Engine &engine, Span<const MValue> args);

/// MATLAB fprintf(...).
///
/// Two call forms (MATLAB disambiguation rule: "scalar then char"):
///   fprintf(fmt, args...)        — writes to stdout via engine.outputText()
///   fprintf(fid, fmt, args...)   — writes to file fid (>= 3),
///                                  or stdout/stderr (fid == 1 or 2).
///
/// Throws MError on invalid / non-writable fid. Engine-stateful.
void fprintf(Engine &engine, Span<const MValue> args);

} // namespace numkit::m::builtin
