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

    // Set compiled function table (from Compiler)
    void setCompiledFuncs(const std::unordered_map<std::string, BytecodeChunk> *funcs)
    {
        compiledFuncs_ = funcs;
    }

private:
    Engine &engine_;

    // Compiled function table (owned by Compiler, just a pointer)
    const std::unordered_map<std::string, BytecodeChunk> *compiledFuncs_ = nullptr;

    // Register file (per-call, saved/restored on recursion)
    std::vector<MValue> registers_;

    // For-loop iteration state stack
    struct ForState
    {
        MValue range;
        size_t index = 0;
        size_t count = 0;
    };
    std::vector<ForState> forStack_;

    // Recursion depth guard
    int recursionDepth_ = 0;
    static constexpr int kMaxRecursion = 500;

    // Dispatch helpers
    void executeBinaryOp(OpCode op, MValue &dst, const MValue &a, const MValue &b);
    void executeUnaryOp(OpCode op, MValue &dst, const MValue &src);

    // Array helpers
    void executeColon(MValue &dst, double start, double stop);
    void executeColon3(MValue &dst, double start, double step, double stop);
    void executeHorzcat(MValue &dst, const MValue *regs, uint8_t count);
    void executeVertcat(MValue &dst, const MValue *regs, uint8_t count);

    // For-loop helpers
    void forSetVar(MValue &varReg, const ForState &fs);

    // Function call helper
    MValue executeCall(const BytecodeChunk &funcChunk, const MValue *args, uint8_t nargs);
};

} // namespace mlab