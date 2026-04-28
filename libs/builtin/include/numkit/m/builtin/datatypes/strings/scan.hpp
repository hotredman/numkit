// libs/builtin/include/numkit/m/builtin/datatypes/strings/scan.hpp
#pragma once

#include <numkit/m/core/MSpan.hpp>
#include <numkit/m/core/MValue.hpp>

namespace numkit::m {
class Engine;
}

namespace numkit::m::builtin {

// ════════════════════════════════════════════════════════════════════════
// Scan family (fscanf / sscanf / textscan) — MATLAB-compatible formatted
// reading. Like the other file-I/O builtins, the natural C++ surface is a
// Span-based shape because these functions are inherently variadic
// (optional size, optional name/value option pairs). Reshaping into
// neater overloads would only force every caller to re-parse the same
// dynamic argument layout.
//
// fscanf / textscan need the owning Engine because the fid table lives
// there; sscanf is pure and takes just Allocator.
// ════════════════════════════════════════════════════════════════════════

void fscanf(Engine &engine, Span<const MValue> args, size_t nargout, Span<MValue> outs);
void sscanf(Allocator &alloc, Span<const MValue> args, size_t nargout, Span<MValue> outs);
void textscan(Engine &engine, Span<const MValue> args, size_t nargout, Span<MValue> outs);

} // namespace numkit::m::builtin
