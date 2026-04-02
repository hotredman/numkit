// include/MLabVM.hpp
#pragma once

#include "MLabBytecode.hpp"
#include "MLabVMValue.hpp"

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

private:
    Engine &engine_;
    const std::unordered_map<std::string, BytecodeChunk> *compiledFuncs_ = nullptr;

    // Register stack — pre-allocated, no per-call allocation
    // Each function call pushes a frame of numRegisters onto the stack
    static constexpr size_t kRegStackSize = 256 * 500; // 256 regs × 500 recursion depth
    std::vector<VMValue> regStack_;                    // allocated once in constructor
    size_t regStackTop_ = 0;                           // current top of stack
    VMValue *R_ = nullptr;                             // pointer to current frame base

    // For-loop state
    struct ForState
    {
        MValue range;
        size_t index = 0;
        size_t count = 0;
    };
    std::vector<ForState> forStack_;

    // Recursion guard
    int recursionDepth_ = 0;
    static constexpr int kMaxRecursion = 500;

    // Internal execute — operates on R_ (current frame)
    VMValue executeInternal(const BytecodeChunk &chunk);

    // User function call
    VMValue callUserFunc(const BytecodeChunk &funcChunk, const VMValue *args, uint8_t nargs);

    // Helpers
    void executeHorzcat(VMValue &dst, const VMValue *regs, uint8_t count);
    void executeVertcat(VMValue &dst, const VMValue *regs, uint8_t count);
    void forSetVar(VMValue &varReg, const ForState &fs);
};

} // namespace mlab