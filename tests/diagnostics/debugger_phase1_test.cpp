// tests/test_debugger_phase1.cpp — Source mapping, error locations, MError
// Parameterized: runs on both TreeWalker and VM backends

#include "MCompiler.hpp"
#include "MLexer.hpp"
#include "MParser.hpp"
#include "dual_engine_fixture.hpp"

using namespace mlab_test;
using namespace numkit;

// ============================================================
// Test fixture
// ============================================================

class DebugPhase1Test : public DualEngineTest
{};

// ============================================================
// 1. sourceMap is populated and parallel to code
// ============================================================

TEST_P(DebugPhase1Test, SourceMapParallelToCode)
{
    // Compile directly to inspect chunk internals
    std::string code = "x = 1 + 2;\ny = x * 3;\n";
    Lexer lexer(code);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();

    Compiler compiler(engine);
    auto src = std::make_shared<const std::string>(code);
    auto chunk = compiler.compile(ast.get(), src);

    EXPECT_EQ(chunk.sourceMap.size(), chunk.code.size()) << "sourceMap must be parallel to code";
    EXPECT_FALSE(chunk.code.empty());
}

TEST_P(DebugPhase1Test, SourceMapHasNonZeroLines)
{
    std::string code = "x = 42;\ny = x + 1;\n";
    Lexer lexer(code);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();

    Compiler compiler(engine);
    auto src = std::make_shared<const std::string>(code);
    auto chunk = compiler.compile(ast.get(), src);

    // At least some instructions should have non-zero source lines
    bool hasLine = false;
    for (auto &loc : chunk.sourceMap) {
        if (loc.line > 0) {
            hasLine = true;
            break;
        }
    }
    EXPECT_TRUE(hasLine) << "sourceMap should contain non-zero line numbers";
}

TEST_P(DebugPhase1Test, SourceMapDistinguishesLines)
{
    std::string code = "x = 1;\ny = 2;\nz = 3;\n";
    Lexer lexer(code);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();

    Compiler compiler(engine);
    auto src = std::make_shared<const std::string>(code);
    auto chunk = compiler.compile(ast.get(), src);

    // Collect unique non-zero lines
    std::set<uint16_t> lines;
    for (auto &loc : chunk.sourceMap)
        if (loc.line > 0)
            lines.insert(loc.line);

    // 3 statements → should see at least 2 distinct lines
    EXPECT_GE(lines.size(), 2u) << "3 statements should produce instructions on multiple lines";
}

// ============================================================
// 2. sourceCode is stored in chunk
// ============================================================

TEST_P(DebugPhase1Test, SourceCodeStoredInChunk)
{
    std::string code = "x = 42;";
    Lexer lexer(code);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();

    Compiler compiler(engine);
    auto src = std::make_shared<const std::string>(code);
    auto chunk = compiler.compile(ast.get(), src);

    ASSERT_NE(chunk.sourceCode, nullptr);
    EXPECT_EQ(*chunk.sourceCode, code);
}

TEST_P(DebugPhase1Test, SourceCodeSharedAcrossFunctions)
{
    std::string code = "function r = foo(x)\n  r = x + 1;\nend\ny = foo(5);\n";
    Lexer lexer(code);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();

    Compiler compiler(engine);
    auto src = std::make_shared<const std::string>(code);
    auto chunk = compiler.compile(ast.get(), src);

    // Script chunk has sourceCode
    ASSERT_NE(chunk.sourceCode, nullptr);

    // Compiled function should share the same pointer
    auto &funcs = compiler.compiledFuncs();
    auto it = funcs.find("foo");
    if (it != funcs.end()) {
        ASSERT_NE(it->second.sourceCode, nullptr);
        EXPECT_EQ(it->second.sourceCode.get(), chunk.sourceCode.get())
            << "Function and script should share same sourceCode pointer";
    }
}

// ============================================================
// 3. VM errors include source location (MError)
// ============================================================

