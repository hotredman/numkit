// value/include/numkit/value/error.hpp
//
// numkit::Error — the runtime error type carrying an optional source
// location (line/col/function/context) and a MATLAB-style identifier.
//
// An L0 type: it depends only on the C++ standard library, so every layer —
// value, ops, the toolboxes, and core — can throw and catch it. (It used to
// live in core/types.hpp; moving it down lets the foundation/toolboxes report
// errors without pulling in the engine.) types.hpp re-includes this header so
// existing `numkit::Error` users are unaffected.

#pragma once

#include <stdexcept>
#include <string>

namespace numkit {

// ============================================================
// Runtime error with source location
// ============================================================
class Error : public std::runtime_error
{
public:
    Error(const std::string &msg,
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

    // Fill in source-location fields only if they are currently empty.
    // Called by outer catch blocks (TreeWalker::execNode, VM::dispatchLoop)
    // to enrich errors thrown deeper in the call stack (e.g. from public
    // C++ library APIs that don't know their source line) without
    // overwriting more-precise location already attached by the thrower.
    void attachIfMissing(int line,
                         int col,
                         const std::string &funcName = "",
                         const std::string &context = "")
    {
        if (line_ == 0)
            line_ = line;
        if (col_ == 0)
            col_ = col;
        if (funcName_.empty() && !funcName.empty())
            funcName_ = funcName;
        if (context_.empty() && !context.empty())
            context_ = context;
    }

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
    std::string identifier_; // e.g. "numkit:badInput"
};

} // namespace numkit
