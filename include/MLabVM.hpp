// include/MLabVM.hpp
#pragma once

#include "MLabBytecode.hpp"
#include "MLabValue.hpp"

#include <vector>

namespace mlab {

class Engine;

class VM
{
public:
    explicit VM(Engine &engine);

    MValue execute(const BytecodeChunk &chunk);

private:
    Engine &engine_;

    // Register file
    std::vector<MValue> registers_;

    // For-loop iteration state stack (for nested loops)
    struct ForState
    {
        MValue range;     // copy of range being iterated
        size_t index = 0; // current column index
        size_t count = 0; // total columns
    };
    std::vector<ForState> forStack_;

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
};

} // namespace mlab