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

    // Dispatch helpers
    void executeBinaryOp(OpCode op, MValue &dst, const MValue &a, const MValue &b);
    void executeUnaryOp(OpCode op, MValue &dst, const MValue &src);
};

} // namespace mlab
