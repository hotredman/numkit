// libs/builtin/include/numkit/builtin/datatypes/strings/print.hpp
#pragma once

#include <numkit/core/span.hpp>
#include <numkit/core/value.hpp>

#include <string>

namespace numkit {
class Engine;
}

namespace numkit::builtin {

/// Format one Value as MATLAB disp() would, INCLUDING the trailing
/// newline. Exposed so embedders can reuse the MATLAB-style renderer
/// without needing an Engine.
std::string dispFormat(const Value &a);

/// MATLAB disp(a1, a2, ...) — renders each argument and writes it to
/// engine.outputText(). Engine-stateful.
void disp(Engine &engine, Span<const Value> args);

/// MATLAB fprintf(...).
///
/// Two call forms (MATLAB disambiguation rule: "scalar then char"):
///   fprintf(fmt, args...)        — writes to stdout via engine.outputText()
///   fprintf(fid, fmt, args...)   — writes to file fid (>= 3),
///                                  or stdout/stderr (fid == 1 or 2).
///
/// Throws Error on invalid / non-writable fid. Engine-stateful.
void fprintf(Engine &engine, Span<const Value> args);

} // namespace numkit::builtin
