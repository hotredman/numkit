// include/MLabTypes.hpp
#pragma once

#include "MLabAst.hpp"
#include "MLabEnvironment.hpp"
#include "MLabSpan.hpp"
#include "MLabValue.hpp"

#include <chrono>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace mlab {

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
// Built-in constant names (shared between Engine and TreeWalker)
// ============================================================
extern const std::unordered_set<std::string> kBuiltinNames;

// ============================================================
// Runtime error with source location
// ============================================================
class MLabError : public std::runtime_error
{
public:
    // Single-frame error (current behavior)
    MLabError(const std::string &msg,
              int line = 0,
              int col = 0,
              const std::string &funcName = "")
        : std::runtime_error(msg) // what() = raw message, no formatting
        , line_(line)
        , col_(col)
        , funcName_(funcName)
    {}

    int line() const { return line_; }
    int col() const { return col_; }
    const std::string &funcName() const { return funcName_; }

    // Formatted for user display: "Error at line 15, column 3 in 'foo':\n  msg"
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
        return result;
    }

private:
    int line_;
    int col_;
    std::string funcName_;
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

} // namespace mlab