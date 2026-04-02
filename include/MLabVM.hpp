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

    // Register file — VMValue for fast scalar path
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

    // Helpers
    void executeHorzcat(VMValue &dst, const VMValue *regs, uint8_t count);
    void executeVertcat(VMValue &dst, const VMValue *regs, uint8_t count);
    void forSetVar(VMValue &varReg, const ForState &fs);
    MValue executeCall(const BytecodeChunk &funcChunk, const MValue *args, uint8_t nargs);
};

} // namespace mlab