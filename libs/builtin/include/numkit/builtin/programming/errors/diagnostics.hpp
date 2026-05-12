// libs/builtin/include/numkit/builtin/programming/errors/diagnostics.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/span.hpp>
#include <numkit/core/value.hpp>

namespace numkit {
class Engine;
}

namespace numkit::builtin {

// ── MATLAB error() ───────────────────────────────────────────────────
/// Throw an Error. Accepts any of:
///   error()                     → generic "Error"
///   error(msg)                  → msg literal
///   error(msg, arg1, ...)       → sprintf-formatted msg
///   error(id, msg, ...)         → identifier + formatted msg (id has ':')
///   error(MException-struct)    → rethrow fields from struct
/// Always throws; never returns.
[[noreturn]] void error(Span<const Value> args);

//// Engine-stateful — writes "Warning: ...\n" via engine.outputText().
//// Same form-dispatch as error() but does not throw.
void warning(Engine &engine, Span<const Value> args);

//// Most-recent warning state. `warning(...)` updates these; the
//// `lastwarn` builtin reads them. Storage is thread_local in the
//// implementation TU, so concurrent engines on different threads
//// each see their own last-warning. No core-side state involved.
struct LastWarn { std::string msg; std::string id; };
LastWarn lastwarnGet();
void     lastwarnSet(const std::string &msg, const std::string &id);

//// Create an MException-like struct with "identifier" and "message"
//// fields. Form: MException(id, msg, arg1, ...). Throws on <2 args.
Value mexception(Span<const Value> args, std::pmr::memory_resource *mr = nullptr);

// ── rethrow(ME) / throw(ME) ──────────────────────────────────────────
/// Extract "message" + "identifier" from the struct and throw Error.
/// Used by both MATLAB rethrow() and throw() (they are aliases).
[[noreturn]] void rethrowStruct(const Value &me);

//// Throw Error if args[0] is false; otherwise return normally.
////   assert(cond)
////   assert(cond, msg, arg1, ...)
////   assert(cond, id, msg, arg1, ...)    (id has ':')
////   assert(cond, MException-struct)
void assertCond(Span<const Value> args);

} // namespace numkit::builtin
