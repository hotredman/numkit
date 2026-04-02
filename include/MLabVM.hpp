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

    // Public API: MValue in/out
    MValue execute(const BytecodeChunk &chunk, const MValue *args = nullptr, uint8_t nargs = 0);

    void setCompiledFuncs(const std::unordered_map<std::string, BytecodeChunk> *funcs)
    {
        compiledFuncs_ = funcs;
    }

private:
    Engine &engine_;
    const std::unordered_map<std::string, BytecodeChunk> *compiledFuncs_ = nullptr;

    // Register file
    std::vector<VMValue> registers_;

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

    // Internal execute — returns VMValue, no MValue conversion overhead
    VMValue executeInternal(const BytecodeChunk &chunk);

    // User function call — VMValue args, VMValue return, no MValue
    VMValue callUserFunc(const BytecodeChunk &funcChunk, const VMValue *args, uint8_t nargs);

    // Helpers
    void executeHorzcat(VMValue &dst, const VMValue *regs, uint8_t count);
    void executeVertcat(VMValue &dst, const VMValue *regs, uint8_t count);
    void forSetVar(VMValue &varReg, const ForState &fs);
};

} // namespace mlab