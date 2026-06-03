// include/vm.hpp
#pragma once

#include <numkit/core/bytecode.hpp>
#include <numkit/core/debugger.hpp>
#include <numkit/core/environment.hpp>
#include <numkit/core/value.hpp>

#include <memory>
#include <unordered_map>
#include <vector>

namespace numkit {

class Engine;

// Result of a dispatch loop iteration — Completed or Paused (for debugger)
enum class ExecStatus : uint8_t { Completed, Paused };

class VM
{
public:
    explicit VM(Engine &engine);

    // Legacy API: runs to completion, throws DebugStopException on pause
    Value execute(const BytecodeChunk &chunk, const Value *args = nullptr, uint8_t nargs = 0);

    // Debug-aware API: can return Paused (state preserved for resume)
    // initialAction controls the debugger's step mode at entry:
    //   StepInto (default) — pause on the first source line
    //   Continue           — run until a breakpoint is hit
    ExecStatus startExecution(const BytecodeChunk &chunk,
                              const Value *args = nullptr,
                              uint8_t nargs = 0,
                              DebugAction initialAction = DebugAction::StepInto);
    ExecStatus resumeExecution();
    bool isPaused() const { return !frames_.empty(); }
    Value takeResult() { return std::move(lastResult_); }

    struct PausedState;
    std::unique_ptr<PausedState> savePausedState();
    void restorePausedState(std::unique_ptr<PausedState> state);

    // Point the VM at the workspace-scope function table. Single-map
    // form is kept for tests / callers that don't run under a
    // script scope; anything running user scripts should also pass a
    // script-local bucket so `clear all` mid-call doesn't strand the
    // surrounding script's local functions.
    void setCompiledFuncs(const std::unordered_map<std::string, BytecodeChunk> *funcs)
    {
        compiledFuncs_ = funcs;
        scriptLocalFuncs_ = nullptr;
    }
    void setCompiledFuncs(const std::unordered_map<std::string, BytecodeChunk> *funcs,
                          const std::unordered_map<std::string, BytecodeChunk> *scriptLocal)
    {
        compiledFuncs_ = funcs;
        scriptLocalFuncs_ = scriptLocal;
    }

    // Unified lookup — tries the script-local bucket first, then
    // workspace. Returns nullptr when the name isn't a compiled
    // function (caller falls through to built-ins / external funcs).
    const BytecodeChunk *findCompiledFunc(const std::string &name) const
    {
        if (scriptLocalFuncs_) {
            auto it = scriptLocalFuncs_->find(name);
            if (it != scriptLocalFuncs_->end())
                return &it->second;
        }
        if (compiledFuncs_) {
            auto it = compiledFuncs_->find(name);
            if (it != compiledFuncs_->end())
                return &it->second;
        }
        return nullptr;
    }

    // After execute(), get exported variables for environment sync
    const std::vector<std::pair<std::string, Value>> &lastVarMap() const { return lastVarMap_; }
    void clearLastVarMap() { lastVarMap_.clear(); }

    // Set dynamic variables map on the current (paused) frame.
    // The map is NOT owned by the VM — caller must keep it alive.
    void setFrameDynVars(std::unordered_map<std::string, Value> *dv);

    // Mutable view of the current (top) frame — used by DebugWorkspace to
    // resolve register pointers by variable name.
    struct FrameView
    {
        const BytecodeChunk *chunk = nullptr;
        Value *registers = nullptr;
    };
    FrameView currentFrameView()
    {
        if (frames_.empty())
            return {};
        auto &f = frames_.back();
        return { f.chunk, f.R };
    }

    // Call depth: 0 at top-level, +1 per user function call
    int callDepth() const { return std::max(0, static_cast<int>(frames_.size()) - 1); }

    // The class declaring the currently-executing method/constructor frame
    // (empty if the top frame is an ordinary function or there is no frame) —
    // drives classdef member-access enforcement for VM-run bodies.
    const std::string &currentMethodClass() const
    {
        static const std::string kEmpty;
        return frames_.empty() ? kEmpty : frames_.back().ownerClass;
    }
    bool currentMethodIsCtor() const
    {
        return !frames_.empty() && frames_.back().isCtor;
    }

    void setMaxRecursionDepth(int d) { maxRecursion_ = d; }

    // ── Frame walking for assignin / evalin / inputname ──
    //
    // Walk frames_ from back, treating frames_.size()-1 as "n=0" (the
    // caller of the running builtin). Skips the script-level frame
    // (index 0 = top-level, returns workspaceEnv there). Lazy-allocates
    // frame.env for any user-function frame visited.
    Environment *callerEnvAtDepth(int n);

