// libs/builtin/include/numkit/builtin/datatypes/strings/scan.hpp
#pragma once

#include <numkit/core/span.hpp>
#include <numkit/core/value.hpp>

namespace numkit {
class Engine;
}

namespace numkit::builtin {

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

void fscanf(Engine &engine, Span<const Value> args, size_t nargout, Span<Value> outs);
void sscanf(std::pmr::memory_resource *mr, Span<const Value> args, size_t nargout, Span<Value> outs);
void textscan(Engine &engine, Span<const Value> args, size_t nargout, Span<Value> outs);

} // namespace numkit::builtin
