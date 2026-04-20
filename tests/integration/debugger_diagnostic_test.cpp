// tests/test_debugger_diagnostic.cpp — Diagnostic: what actually works?
#include "dual_engine_fixture.hpp"
#include <numkit/m/core/MCompiler.hpp>
#include <numkit/m/core/MLexer.hpp>
#include <numkit/m/core/MParser.hpp>

using namespace m_test;
using namespace numkit::m;

class DebugDiagnostic : public DualEngineTest
{};

// ============================================================
// A. Does sourceMap exist and get populated?
// ============================================================

TEST_P(DebugDiagnostic, A1_SourceMapExists)
{
    std::string code = "x = 1 + 2;";
    Lexer lexer(code);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();

    Compiler compiler(engine);
    auto src = std::make_shared<const std::string>(code);
    auto chunk = compiler.compile(ast.get(), src);

    // Does sourceMap exist and match code size?
    EXPECT_EQ(chunk.sourceMap.size(), chunk.code.size())
        << "DIAGNOSTIC: sourceMap size = " << chunk.sourceMap.size()
        << ", code size = " << chunk.code.size();
}

TEST_P(DebugDiagnostic, A2_SourceMapHasLines)
{
    std::string code = "x = 1;\ny = 2;\n";
    Lexer lexer(code);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();

    Compiler compiler(engine);
    auto src = std::make_shared<const std::string>(code);
    auto chunk = compiler.compile(ast.get(), src);

    int nonZero = 0;
    for (auto &loc : chunk.sourceMap)
        if (loc.line > 0) nonZero++;

    std::cerr << "DIAGNOSTIC A2: " << nonZero << " of " << chunk.sourceMap.size()
              << " instructions have non-zero line numbers\n";
    EXPECT_GT(nonZero, 0);
}

// ============================================================
// B. Does sourceCode get stored in chunk?
// ============================================================

TEST_P(DebugDiagnostic, B1_SourceCodeInChunk)
{
    std::string code = "x = 42;";
    Lexer lexer(code);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();

    Compiler compiler(engine);
    auto src = std::make_shared<const std::string>(code);
    auto chunk = compiler.compile(ast.get(), src);

    bool hasSourceCode = (chunk.sourceCode != nullptr);
    std::cerr << "DIAGNOSTIC B1: chunk.sourceCode is "
              << (hasSourceCode ? "SET" : "NULL") << "\n";
    EXPECT_TRUE(hasSourceCode);
}

// ============================================================
// C. Does VM throw MError with location?
// ============================================================

TEST_P(DebugDiagnostic, C1_VMThrowsMError)
{
    if (GetParam() == BackendParam::TreeWalker) {
        std::cerr << "DIAGNOSTIC C1: skipping for TW\n";
        return;
    }

    try {
        eval("x = 1;\nv = [1 2 3];\nv(100);\n");
        FAIL() << "Should have thrown";
    } catch (const MError &e) {
        std::cerr << "DIAGNOSTIC C1: caught MError, line=" << e.line()
                  << " col=" << e.col()
                  << " func='" << e.funcName() << "'"
                  << " what='" << e.what() << "'\n";
        EXPECT_GT(e.line(), 0);
    } catch (const std::runtime_error &e) {
        std::cerr << "DIAGNOSTIC C1: caught std::runtime_error (NOT MError): '"
                  << e.what() << "'\n";
        FAIL() << "VM should throw MError, not plain runtime_error";
    }
}

TEST_P(DebugDiagnostic, C2_TWThrowsMError)
{
    if (GetParam() == BackendParam::VM) {
        std::cerr << "DIAGNOSTIC C2: skipping for VM\n";
        return;
    }

    try {
        eval("x = 1;\nv = [1 2 3];\nv(100);\n");
        FAIL() << "Should have thrown";
    } catch (const MError &e) {
        std::cerr << "DIAGNOSTIC C2: caught MError, line=" << e.line()
                  << " col=" << e.col()
                  << " what='" << e.what() << "'\n";
        EXPECT_GT(e.line(), 0);
    } catch (const std::runtime_error &e) {
        std::cerr << "DIAGNOSTIC C2: caught std::runtime_error (NOT MError): '"
                  << e.what() << "'\n";
        FAIL() << "TW should throw MError, not plain runtime_error";
    }
}

// ============================================================
// D. Does formattedWhat() work?
// ============================================================

TEST_P(DebugDiagnostic, D1_FormattedWhat)
{
    MError err("bad stuff", 15, 3, "my_func");
    std::cerr << "DIAGNOSTIC D1: what()='" << err.what() << "'\n";
    std::cerr << "DIAGNOSTIC D1: formattedWhat()='" << err.formattedWhat() << "'\n";

    EXPECT_STREQ(err.what(), "bad stuff");
    EXPECT_TRUE(err.formattedWhat().find("line 15") != std::string::npos);
}

// ============================================================
// E. Do variables survive errors?
// ============================================================

TEST_P(DebugDiagnostic, E1_VarsSurviveError_VM)
{
    if (GetParam() == BackendParam::TreeWalker) {
        std::cerr << "DIAGNOSTIC E1: skipping for TW (TW always works)\n";
        return;
    }

    try {
        eval("surv_x = 42;\nsurv_y = 7;\nv = [1 2 3];\nz = v(100);\n");
    } catch (...) {}

    auto *x = getVarPtr("surv_x");
    auto *y = getVarPtr("surv_y");
    std::cerr << "DIAGNOSTIC E1: surv_x is " << (x ? "SET" : "NULL") << "\n";
    std::cerr << "DIAGNOSTIC E1: surv_y is " << (y ? "SET" : "NULL") << "\n";

    EXPECT_NE(x, nullptr) << "surv_x should survive error";
    EXPECT_NE(y, nullptr) << "surv_y should survive error";
    if (x) EXPECT_DOUBLE_EQ(x->toScalar(), 42.0);
    if (y) EXPECT_DOUBLE_EQ(y->toScalar(), 7.0);
}

TEST_P(DebugDiagnostic, E2_VarsSurviveError_TW)
{
    if (GetParam() == BackendParam::VM) {
        std::cerr << "DIAGNOSTIC E2: skipping for VM\n";
        return;
    }

    try {
        eval("surv_a = 99;\nv = [1 2 3];\nz = v(100);\n");
    } catch (...) {}

    auto *a = getVarPtr("surv_a");
    std::cerr << "DIAGNOSTIC E2: surv_a is " << (a ? "SET" : "NULL") << "\n";
    EXPECT_NE(a, nullptr) << "surv_a should survive error in TW";
    if (a) EXPECT_DOUBLE_EQ(a->toScalar(), 99.0);
}

// ============================================================
// F. Does try/catch get clean message?
// ============================================================

TEST_P(DebugDiagnostic, F1_TryCatchMessage)
{
    eval(R"(
        try
            error('boom');
        catch e
            diag_msg = e.message;
        end
    )");
    auto *msg = getVarPtr("diag_msg");
    ASSERT_NE(msg, nullptr);
    std::cerr << "DIAGNOSTIC F1: caught message = '" << msg->toString() << "'\n";
    EXPECT_EQ(msg->toString(), "boom");
}

// ============================================================
// G. Does disassemble show source locations?
// ============================================================

TEST_P(DebugDiagnostic, G1_Disassemble)
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
    std::cerr << "DIAGNOSTIC G1: disassembly:\n" << dis << "\n";

    bool hasL = (dis.find("L1:") != std::string::npos || dis.find("L2:") != std::string::npos);
    EXPECT_TRUE(hasL) << "disassembly should show Lx: markers";
}

INSTANTIATE_DUAL(DebugDiagnostic);