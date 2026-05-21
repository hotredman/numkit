// libs/builtin/include/numkit/builtin/language/strings/print.hpp
#pragma once

#include <numkit/core/span.hpp>
#include <numkit/core/value.hpp>

#include <string>

namespace numkit {
class Engine;
}

namespace numkit::builtin {

/// @brief Format one `Value` as MATLAB's `disp()` would, including
/// the trailing newline.
///
/// Exposed so embedders can reuse the MATLAB-style renderer without
/// needing an `Engine`.
///
/// @param a  Value to render.
/// @return   The formatted string (with trailing newline).
/// @see disp, fprintf
std::string dispFormat(const Value &a);

/// @brief MATLAB `disp(a1, a2, …)` — render each argument and write
/// it to `engine.outputText()`.
///
/// Engine-stateful — output is routed through the engine's text
/// sink, so `disp` honours redirection / capture set up by the
/// embedder.
///
/// @param engine  Engine context (provides `outputText()`).
/// @param args    Values to render.
/// @see dispFormat, fprintf
void disp(Engine &engine, Span<const Value> args);  // lint: engine-io

/// @brief MATLAB `fprintf(...)`.
///
/// Two call forms (MATLAB disambiguation rule: "scalar then char"):
/// - `fprintf(fmt, args…)`       — writes to stdout via
///                                  `engine.outputText()`.
/// - `fprintf(fid, fmt, args…)`  — writes to file `fid` (≥ 3), or
///                                  stdout/stderr (`fid == 1` or `2`).
///
/// Engine-stateful.
///
/// @param engine  Engine context (provides fid table + text sink).
/// @param args    `(fmt, args…)` or `(fid, fmt, args…)`.
/// @throws Error  On invalid / non-writable fid or malformed format.
/// @see disp, dispFormat
void fprintf(Engine &engine, Span<const Value> args);  // lint: engine-io

} // namespace numkit::builtin
