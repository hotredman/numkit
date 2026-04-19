// include/MVM.hpp
#pragma once

#include "MBytecode.hpp"
#include "MDebugger.hpp"
#include "MValue.hpp"

#include <unordered_map>
#include <vector>

namespace numkit::m::m {

class Engine;

// Result of a dispatch loop iteration — Completed or Paused (for debugger)
enum class ExecStatus : uint8_t { Completed, Paused };

class VM
{
public:
    explicit VM(Engine &engine);

    // Legacy API: runs to completion, throws DebugStopException on pause
    MValue execute(const BytecodeChunk &chunk, const MValue *args = nullptr, uint8_t nargs = 0);

    // Debug-aware API: can return Paused (state preserved for resume)
    // initialAction controls the debugger's step mode at entry:
    //   StepInto (default) — pause on the first source line
    //   Continue           — run until a breakpoint is hit
    ExecStatus startExecution(const BytecodeChunk &chunk,
                              const MValue *args = nullptr,
                              uint8_t nargs = 0,
                              DebugAction initialAction = DebugAction::StepInto);
    ExecStatus resumeExecution();
    bool isPaused() const { return !frames_.empty(); }
    MValue takeResult() { return std::move(lastResult_); }

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
    const std::vector<std::pair<std::string, MValue>> &lastVarMap() const { return lastVarMap_; }
    void clearLastVarMap() { lastVarMap_.clear(); }

    // Set dynamic variables map on the current (paused) frame.
    // The map is NOT owned by the VM — caller must keep it alive.
    void setFrameDynVars(std::unordered_map<std::string, MValue> *dv);

    // Mutable view of the current (top) frame — used by DebugWorkspace to
    // resolve register pointers by variable name.
    struct FrameView
    {
        const BytecodeChunk *chunk = nullptr;
        MValue *registers = nullptr;
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

    void setMaxRecursionDepth(int d) { maxRecursion_ = d; }

private:
    Engine &engine_;
    const std::unordered_map<std::string, BytecodeChunk> *compiledFuncs_ = nullptr;
    const std::unordered_map<std::string, BytecodeChunk> *scriptLocalFuncs_ = nullptr;

    // Register stack — MValue directly (16 bytes each)
    static constexpr size_t kRegStackSize = 256 * 50;
    std::vector<MValue> regStack_;
    size_t regStackTop_ = 0;
    MValue *R_ = nullptr;

    // ── Explicit call frame stack (non-recursive VM) ────────
    struct CallFrame
    {
        const BytecodeChunk *chunk = nullptr;
        const Instruction *ip = nullptr; // saved instruction pointer
        MValue *R = nullptr;             // register base for this frame
        size_t regBase = 0;              // offset in regStack_
        uint8_t nregs = 0;              // register count
        size_t forStackBase = 0;         // forStack_.size() at frame entry
        size_t tryStackBase = 0;         // tryStack_.size() at frame entry
        uint8_t destReg = 0;            // caller register for return value
        bool isMultiReturn = false;      // CALL_MULTI?
        uint8_t outBase = 0;            // multi-return: output base register
        uint8_t nout = 0;               // multi-return: output count
        size_t nargout = 1;

        // Dynamic variables — fallback for variables not in static varMap.
        // Used by debug eval to inject variables created at breakpoints.
        std::unordered_map<std::string, MValue> *dynVars = nullptr;
    };
    std::vector<CallFrame> frames_;
    MValue lastResult_;

    // For-loop state
    struct ForState
    {
        MValue range;
        const double *data = nullptr;
        const void *rawData = nullptr; // for char/logical
        MType rangeType = MType::DOUBLE;
        size_t index = 0;
        size_t count = 0;
        size_t rows = 0;
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
        std::vector<MValue> regSnapshot;
        size_t regStackTop = 0;
        MValue lastResult;
    };
private:

    int maxRecursion_ = 200;

    // Per-chunk call target cache
    std::unordered_map<const BytecodeChunk *, std::vector<const BytecodeChunk *>> chunkCallCache_;

    // Exported variables after execution
    std::vector<std::pair<std::string, MValue>> lastVarMap_;

    // Multi-return buffer: RET_MULTI stores values here instead of cell-packing
    static constexpr size_t kMaxReturns = 16;
    MValue returnBuf_[kMaxReturns];
    uint8_t returnCount_ = 0;

    // ── Core dispatch loop (non-recursive) ──────────────────
    ExecStatus dispatchLoop();

    // Frame management
    void pushCallFrame(const BytecodeChunk &funcChunk, const MValue *args, uint8_t nargs,
                       uint8_t destReg, size_t nargout,
                       bool isMulti = false, uint8_t outBase = 0, uint8_t nout = 0);
    void popCallFrame(MValue retVal);

    // Export top-level variables to lastVarMap_ and globalsEnv
    void exportTopLevelVariables();

    // Exception helpers for dispatch loop (frame-aware)
    bool dispatchTryCatch(const char *msg, const char *identifier);
    [[noreturn]] void enrichAndThrow(const std::exception &ex,
                                     const Instruction *ip,
                                     const BytecodeChunk &chunk);

    // Helpers
    void forSetVar(MValue &varReg, const ForState &fs);

    // Dispatch helpers (extracted from dispatchLoop for readability)
    MValue binarySlowPath(OpCode op, const MValue &lhs, const MValue &rhs);
    MValue unarySlowPath(OpCode op, const MValue &operand);
    void execCallBuiltin(const Instruction &I, MValue *R);
    bool execCallIndirect(const Instruction &I, MValue *R,
                          CallFrame &frame, const Instruction *ip);
    void execIndirectIndex(const Instruction &I, MValue *R);
    void execDisplay(const Instruction &I, MValue *R, const BytecodeChunk &chunk);
    void execWho(const Instruction &I, MValue *R, const BytecodeChunk &chunk);
    void execWhos(const Instruction &I, MValue *R, const BytecodeChunk &chunk);

    // ── Debugger ────────────────────────────────────────────
    DebugController *debugCtl();
};

} // namespace numkit::m::m