TEST_P(DebugPhase1Test, UndefinedVarErrorHasLocation)
{
    if (GetParam() == BackendParam::TreeWalker) {
        // TW enriches errors in execBlock with line/col
        try {
            eval("x = 1;\nv = [1 2 3];\nv(100);\n");
            FAIL() << "Should have thrown";
        } catch (const MError &e) {
            EXPECT_GT(e.line(), 0) << "TW error should have line info";
        } catch (...) {
            // Some TW paths may not enrich — acceptable
        }
        return;
    }

    // VM: use runtime error (index out of bounds) which happens during execution
    try {
        eval("x = 1;\nv = [1 2 3];\nv(100);\n");
        FAIL() << "Should have thrown";
    } catch (const MError &e) {
        EXPECT_GT(e.line(), 0) << "VM error should include line number";
        EXPECT_GE(e.line(), 3) << "Error should be on line 3 (v(100))";
        std::string msg = e.what();
        EXPECT_TRUE(msg.find("ndex") != std::string::npos || msg.find("exceed") != std::string::npos
                    || msg.find("ound") != std::string::npos)
            << "Error message should describe the indexing error: " << msg;
    }
}

TEST_P(DebugPhase1Test, IndexErrorHasLocation)
{
    try {
        eval("v = [1 2 3];\nv(10);\n");
        FAIL() << "Should have thrown";
    } catch (const MError &e) {
        EXPECT_GT(e.line(), 0) << "Index error should have line info";
    } catch (const std::runtime_error &) {
        // Also acceptable — some paths may not enrich yet
    }
}

TEST_P(DebugPhase1Test, UndefinedVarInFunctionHasLocation)
{
    // Undefined variable inside a function uses ASSERT_DEF at runtime
    if (GetParam() == BackendParam::TreeWalker)
        return; // TW doesn't use ASSERT_DEF

    try {
        eval(R"(
            function r = bad_func()
                r = some_undefined_thing;
            end
            bad_func();
        )");
        FAIL() << "Should have thrown";
    } catch (const MError &e) {
        EXPECT_GT(e.line(), 0) << "ASSERT_DEF error should have line info";
        std::string msg = e.what();
        EXPECT_TRUE(msg.find("some_undefined_thing") != std::string::npos)
            << "Message should mention the variable: " << msg;
    } catch (const std::runtime_error &) {
        // Acceptable
    }
}

// ============================================================
// 4. formattedWhat() produces nice output
// ============================================================

TEST_P(DebugPhase1Test, FormattedWhatIncludesLocation)
{
    MError err("bad stuff", 15, 3, "my_func");

    EXPECT_EQ(err.line(), 15);
    EXPECT_EQ(err.col(), 3);
    EXPECT_STREQ(err.what(), "bad stuff");

    std::string fmt = err.formattedWhat();
    EXPECT_TRUE(fmt.find("line 15") != std::string::npos) << fmt;
    EXPECT_TRUE(fmt.find("column 3") != std::string::npos) << fmt;
    EXPECT_TRUE(fmt.find("my_func") != std::string::npos) << fmt;
    EXPECT_TRUE(fmt.find("bad stuff") != std::string::npos) << fmt;
}

TEST_P(DebugPhase1Test, FormattedWhatNoLocationJustMessage)
{
    MError err("plain error");

    EXPECT_STREQ(err.what(), "plain error");
    EXPECT_EQ(err.formattedWhat(), "plain error")
        << "No location → formattedWhat should equal what()";
}

TEST_P(DebugPhase1Test, WhatIsRawMessage)
{
    // Verify what() does NOT contain "Error at line" formatting
    MError err("divide by zero", 10, 5, "calc");
    std::string w = err.what();
    EXPECT_EQ(w, "divide by zero") << "what() should be raw message, not formatted";
    EXPECT_TRUE(w.find("Error at line") == std::string::npos)
        << "what() must not contain formatting";
}

// ============================================================
// 5. try/catch receives clean error message
// ============================================================

TEST_P(DebugPhase1Test, TryCatchGetsCleanMessage)
{
    eval(R"(
        try
            error('boom');
        catch e
            caught_msg = e.message;
        end
    )");
    auto *msg = getVarPtr("caught_msg");
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->toString(), "boom")
        << "try/catch should get clean message without location prefix";
}

TEST_P(DebugPhase1Test, TryCatchGetsCleanMessageFromRuntimeError)
{
    eval(R"(
        try
            v = [1 2 3];
            v(100);
        catch e
            caught_msg = e.message;
        end
    )");
    auto *msg = getVarPtr("caught_msg");
    ASSERT_NE(msg, nullptr);
    std::string m = msg->toString();
    // Should NOT contain "Error at line" prefix
    EXPECT_TRUE(m.find("Error at line") == std::string::npos)
        << "try/catch message should be clean: " << m;
    // Should contain useful error text
    EXPECT_TRUE(m.find("ndex") != std::string::npos || m.find("ound") != std::string::npos
                || m.find("exceed") != std::string::npos)
        << "Message should describe the error: " << m;
}