    // Variable write-through to the n-th caller's frame register if
    // the name is in that frame's chunk.varMap. No-op otherwise. The
    // env-side write is the caller's responsibility (engine.cpp).
    void assignInCallerFrame(int n, const std::string &name, const Value &val);

    // Scope-eval support (used by Engine::eval(src, scope) and the
    // evalin / eval / run builtins). When non-null, the inner exec's
    // top-level frame uses this env for currentCallEnv() instead of
    // workspaceEnv — so imports and ctx.env-based writes go to scope.
    // Saved/restored across re-entrant execute() boundaries via
    // PausedState.
    Environment *inheritedScope_ = nullptr;

    // Find a frame whose lazy env equals `env` (any depth) and write
    // `val` into the register slot for `name` if name is in that
    // frame's chunk.varMap. Used by Engine::syncVMToScope to push
    // inner-eval results back into the outer caller's static slots.
    void writeToFrameMatchingEnv(Environment *env, const std::string &name,
                                 const Value &val);

    // Caller arg names of the innermost user-function frame (the
    // function that contains the running builtin's call site). Empty
    // when at top-level. Used by inputname(k).
    const std::vector<std::string> &currentFrameCallerArgNames() const
    {
        static const std::vector<std::string> kEmpty;
        if (frames_.size() < 2) return kEmpty;
        return frames_.back().callerArgNames;
    }

    // ── Scoped-eval read-side support ──
    //
    // Build a snapshot of caller-frame variables (statically allocated
    // R[reg] entries plus any per-frame dynVars overlay) so that an
    // inner scoped eval can resolve bare identifiers that exist only
    // in the caller's registers. Empty if no matching frame.
    std::unordered_map<std::string, Value>
    snapshotFrameVars(Environment *frameEnv);

    // Plug `dv` in as the next-pushed top-level frame's dynVars. The
    // pointer must outlive that frame's execution. Used by
    // Engine::eval(src, scope) to expose caller's vars to inner exec.
    void setNextFrameDynVars(std::unordered_map<std::string, Value> *dv)
    {
        nextFrameDynVars_ = dv;
    }

private:
    Engine &engine_;
    std::unordered_map<std::string, Value> *nextFrameDynVars_ = nullptr;
    const std::unordered_map<std::string, BytecodeChunk> *compiledFuncs_ = nullptr;
    const std::unordered_map<std::string, BytecodeChunk> *scriptLocalFuncs_ = nullptr;

    // Register stack — Value directly (16 bytes each)
    static constexpr size_t kRegStackSize = 256 * 50;
    std::vector<Value> regStack_;
    size_t regStackTop_ = 0;
    Value *R_ = nullptr;

    // ── Explicit call frame stack (non-recursive VM) ────────
    struct CallFrame
    {
        const BytecodeChunk *chunk = nullptr;
        const Instruction *ip = nullptr; // saved instruction pointer
        Value *R = nullptr;             // register base for this frame
        size_t regBase = 0;              // offset in regStack_
        uint8_t nregs = 0;              // register count
        size_t forStackBase = 0;         // forStack_.size() at frame entry
        size_t tryStackBase = 0;         // tryStack_.size() at frame entry
        uint8_t destReg = 0;            // caller register for return value
        bool isMultiReturn = false;      // CALL_MULTI?
        uint8_t outBase = 0;            // multi-return: output base register
        uint8_t nout = 0;               // multi-return: output count
        size_t nargout = 1;
        // classdef method/ctor frames only: the declaring class (drives
        // member-access enforcement; read on demand, never a separate stack,
        // so it is naturally exception-safe). Empty for ordinary functions.
        std::string ownerClass;
        bool isCtor = false;

        // Dynamic variables — fallback for variables not in static varMap.
        // Used by debug eval to inject variables created at breakpoints.
        std::unordered_map<std::string, Value> *dynVars = nullptr;

        // Lazy per-frame env for scope-local imports / env-resident
        // builtin state. parent = workspaceEnv. Allocated on first
        // currentCallEnv() inside a user function; nullptr at top level.
        std::unique_ptr<Environment> env;

        // Names of the bare-identifier args at the caller's CALL site,
        // copied from caller's chunk.callSiteArgNames at pushCallFrame.
        // inputname(k) reads element [k-1] from this vector. Empty
        // string for non-identifier args, empty vector for top-level
        // and non-tracked call sites.
        std::vector<std::string> callerArgNames;
    };
    std::vector<CallFrame> frames_;
    Value lastResult_;

