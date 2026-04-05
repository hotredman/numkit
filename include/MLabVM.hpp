// include/MLabVM.hpp
#pragma once

#include "MLabBytecode.hpp"
#include "MLabDebugger.hpp"
#include "MLabValue.hpp"

#include <unordered_map>
#include <vector>

namespace mlab {

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
    ExecStatus startExecution(const BytecodeChunk &chunk, const MValue *args = nullptr, uint8_t nargs = 0);
    ExecStatus resumeExecution();
    bool isPaused() const { return !frames_.empty(); }
    MValue takeResult() { return std::move(lastResult_); }

    struct PausedState;
    std::unique_ptr<PausedState> savePausedState();
    void restorePausedState(std::unique_ptr<PausedState> state);

    void setCompiledFuncs(const std::unordered_map<std::string, BytecodeChunk> *funcs)
    {
        compiledFuncs_ = funcs;
    }

    // After execute(), get exported variables for environment sync
    const std::vector<std::pair<std::string, MValue>> &lastVarMap() const { return lastVarMap_; }
    void clearLastVarMap() { lastVarMap_.clear(); }

    // Call depth: 0 at top-level, +1 per user function call
    int callDepth() const { return std::max(0, static_cast<int>(frames_.size()) - 1); }

    void setMaxRecursionDepth(int d) { maxRecursion_ = d; }

private:
    Engine &engine_;
    const std::unordered_map<std::string, BytecodeChunk> *compiledFuncs_ = nullptr;

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

    // Export top-level variables to lastVarMap_ and globalStore
    void exportTopLevelVariables();

    // Exception helpers for dispatch loop (frame-aware)
    bool dispatchTryCatch(const char *msg, const char *identifier);
    [[noreturn]] void enrichAndThrow(const std::exception &ex,
                                     const Instruction *ip,
                                     const BytecodeChunk &chunk);

    // Helpers
    void forSetVar(MValue &varReg, const ForState &fs);

    // ── Debugger ────────────────────────────────────────────
    DebugController *debugCtl();
};

} // namespace mlab