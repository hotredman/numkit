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
using ExternalFunc = std::function<void(Span<const MValue> args, size_t nargout, Span<MValue> outs)>;

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
    MLabError(const std::string &msg, int line = 0, int col = 0)
        : std::runtime_error(msg)
        , line_(line)
        , col_(col)
    {}
    int line() const { return line_; }
    int col() const { return col_; }

private:
    int line_;
    int col_;
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