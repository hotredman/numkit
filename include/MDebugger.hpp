// include/MLabDebugger.hpp
//
// Debugger infrastructure for MLab interpreter.
//
// Design principles:
//   - Zero overhead when no observer is attached
//   - Unified interface for both VM and TreeWalker backends
//   - Read-only DebugContext (no mutation through debug API)
//   - No coupling to UI — observer is abstract
//   - No heap allocation for debug frames
//   - Shared step logic — no duplication between backends
//
#pragma once

#include "MValue.hpp"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace mlab {

class Environment;
struct BytecodeChunk;

// ============================================================
// DebugAction — returned by observer to control execution
// ============================================================
enum class DebugAction : uint8_t {
    Continue, // run until next breakpoint
    StepOver, // next line in current scope (skip function calls)
    StepInto, // step into function call (break on very next line)
    StepOut,  // run until current function returns
    Stop      // abort execution
};

// ============================================================
// StackFrame — one entry in the call stack
//
// All data inline — no heap allocation, no void*.
// VM sets chunk + registers, TW sets env. Unused fields are null.
// ============================================================
struct StackFrame
{
    std::string functionName; // "<script>" for top-level
    uint16_t line = 0;
    uint16_t col = 0;

    // VM backend: variable access via registers
    const BytecodeChunk *chunk = nullptr;
    const MValue *registers = nullptr;

    // TW backend: variable access via Environment
    Environment *env = nullptr;

    // Variable inspection (works for both backends)
    struct VarEntry
    {
        std::string name;
        const MValue *value;
    };
    std::vector<VarEntry> variables() const;
};

// ============================================================
// DebugContext — read-only view of execution state
//
// Passed to observer callbacks. callStack back() = current frame.
// ============================================================
struct DebugContext
{
    uint16_t line = 0;
    uint16_t col = 0;
    const std::string *functionName = nullptr;

    // Source code text for display. Available in VM backend (from BytecodeChunk).
    // Null in TreeWalker backend (TW operates on AST, not compiled chunks).
    const std::string *sourceCode = nullptr;

    // Call stack: front() = top-level, back() = current frame
    const std::vector<StackFrame> *callStack = nullptr;

    const StackFrame *currentFrame() const
    {
        return (callStack && !callStack->empty()) ? &callStack->back() : nullptr;
    }

    const MValue *getVariable(const std::string &name) const;
    std::vector<std::string> variableNames() const;
};

// ============================================================
// DebugObserver — abstract interface implemented by UI/CLI
// ============================================================
class DebugObserver
{
public:
    virtual ~DebugObserver() = default;
    virtual DebugAction onLine(const DebugContext &ctx) = 0;
    virtual DebugAction onBreakpoint(const DebugContext &ctx) = 0;
    virtual void onError(const DebugContext &ctx, const std::string &msg) = 0;
    virtual void onFunctionEntry(const DebugContext &ctx) = 0;
    virtual void onFunctionExit(const DebugContext &ctx) = 0;
};

// ============================================================
// BreakpointManager — tracks line breakpoints
// ============================================================
class BreakpointManager
{
public:
    struct Breakpoint
    {
        int id;
        uint16_t line;
        bool enabled = true;
    };

    int addBreakpoint(uint16_t line);
    void removeBreakpoint(int id);
    void enableBreakpoint(int id, bool enabled);
    void clearAll();
    bool shouldBreak(uint16_t line) const;
    const std::vector<Breakpoint> &breakpoints() const { return breakpoints_; }

private:
    std::vector<Breakpoint> breakpoints_;
    int nextId_ = 1;
    mutable std::unordered_map<uint16_t, size_t> lineIndex_;
    mutable bool indexDirty_ = true;
    void rebuildIndex() const;
};

// ============================================================
// DebugController — shared step/breakpoint logic
//
// Both VM and TW call checkLine() on line change.
// Owns the call stack and step state.
// No backend-specific code.
//
// Lifetime: observer and bpm must outlive the controller.
// Do not call Engine::setDebugObserver() during execution.
// ============================================================
class DebugController
{
public:
    explicit DebugController(DebugObserver *observer, BreakpointManager *bpm)
        : observer_(observer)
        , bpm_(bpm)
    {}

    // Reset for new execution.
    // Default StepInto: observer's onLine() fires on the first source line,
    // giving it a chance to choose the execution mode (Continue, StepOver, etc).
    // Pass Continue to skip the initial stop (breakpoints still work).
    void reset(DebugAction initialAction = DebugAction::StepInto);

    // Called by backend on source line change.
    // Returns false if Stop requested.
    bool checkLine(uint16_t line, uint16_t col, int callDepth);

    // Call stack management
    void pushFrame(StackFrame frame);
    void popFrame();

    // RAII guard: pushes frame on construction, pops on destruction (even on exception).
    // Movable (for std::optional), not copyable.
    class FrameGuard
    {
    public:
        FrameGuard(DebugController &ctl, StackFrame frame)
            : ctl_(&ctl)
        {
            ctl_->pushFrame(std::move(frame));
        }
        ~FrameGuard()
        {
            if (ctl_)
                ctl_->popFrame();
        }
        FrameGuard(FrameGuard &&o) noexcept
            : ctl_(o.ctl_)
        {
            o.ctl_ = nullptr;
        }
        FrameGuard &operator=(FrameGuard &&) = delete;
        FrameGuard(const FrameGuard &) = delete;
        FrameGuard &operator=(const FrameGuard &) = delete;

    private:
        DebugController *ctl_;
    };

    std::vector<StackFrame> &callStack() { return callStack_; }
    const std::vector<StackFrame> &callStack() const { return callStack_; }
    StackFrame *currentFrame() { return callStack_.empty() ? nullptr : &callStack_.back(); }

    DebugContext makeContext(uint16_t line, uint16_t col) const;
    DebugContext makeContext() const;

    // Set action for resume after pause (called before re-entering dispatch loop)
    void setResumeAction(DebugAction action, int callDepth);

    // Restore lastLine_ (used after debug eval to avoid re-triggering on the same line)
    void setLastLine(uint16_t line) { lastLine_ = line; }

private:
    DebugObserver *observer_;
    BreakpointManager *bpm_;
    std::vector<StackFrame> callStack_;
    DebugAction stepAction_ = DebugAction::Continue;
    int stepDepth_ = 0;
    uint16_t lastLine_ = 0;

    void applyAction(DebugAction action, int callDepth);
};

// ============================================================
// DebugStopException
// ============================================================
class DebugStopException : public std::runtime_error
{
public:
    DebugStopException()
        : std::runtime_error("Execution stopped by debugger")
    {}
};

} // namespace mlab