// include/MTypes.hpp
#pragma once

#include <numkit/m/core/MAst.hpp>
#include <numkit/m/core/MEnvironment.hpp>
#include <numkit/m/core/MSpan.hpp>
#include <numkit/m/core/MValue.hpp>

#include <chrono>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace numkit::m {

// ============================================================
// Timer types
// ============================================================
using Clock = std::chrono::high_resolution_clock;
using TimePoint = Clock::time_point;

// ============================================================
// Function types
// ============================================================
using BinaryOpFunc = std::function<MValue(const MValue &, const MValue &)>;
using UnaryOpFunc = std::function<MValue(const MValue &)>;

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
    void(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx)>;

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

// ============================================================
// Runtime error with source location
// ============================================================
class MError : public std::runtime_error
{
public:
    MError(const std::string &msg,
              int line = 0,
              int col = 0,
              const std::string &funcName = "",
              const std::string &context = "",
              const std::string &identifier = "")
        : std::runtime_error(msg) // what() = raw message (clean, for try/catch)
        , line_(line)
        , col_(col)
        , funcName_(funcName)
        , context_(context)
        , identifier_(identifier)
    {}

    int line() const { return line_; }
    int col() const { return col_; }
    const std::string &funcName() const { return funcName_; }
    const std::string &context() const { return context_; }
    const std::string &identifier() const { return identifier_; }

    // Formatted for user display: "Error at line 15, column 3:\n  msg (in call to 'sin')"
    std::string formattedWhat() const
    {
        if (line_ <= 0)
            return what();
        std::string result = "Error at line " + std::to_string(line_);
        if (col_ > 0)
            result += ", column " + std::to_string(col_);
        if (!funcName_.empty())
            result += " in '" + funcName_ + "'";
        result += ":\n  ";
        result += what();
        if (!context_.empty())
            result += " (" + context_ + ")";
        return result;
    }

private:
    int line_;
    int col_;
    std::string funcName_;
    std::string context_;    // e.g. "in call to 'sin'"
    std::string identifier_; // e.g. "MATLAB:badInput"
};

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
};

} // namespace numkit::m