    // For-loop state
    struct ForState
    {
        Value range;
        const double *data = nullptr;
        const void *rawData = nullptr; // for char/logical
        ValueType rangeType = ValueType::DOUBLE;
        size_t index = 0;
        size_t count = 0;
        size_t rows = 0;
        // Lazy colon range: `for v = start:stop` and `for v = start:step:stop`
        // skip the COLON allocation by storing scalars here. forSetVar checks
        // lazy first; when true, range/data/rawData are unused. Only valid
        // when rangeType == ValueType::DOUBLE and rows == 1 (scalar each iter).
        bool   lazy = false;
        double lazyStart = 0.0;
        double lazyStep  = 1.0;
    };
    std::vector<ForState> forStack_;

    // Try/catch handler stack
    struct TryHandler
    {
        const Instruction *catchIp; // where to jump on exception
        uint8_t exReg;              // register for exception struct
        size_t forStackSize;        // forStack_ size at TRY_BEGIN (restore on catch)
        size_t frameIndex;          // index in frames_ that owns this handler
    };
    std::vector<TryHandler> tryStack_;

public:
    // Defined here (after private types) so callers can destroy unique_ptr<PausedState>
    struct PausedState
    {
        std::vector<CallFrame> frames;
        std::vector<ForState> forStack;
        std::vector<TryHandler> tryStack;
        std::vector<Value> regSnapshot;
        size_t regStackTop = 0;
        Value lastResult;
        // BUG #3: outer dispatch loop holds `auto &resolvedFuncs =
        // chunkCallCache_[frame.chunk];` (a reference into the map's
        // node memory). startExecution() on the inner re-entry clears
        // chunkCallCache_, which destroys that node and dangles the
        // reference. unordered_map move preserves node addresses, so
        // saving here + restoring on exit keeps the outer's reference
        // pointing at live memory.
        std::unordered_map<const BytecodeChunk *,
                           std::vector<const BytecodeChunk *>>
            chunkCallCache;
    };
private:

    int maxRecursion_ = 200;

    // Per-chunk call target cache
    std::unordered_map<const BytecodeChunk *, std::vector<const BytecodeChunk *>> chunkCallCache_;

    // Exported variables after execution
    std::vector<std::pair<std::string, Value>> lastVarMap_;

    // Multi-return buffer: RET_MULTI stores values here instead of cell-packing
    static constexpr size_t kMaxReturns = 16;
    Value returnBuf_[kMaxReturns];
    uint8_t returnCount_ = 0;

    // ── Core dispatch loop (non-recursive) ──────────────────
    ExecStatus dispatchLoop();

    // Frame management
    // ctorSeed (when non-null) pre-binds the new frame's first return variable
    // to *ctorSeed before the body runs — used to seed a classdef constructor's
    // output variable with the default instance (MATLAB constructor semantics).
    void pushCallFrame(const BytecodeChunk &funcChunk, const Value *args, uint8_t nargs,
                       uint8_t destReg, size_t nargout,
                       bool isMulti = false, uint8_t outBase = 0, uint8_t nout = 0,
                       const std::string &ownerClass = std::string(), bool isCtor = false,
                       const Value *ctorSeed = nullptr);
    void popCallFrame(Value retVal);

    // Returns the Environment a builtin call should see for ctx.env.
    // Top-level (frames_.size() <= 1): workspaceEnv directly.
    // Inside a user function: lazily-allocated frame.env, parent =
    // workspaceEnv. Function-local imports / state live in this env and
    // are dropped when the frame is popped.
    Environment *currentCallEnv();

    // Export top-level variables to lastVarMap_ and globalsEnv
    void exportTopLevelVariables();

    // Exception helpers for dispatch loop (frame-aware)
    bool dispatchTryCatch(const char *msg, const char *identifier);
    [[noreturn]] void enrichAndThrow(const std::exception &ex,
                                     const Instruction *ip,
                                     const BytecodeChunk &chunk);

    // Helpers
    void forSetVar(Value &varReg, const ForState &fs);

    // Dispatch helpers (extracted from dispatchLoop for readability)
    Value binarySlowPath(OpCode op, const Value &lhs, const Value &rhs);
    Value unarySlowPath(OpCode op, const Value &operand);
    void execCallBuiltin(const Instruction &I, Value *R);
    bool execCallIndirect(const Instruction &I, Value *R,
                          CallFrame &frame, const Instruction *ip);
    void execIndirectIndex(const Instruction &I, Value *R);
    void execDisplay(const Instruction &I, Value *R, const BytecodeChunk &chunk);
    void execWho(const Instruction &I, Value *R, const BytecodeChunk &chunk);
    void execWhos(const Instruction &I, Value *R, const BytecodeChunk &chunk);

    // ── Debugger ────────────────────────────────────────────
    DebugController *debugCtl();
};

} // namespace numkit