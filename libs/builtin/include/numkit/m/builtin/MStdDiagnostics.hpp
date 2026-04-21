// libs/builtin/include/numkit/m/builtin/MStdDiagnostics.hpp
#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MSpan.hpp>
#include <numkit/m/core/MValue.hpp>

namespace numkit::m {
class Engine;
}

namespace numkit::m::builtin {

// ── MATLAB error() ───────────────────────────────────────────────────
/// Throw an MError. Accepts any of:
///   error()                     → generic "Error"
///   error(msg)                  → msg literal
///   error(msg, arg1, ...)       → sprintf-formatted msg
///   error(id, msg, ...)         → identifier + formatted msg (id has ':')
///   error(MException-struct)    → rethrow fields from struct
/// Always throws; never returns.
[[noreturn]] void error(Span<const MValue> args);

// ── MATLAB warning() ─────────────────────────────────────────────────
/// Engine-stateful — writes "Warning: ...\n" via engine.outputText().
/// Same form-dispatch as error() but does not throw.
void warning(Engine &engine, Span<const MValue> args);

// ── MATLAB MException() ──────────────────────────────────────────────
/// Create an MException-like struct with "identifier" and "message"
/// fields. Form: MException(id, msg, arg1, ...). Throws on <2 args.
MValue mexception(Allocator &alloc, Span<const MValue> args);

// ── rethrow(ME) / throw(ME) ──────────────────────────────────────────
/// Extract "message" + "identifier" from the struct and throw MError.
/// Used by both MATLAB rethrow() and throw() (they are aliases).
[[noreturn]] void rethrowStruct(const MValue &me);

// ── MATLAB assert() ──────────────────────────────────────────────────
/// Throw MError if args[0] is false; otherwise return normally.
///   assert(cond)
///   assert(cond, msg, arg1, ...)
///   assert(cond, id, msg, arg1, ...)    (id has ':')
///   assert(cond, MException-struct)
void assertCond(Span<const MValue> args);

} // namespace numkit::m::builtin
