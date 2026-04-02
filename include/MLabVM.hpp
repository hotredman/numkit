// include/MLabVM.hpp
#pragma once

#include "MLabBytecode.hpp"
#include "MLabValue.hpp"

#include <unordered_map>
#include <vector>

namespace mlab {

class Engine;

class VM
{
public:
    explicit VM(Engine &engine);

    MValue execute(const BytecodeChunk &chunk, const MValue *args = nullptr, uint8_t nargs = 0);

    void setCompiledFuncs(const std::unordered_map<std::string, BytecodeChunk> *funcs)
    {
        compiledFuncs_ = funcs;
    }

    // After execute(), get exported variables for environment sync
    const std::vector<std::pair<std::string, MValue>> &lastVarMap() const { return lastVarMap_; }

private:
    Engine &engine_;
    const std::unordered_map<std::string, BytecodeChunk> *compiledFuncs_ = nullptr;

    // Register stack — MValue directly (16 bytes each)
    static constexpr size_t kRegStackSize = 256 * 50;
    std::vector<MValue> regStack_;
    size_t regStackTop_ = 0;
    MValue *R_ = nullptr;

    // For-loop state
    struct ForState
    {
        MValue range;
        const double *data = nullptr;
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
    };
    std::vector<TryHandler> tryStack_;

    // Recursion guard
    int recursionDepth_ = 0;
    static constexpr int kMaxRecursion = 50;

    // Per-chunk call target cache
    std::unordered_map<const BytecodeChunk *, std::vector<const BytecodeChunk *>> chunkCallCache_;

    // Exported variables after execution
    std::vector<std::pair<std::string, MValue>> lastVarMap_;

    // Internal dispatch
    MValue executeInternal(const BytecodeChunk &chunk);

    // User function call
    MValue callUserFunc(const BytecodeChunk &funcChunk, const MValue *args, uint8_t nargs);

    std::vector<MValue> callUserFuncMulti(const BytecodeChunk &funcChunk,
                                          const MValue *args,
                                          uint8_t nargs,
                                          size_t nout);

    // Helpers
    void executeHorzcat(MValue &dst, const MValue *regs, uint8_t count);
    void executeVertcat(MValue &dst, const MValue *regs, uint8_t count);
    void forSetVar(MValue &varReg, const ForState &fs);
};

} // namespace mlab