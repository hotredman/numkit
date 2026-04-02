// include/MLabCompiler.hpp
#pragma once

#include "MLabAst.hpp"
#include "MLabBytecode.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace mlab {

class Engine;

class Compiler
{
public:
    explicit Compiler(Engine &engine);

    BytecodeChunk compile(const ASTNode *ast);

    // Compile a function definition into a BytecodeChunk
    BytecodeChunk compileFunction(const ASTNode *funcDef);

    // Access compiled function table
    const std::unordered_map<std::string, BytecodeChunk> &compiledFuncs() const
    {
        return compiledFuncs_;
    }

    // Debug: dump bytecode
    static std::string disassemble(const BytecodeChunk &chunk);

private:
    Engine &engine_;

    // Current chunk being compiled
    BytecodeChunk chunk_;

    // Register allocation: variable name → register index
    std::unordered_map<std::string, uint8_t> varRegisters_;
    uint8_t nextReg_ = 0;

    // Compiled function table (persists across compile() calls)
    std::unordered_map<std::string, BytecodeChunk> compiledFuncs_;

    // Allocate a register for a variable (or return existing)
    uint8_t varReg(const std::string &name);
    // Allocate a temporary register
    uint8_t tempReg();

    // Emit instructions
    void emit(Instruction instr);
    void emitABC(OpCode op, uint8_t a, uint8_t b, uint8_t c);
    void emitAB(OpCode op, uint8_t a, uint8_t b);
    void emitAD(OpCode op, uint8_t a, int16_t d);
    void emitA(OpCode op, uint8_t a);
    void emitD(OpCode op, int16_t d);
    void emitNone(OpCode op);

    // Current instruction index (for patching jumps)
    size_t currentPos() const;
    // Patch a jump instruction's offset at given position
    void patchJump(size_t instrPos, int16_t offset);

    // Add constant to pool, return index
    int16_t addConstant(double value);
    int16_t addStringConstant(const std::string &s);

    // Compile AST nodes — return register holding result
    uint8_t compileNode(const ASTNode *node);
    uint8_t compileBlock(const ASTNode *node);
    uint8_t compileNumber(const ASTNode *node);
    uint8_t compileString(const ASTNode *node);
    uint8_t compileBool(const ASTNode *node);
    uint8_t compileIdentifier(const ASTNode *node);
    uint8_t compileAssign(const ASTNode *node);
    uint8_t compileBinaryOp(const ASTNode *node);
    uint8_t compileUnaryOp(const ASTNode *node);
    uint8_t compileExprStmt(const ASTNode *node);

    // Phase 2: control flow
    uint8_t compileIf(const ASTNode *node);
    uint8_t compileWhile(const ASTNode *node);
    uint8_t compileBreak(const ASTNode *node);
    uint8_t compileContinue(const ASTNode *node);

    // Phase 3: for-loop, colon, arrays
    uint8_t compileFor(const ASTNode *node);
    uint8_t compileColonExpr(const ASTNode *node);
    uint8_t compileMatrixLiteral(const ASTNode *node);
    uint8_t compileIndexExpr(const ASTNode *node);
    uint8_t compileIndexAssign(const ASTNode *node);

    // Phase 4+5: function calls
    uint8_t compileCall(const ASTNode *node);
    uint8_t compileFunctionDef(const ASTNode *node);
    uint8_t compileReturn(const ASTNode *node);

    // Break/continue loop patching
    struct LoopContext
    {
        std::vector<size_t> breakPatches;
        std::vector<size_t> continuePatches;
    };
    std::vector<LoopContext> loopStack_;
};

} // namespace mlab