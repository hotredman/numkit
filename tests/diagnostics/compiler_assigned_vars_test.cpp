// tests/diagnostics/compiler_assigned_vars_test.cpp
//
// Locks down the compiler's BytecodeChunk::assignedVars contract — every
// source form that writes to a user variable must populate assignedVars,
// every pure-read form must NOT. The debug workspace uses this set to
// honour MATLAB's whos-parity rule for shadowed built-ins, so regressions
// here silently change the debug UI.

#include "MCompiler.hpp"
#include "MEngine.hpp"
#include "MLexer.hpp"
#include "MParser.hpp"
#include "MTypes.hpp"

#include <gtest/gtest.h>

using namespace numkit;

namespace {

BytecodeChunk compileSnippet(Engine &engine, const std::string &code)
{
    Lexer lexer(code);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();
    auto src = std::make_shared<const std::string>(code);
    return engine.compilerPtr()->compile(ast.get(), src);
}

} // namespace

// ============================================================
// Writes: every form must land in assignedVars.
// ============================================================

TEST(CompilerAssignedVars, SimpleAssignment)
{
    Engine engine;
    auto chunk = compileSnippet(engine, "x = 5;");
    EXPECT_TRUE(chunk.assignedVars.count("x") > 0);
}

TEST(CompilerAssignedVars, MultiAssignment)
{
    Engine engine;
    auto chunk = compileSnippet(engine, "[a, b] = size([1 2 3]);");
    EXPECT_TRUE(chunk.assignedVars.count("a") > 0);
    EXPECT_TRUE(chunk.assignedVars.count("b") > 0);
}

TEST(CompilerAssignedVars, MultiAssignmentIgnoresTilde)
{
    Engine engine;
    auto chunk = compileSnippet(engine, "[~, b] = size([1 2 3]);");
    // Only real names end up in varMap / assignedVars.
    EXPECT_TRUE(chunk.assignedVars.count("b") > 0);
}

TEST(CompilerAssignedVars, IndexedAssignment)
{
    Engine engine;
    // v must be pre-loaded from workspace so the compiler accepts reading it.
    engine.eval("v = [1 2 3 4];");
    auto chunk = compileSnippet(engine, "v(2) = 99;");
    EXPECT_TRUE(chunk.assignedVars.count("v") > 0);
}

TEST(CompilerAssignedVars, IndexedDeleteAssignment)
{
    // This was the write-site the original compile pass missed — `v(idx) = []`
    // mutates v but went through varReg() (now varRegLookup) without being
    // flagged, so the debug workspace lost track of shadowed built-ins that
    // were only touched via element deletion.
    Engine engine;
    engine.eval("v = [1 2 3 4];");
    auto chunk = compileSnippet(engine, "v(2) = [];");
    EXPECT_TRUE(chunk.assignedVars.count("v") > 0)
        << "v(idx) = [] must mark v as assigned";
}

TEST(CompilerAssignedVars, FieldAssignment)
{
    Engine engine;
    auto chunk = compileSnippet(engine, "s.a = 10;");
    EXPECT_TRUE(chunk.assignedVars.count("s") > 0);
}

TEST(CompilerAssignedVars, NestedFieldAssignment)
{
    Engine engine;
    engine.eval("s.a.b = 1;");
    auto chunk = compileSnippet(engine, "s.a.b = 2;");
    EXPECT_TRUE(chunk.assignedVars.count("s") > 0);
}

TEST(CompilerAssignedVars, DynamicFieldAssignment)
{
    Engine engine;
    engine.eval("s.x = 1; f = 'x';");
    auto chunk = compileSnippet(engine, "s.(f) = 7;");
    EXPECT_TRUE(chunk.assignedVars.count("s") > 0);
}

TEST(CompilerAssignedVars, CellAssignment)
{
    Engine engine;
    engine.eval("c = {1, 2, 3};");
    auto chunk = compileSnippet(engine, "c{2} = 99;");
    EXPECT_TRUE(chunk.assignedVars.count("c") > 0);
}

TEST(CompilerAssignedVars, ForLoopVariable)
{
    Engine engine;
    auto chunk = compileSnippet(engine, "for i = 1:3\n  x = i;\nend\n");
    EXPECT_TRUE(chunk.assignedVars.count("i") > 0);
    EXPECT_TRUE(chunk.assignedVars.count("x") > 0);
}

TEST(CompilerAssignedVars, TryCatchVariable)
{
    Engine engine;
    auto chunk = compileSnippet(engine, "try\n  x = 1;\ncatch err\n  y = 2;\nend\n");
    EXPECT_TRUE(chunk.assignedVars.count("err") > 0);
}

