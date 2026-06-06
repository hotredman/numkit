// include/types.hpp
#pragma once

#include <numkit/core/ast.hpp>
#include <numkit/core/environment.hpp>
#include <numkit/value/error.hpp>   // numkit::Error (L0) — re-exported here for existing users
#include <numkit/value/span.hpp>
#include <numkit/value/value.hpp>

#include <chrono>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace numkit {

// ============================================================
// Timer types
// ============================================================
using Clock = std::chrono::high_resolution_clock;
using TimePoint = Clock::time_point;

// ============================================================
// Function types
// ============================================================
using BinaryOpFunc = std::function<Value(const Value &, const Value &)>;
using UnaryOpFunc = std::function<Value(const Value &)>;

// Forward declaration for CallContext
class Engine;

// ============================================================
// CallContext — passed to all external functions
//
// Provides controlled access to the interpreter state:
//   engine — allocator, outputText, figureManager, hasFunction, etc.
//   env    — current scope (global for scripts, local for functions)
// ============================================================
struct CallContext
{
    Engine *engine;
    Environment *env;
};

using ExternalFunc = std::function<
    void(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)>;

// ============================================================
// Control flow signals
// ============================================================
struct BreakSignal
{};
struct ContinueSignal
{};
struct ReturnSignal
{};

enum class FlowSignal : uint8_t { NONE = 0, BREAK, CONTINUE, RETURN };

// ============================================================
// Reserved names — classified for precise use at call sites.
// ============================================================
//
// Numeric/logical constants served by `constantsEnv_` (parent of the
// base workspace). Always defined at runtime. Assigning to one creates
// a shadow local; `clear name` removes the shadow.
extern const std::unordered_set<std::string> kBuiltinConstants;

// Runtime pseudo-variables set by the VM / display pipeline, not by
// user code. `nargin` / `nargout` are loaded on function entry; `ans`
// is set by DISPLAY for bare expressions; `end` is the magic keyword
// used in indexing. None of them live in any environment.
extern const std::unordered_set<std::string> kPseudoVars;

// Union of the above — "any name the compiler / debugger treats
// specially and must not mistake for a plain user variable". Most
// filter sites want this broad set; the narrower ones are for places
// where we specifically need "true constant" semantics (e.g. deciding
// whether to LOAD_CONST from `constantsEnv_`).
extern const std::unordered_set<std::string> kBuiltinNames;

// numkit::Error (runtime error with source location) moved to an L0 header:
// <numkit/value/error.hpp>, included above. Kept out of this heavy core header
// so value/ops/toolboxes can throw it without pulling in the engine.

// ============================================================
// User-defined function descriptor
// ============================================================
struct UserFunction
{
    std::string name;
    std::vector<std::string> params;
    std::vector<std::string> returns;
    std::shared_ptr<const ASTNode> body;
    std::shared_ptr<Environment> closureEnv;
    mutable int8_t usesNarginNargout = -1;
    // classdef methods/constructors only: the class that declares this body.
    // Drives the member-access execution context (pushed while the body runs).
    // Empty for ordinary functions.
    std::string ownerClass;
};

} // namespace numkit