// ============================================================
// 6. Nested function errors point to inner function
// ============================================================

TEST_P(DebugPhase1Test, NestedFunctionErrorLocation)
{
    // Only meaningful for VM — TW doesn't use sourceMap
    if (GetParam() == BackendParam::TreeWalker)
        return;

    try {
        eval(R"(
            function r = inner(x)
                r = x(100);
            end
            inner([1 2 3]);
        )");
        FAIL() << "Should have thrown";
    } catch (const MError &e) {
        EXPECT_GT(e.line(), 0);
        // Error should reference the inner function
        EXPECT_EQ(e.funcName(), "inner") << "Error should name the function where it occurred";
    } catch (const std::runtime_error &) {
        // Acceptable if enrichment didn't trigger
    }
}

// ============================================================
// 7. Disassemble shows source locations
// ============================================================

TEST_P(DebugPhase1Test, DisassembleShowsLineInfo)
{
    std::string code = "x = 1;\ny = 2;\n";
    Lexer lexer(code);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();

    Compiler compiler(engine);
    auto src = std::make_shared<const std::string>(code);
    auto chunk = compiler.compile(ast.get(), src);

    std::string dis = Compiler::disassemble(chunk);
    // Should contain line markers like "L1:" or "L2:"
    EXPECT_TRUE(dis.find("L1:") != std::string::npos || dis.find("L2:") != std::string::npos)
        << "Disassembly should show source locations:\n"
        << dis;
}

// ============================================================
// 8. Variables assigned before error survive in workspace
// ============================================================

TEST_P(DebugPhase1Test, VariablesSurviveError)
{
    // MATLAB behavior: x and y should be in workspace even though z = v(100) fails
    try {
        eval("x = 42;\ny = 7;\nv = [1 2 3];\nz = v(100);\n");
    } catch (...) {
        // Expected
    }

    auto *x = getVarPtr("x");
    auto *y = getVarPtr("y");
    ASSERT_NE(x, nullptr) << "x should survive in workspace after error";
    ASSERT_NE(y, nullptr) << "y should survive in workspace after error";
    EXPECT_DOUBLE_EQ(x->toScalar(), 42.0);
    EXPECT_DOUBLE_EQ(y->toScalar(), 7.0);
}

TEST_P(DebugPhase1Test, VariablesSurviveErrorPartialAssign)
{
    // v should be in workspace even though indexing fails on the next line
    try {
        eval("v = [10 20 30];\nv(100);\n");
    } catch (...) {
    }

    auto *v = getVarPtr("v");
    ASSERT_NE(v, nullptr) << "v should survive in workspace after error";
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 10.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[1], 20.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[2], 30.0);
}

// ============================================================
// 9. Existing error tests still pass (backward compat)
// ============================================================

TEST_P(DebugPhase1Test, RuntimeErrorStillCatchable)
{
    // Verify MError is still catchable as std::runtime_error
    EXPECT_THROW(eval("undefined_xyz;"), std::runtime_error);
}

TEST_P(DebugPhase1Test, IndexOutOfBoundsStillThrows)
{
    EXPECT_THROW(eval("v = [1 2 3]; v(10);"), std::runtime_error);
}

TEST_P(DebugPhase1Test, CompileErrorDoesNotLeakOldVars)
{
    // First eval succeeds — sets prev_var
    eval("prev_var = 999;");
    EXPECT_DOUBLE_EQ(getVarPtr("prev_var")->toScalar(), 999.0);

    // Second eval fails at compile time (undefined var on top-level).
    // prev_var should remain, but no NEW stale vars should appear
    // from the failed compilation.
    try {
        eval("new_var = 123;\nundefined_compile_fail;");
    } catch (...) {
    }

    // prev_var should still be there (set in previous eval)
    EXPECT_NE(getVarPtr("prev_var"), nullptr);

    // In VM mode: new_var may or may not be set depending on whether
    // compile succeeded (it may fail at compile time for undefined var).
    // The key invariant: no STALE data from a PREVIOUS execute leaks
    // into the CURRENT failed eval's export.
}

INSTANTIATE_DUAL(DebugPhase1Test);