TEST(CompilerAssignedVars, GlobalDeclaration)
{
    Engine engine;
    auto chunk = compileSnippet(engine, "global g;");
    EXPECT_TRUE(chunk.assignedVars.count("g") > 0);
}

TEST(CompilerAssignedVars, FunctionParamsAndReturns)
{
    Engine engine;
    // Compile the function definition directly so we inspect its chunk.
    Lexer lexer("function r = foo(a, b)\n    r = a + b;\nend\n");
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();

    // Top-level AST: a BLOCK containing the FUNCTION_DEF.
    const ASTNode *funcDef = nullptr;
    for (auto &child : ast->children)
        if (child && child->type == NodeType::FUNCTION_DEF)
            funcDef = child.get();
    ASSERT_NE(funcDef, nullptr);

    auto src = std::make_shared<const std::string>("<test>");
    auto chunk = engine.compilerPtr()->compileFunction(funcDef, src);
    EXPECT_TRUE(chunk.assignedVars.count("a") > 0) << "param a must be assigned";
    EXPECT_TRUE(chunk.assignedVars.count("b") > 0) << "param b must be assigned";
    EXPECT_TRUE(chunk.assignedVars.count("r") > 0) << "return r must be assigned";
    // Pseudo-vars are intentionally NOT marked.
    EXPECT_EQ(chunk.assignedVars.count("nargin"), 0u) << "nargin is pseudo — not a user assignment";
    EXPECT_EQ(chunk.assignedVars.count("nargout"), 0u) << "nargout is pseudo — not a user assignment";
}

// ============================================================
// Reads: must NOT end up in assignedVars.
// ============================================================

TEST(CompilerAssignedVars, PlainRead)
{
    Engine engine;
    engine.eval("x = 5;");
    auto chunk = compileSnippet(engine, "y = x;");
    EXPECT_TRUE(chunk.assignedVars.count("y") > 0) << "y is written";
    EXPECT_EQ(chunk.assignedVars.count("x"), 0u) << "x is only read, must not be marked";
}

TEST(CompilerAssignedVars, BuiltinReadOnly)
{
    Engine engine;
    auto chunk = compileSnippet(engine, "x = pi + eps;");
    EXPECT_TRUE(chunk.assignedVars.count("x") > 0);
    EXPECT_EQ(chunk.assignedVars.count("pi"), 0u)
        << "reading pi must not mark it as assigned";
    EXPECT_EQ(chunk.assignedVars.count("eps"), 0u);
}

TEST(CompilerAssignedVars, IndexReadOnly)
{
    Engine engine;
    engine.eval("v = [1 2 3];");
    auto chunk = compileSnippet(engine, "y = v(2);");
    EXPECT_TRUE(chunk.assignedVars.count("y") > 0);
    EXPECT_EQ(chunk.assignedVars.count("v"), 0u)
        << "v is only read via v(2), must not be marked";
}

// ============================================================
// Reserved-name classification invariants.
// ============================================================

TEST(ReservedNames, SetsAreDisjoint)
{
    for (auto &n : kBuiltinConstants) {
        EXPECT_EQ(kPseudoVars.count(n), 0u)
            << "'" << n << "' is in both kBuiltinConstants and kPseudoVars";
    }
}

TEST(ReservedNames, UnionMatchesKBuiltinNames)
{
    std::unordered_set<std::string> u = kBuiltinConstants;
    u.insert(kPseudoVars.begin(), kPseudoVars.end());
    EXPECT_EQ(u, kBuiltinNames)
        << "kBuiltinNames must be exactly kBuiltinConstants ∪ kPseudoVars";
}

TEST(ReservedNames, ConstantsContainExpectedNames)
{
    for (auto *n : {"pi", "eps", "inf", "nan", "i", "j", "true", "false"})
        EXPECT_TRUE(kBuiltinConstants.count(n) > 0)
            << n << " must be in kBuiltinConstants";
}

TEST(ReservedNames, PseudoVarsContainExpectedNames)
{
    for (auto *n : {"ans", "nargin", "nargout", "end"})
        EXPECT_TRUE(kPseudoVars.count(n) > 0)
            << n << " must be in kPseudoVars";
}

TEST(CompilerAssignedVars, BuiltinShadowInScript)
{
    // The whole shadowing feature rests on this: a script that assigns pi
    // marks pi in assignedVars, whereas one that only reads it does not.
    {
        Engine engine;
        auto chunk = compileSnippet(engine, "pi = 5;");
        EXPECT_TRUE(chunk.assignedVars.count("pi") > 0)
            << "pi = 5 must mark pi as assigned (shadowing)";
    }
    {
        Engine engine;
        auto chunk = compileSnippet(engine, "x = pi;");
        EXPECT_EQ(chunk.assignedVars.count("pi"), 0u)
            << "x = pi must NOT mark pi as assigned";
    }
}
