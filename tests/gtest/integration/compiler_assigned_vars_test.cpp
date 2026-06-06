// tests/diagnostics/compiler_assigned_vars_test.cpp
//
// Locks down the compiler's BytecodeChunk::assignedVars contract — every
// source form that writes to a user variable must populate assignedVars,
// every pure-read form must NOT. The debug workspace uses this set to
// honour MATLAB's whos-parity rule for shadowed built-ins, so regressions
// here silently change the debug UI.

#include <numkit/core/compiler.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/core/lexer.hpp>
#include <numkit/core/parser.hpp>
#include <numkit/core/types.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

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
    StdEngine engine;
    auto chunk = compileSnippet(engine, "x = 5;");
    EXPECT_TRUE(chunk.assignedVars.count("x") > 0);
}

TEST(CompilerAssignedVars, MultiAssignment)
{
    StdEngine engine;
    auto chunk = compileSnippet(engine, "[a, b] = size([1 2 3]);");
    EXPECT_TRUE(chunk.assignedVars.count("a") > 0);
    EXPECT_TRUE(chunk.assignedVars.count("b") > 0);
}

TEST(CompilerAssignedVars, MultiAssignmentIgnoresTilde)
{
    StdEngine engine;
    auto chunk = compileSnippet(engine, "[~, b] = size([1 2 3]);");
    // Only real names end up in varMap / assignedVars.
    EXPECT_TRUE(chunk.assignedVars.count("b") > 0);
}

TEST(CompilerAssignedVars, IndexedAssignment)
{
    StdEngine engine;
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
    StdEngine engine;
    engine.eval("v = [1 2 3 4];");
    auto chunk = compileSnippet(engine, "v(2) = [];");
    EXPECT_TRUE(chunk.assignedVars.count("v") > 0)
        << "v(idx) = [] must mark v as assigned";
}

TEST(CompilerAssignedVars, FieldAssignment)
{
    StdEngine engine;
    auto chunk = compileSnippet(engine, "s.a = 10;");
    EXPECT_TRUE(chunk.assignedVars.count("s") > 0);
}

TEST(CompilerAssignedVars, NestedFieldAssignment)
{
    StdEngine engine;
    engine.eval("s.a.b = 1;");
    auto chunk = compileSnippet(engine, "s.a.b = 2;");
    EXPECT_TRUE(chunk.assignedVars.count("s") > 0);
}

TEST(CompilerAssignedVars, DynamicFieldAssignment)
{
    StdEngine engine;
    engine.eval("s.x = 1; f = 'x';");
    auto chunk = compileSnippet(engine, "s.(f) = 7;");
    EXPECT_TRUE(chunk.assignedVars.count("s") > 0);
}

TEST(CompilerAssignedVars, CellAssignment)
{
    StdEngine engine;
    engine.eval("c = {1, 2, 3};");
    auto chunk = compileSnippet(engine, "c{2} = 99;");
    EXPECT_TRUE(chunk.assignedVars.count("c") > 0);
}

TEST(CompilerAssignedVars, ForLoopVariable)
{
    StdEngine engine;
    auto chunk = compileSnippet(engine, "for i = 1:3\n  x = i;\nend\n");
    EXPECT_TRUE(chunk.assignedVars.count("i") > 0);
    EXPECT_TRUE(chunk.assignedVars.count("x") > 0);
}

TEST(CompilerAssignedVars, TryCatchVariable)
{
    StdEngine engine;
    auto chunk = compileSnippet(engine, "try\n  x = 1;\ncatch err\n  y = 2;\nend\n");
    EXPECT_TRUE(chunk.assignedVars.count("err") > 0);
}

TEST(CompilerAssignedVars, GlobalDeclaration)
{
    StdEngine engine;
    auto chunk = compileSnippet(engine, "global g;");
    EXPECT_TRUE(chunk.assignedVars.count("g") > 0);
}

TEST(CompilerAssignedVars, FunctionParamsAndReturns)
{
    StdEngine engine;
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
    StdEngine engine;
    engine.eval("x = 5;");
    auto chunk = compileSnippet(engine, "y = x;");
    EXPECT_TRUE(chunk.assignedVars.count("y") > 0) << "y is written";
    EXPECT_EQ(chunk.assignedVars.count("x"), 0u) << "x is only read, must not be marked";
}

TEST(CompilerAssignedVars, BuiltinReadOnly)
{
    StdEngine engine;
    auto chunk = compileSnippet(engine, "x = pi + eps;");
    EXPECT_TRUE(chunk.assignedVars.count("x") > 0);
    EXPECT_EQ(chunk.assignedVars.count("pi"), 0u)
        << "reading pi must not mark it as assigned";
    EXPECT_EQ(chunk.assignedVars.count("eps"), 0u);
}

TEST(CompilerAssignedVars, IndexReadOnly)
{
    StdEngine engine;
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
    // `true`/`false`/`nan`/`NaN`/`inf`/`Inf` are MATLAB built-in
    // functions, not constants — they support shape forms
    // `nan(M, N, 'single')`, `Inf(N)`, `true(M, N)` etc. and must
    // NOT be in kBuiltinConstants. See BUGS.md #30 + matrix.cpp:nan_reg.
    for (auto *n : {"pi", "eps", "i", "j"})
        EXPECT_TRUE(kBuiltinConstants.count(n) > 0)
            << n << " must be in kBuiltinConstants";
    for (auto *n : {"true", "false", "nan", "NaN", "inf", "Inf"})
        EXPECT_FALSE(kBuiltinConstants.count(n) > 0)
            << n << " must NOT be in kBuiltinConstants (it's a function)";
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
        StdEngine engine;
        auto chunk = compileSnippet(engine, "pi = 5;");
        EXPECT_TRUE(chunk.assignedVars.count("pi") > 0)
            << "pi = 5 must mark pi as assigned (shadowing)";
    }
    {
        StdEngine engine;
        auto chunk = compileSnippet(engine, "x = pi;");
        EXPECT_EQ(chunk.assignedVars.count("pi"), 0u)
            << "x = pi must NOT mark pi as assigned";
    }
}

// ============================================================
// Register pre-allocation for FUNCTIONS (not only the top-level script).
//
// Bug: compileFunction skipped the assigned-var clustering pass that the
// top-level script gets via preImportGlobals. So inside a user function each
// local adopted whatever (high) temp slot its first assignment landed in;
// maxVarReg_ (the statement-boundary temp-release floor) crept upward and a
// function with only ~80 real locals false-positived on the 255-register limit
// (e.g. an inlined DOPRI5 ode45 — 73 named vars but maxVarReg crept to 253).
// preallocateAssignedVars now runs for functions too. See compiler.cpp.
// ============================================================

static BytecodeChunk compileFnSource(Engine &engine, const std::string &src)
{
    Lexer lexer(src);
    Parser parser(lexer.tokenize());
    auto ast = parser.parse();
    const ASTNode *fd = nullptr;
    for (auto &c : ast->children)
        if (c && c->type == NodeType::FUNCTION_DEF) fd = c.get();
    EXPECT_NE(fd, nullptr);
    auto s = std::make_shared<const std::string>(src);
    return engine.compilerPtr()->compileFunction(fd, s);
}

TEST(CompilerRegisterAlloc, FunctionLocalsClusterLikeScript)
{
    // 120 distinct locals, each from a multi-term expression so its first
    // assignment lands in a high temp slot. Without per-function
    // pre-allocation the slots fragment, maxVarReg_ creeps past 255, and this
    // throws a (false) register exhaustion. With it, locals cluster low and the
    // chunk needs only ~(#locals + a little temp headroom).
    std::string src = "function out = deepfn(a)\n";
    for (int i = 1; i <= 120; ++i)
        src += "  v" + std::to_string(i) + " = a + a*2 + a*3 + a*4 + a*5 + a*6;\n";
    src += "  out = v1 + v120;\nend\n";
    StdEngine engine;
    BytecodeChunk chunk;
    ASSERT_NO_THROW({ chunk = compileFnSource(engine, src); })
        << "120-local function must compile (pre-fix this threw register exhaustion)";
    EXPECT_LT(static_cast<int>(chunk.numRegisters), 170)
        << "locals must cluster low — numRegisters ~#locals, not a creeping floor";
}

TEST(CompilerRegisterAlloc, FunctionLocalsRunCorrectly)
{
    // End-to-end: the clustered chunk executes on the VM with correct values.
    std::string src = "function out = deepfn2(a)\n";
    for (int i = 1; i <= 90; ++i)
        src += "  v" + std::to_string(i) + " = a + " + std::to_string(i) + ";\n";
    src += "  out = v90;\nend\n";
    StdEngine engine;
    engine.eval(src);
    EXPECT_DOUBLE_EQ(engine.eval("deepfn2(10)").toScalar(), 100.0); // 10 + 90
}

TEST(CompilerRegisterAlloc, TooManyLocalsThrowsLoudly)
{
    // P0: a function that genuinely needs >255 register slots must throw a
    // typed RegisterExhaustionError — not silently produce a degenerate chunk.
    std::string src = "function out = hugefn()\n";
    for (int i = 1; i <= 300; ++i)
        src += "  a" + std::to_string(i) + " = " + std::to_string(i) + ";\n";
    src += "  out = a1;\nend\n";
    StdEngine engine;
    EXPECT_THROW(compileFnSource(engine, src), RegisterExhaustionError);
}

TEST(CompilerRegisterAlloc, RegisterBuiltinMSourceTooLargeThrowsClearError)
{
    // P0 at the embedded-wrapper boundary: registerBuiltinMSource must fail
    // loudly (not silently TW-only — which would later surface as a misleading
    // "undefined function" on the VM backend) when a wrapper exceeds the limit.
    std::string src = "function out = hugewrap()\n";
    for (int i = 1; i <= 300; ++i)
        src += "  a" + std::to_string(i) + " = " + std::to_string(i) + ";\n";
    src += "  out = a1;\nend\n";
    StdEngine engine;
    bool threw = false;
    try {
        engine.registerBuiltinMSource(src);
    } catch (const std::exception &e) {
        threw = true;
        EXPECT_NE(std::string(e.what()).find("255"), std::string::npos)
            << "error should mention the 255-register limit; got: " << e.what();
    }
    EXPECT_TRUE(threw) << "registerBuiltinMSource must throw on a too-large wrapper";

    // A normal small wrapper still registers and runs on the VM.
    engine.registerBuiltinMSource("function y = tinywrap(x)\n  y = x * 3;\nend\n");
    EXPECT_DOUBLE_EQ(engine.eval("tinywrap(4)").toScalar(), 12.0);
}

TEST(CompilerRegisterAlloc, ClassdefMethodTooLargeThrowsLoudly)
{
    // P0 consistency: a classdef METHOD that genuinely needs >255 register
    // slots must surface loudly when the VM lazily compiles it (the default
    // backend is the VM), not silently fall back to a TW-only, non-debuggable
    // method body. Pre-fix this returned a1=1 (silent TW-fallback).
    std::string src = "classdef BigC\n  methods\n    function out = big(obj)\n";
    for (int i = 1; i <= 300; ++i)
        src += "      a" + std::to_string(i) + " = " + std::to_string(i) + ";\n";
    src += "      out = a1;\n    end\n  end\nend\n";
    StdEngine engine;
    engine.eval(src);
    engine.eval("bc = BigC;");
    bool threw = false;
    try {
        engine.eval("rr = bc.big();");
    } catch (const std::exception &e) {
        threw = true;
        EXPECT_NE(std::string(e.what()).find("255"), std::string::npos)
            << "classdef method exhaustion should mention the 255-register limit; got: "
            << e.what();
    }
    EXPECT_TRUE(threw)
        << "a >255-register classdef method must throw, not silently TW-fallback";

    // A normal small method still compiles + runs on the VM.
    engine.eval("classdef SmallC\n  methods\n    function y = inc(obj, x)\n"
                "      y = x + 1;\n    end\n  end\nend\n");
    engine.eval("sc = SmallC;");
    EXPECT_DOUBLE_EQ(engine.eval("sc.inc(41)").toScalar(), 42.0);
}

TEST(CompilerRegisterAlloc, MFileLoaderTooLargeThrowsLoudly)
{
    // P0 path #2: a too-large function loaded from an .m file on disk
    // (resolveMFile_) must surface register exhaustion, not silently register
    // TW-only and then look like an "undefined function" on the VM backend.
    namespace fs = std::filesystem;
    fs::path dir = fs::temp_directory_path() / "numkit_regtest_mfile";
    fs::create_directories(dir);
    fs::path mf = dir / "hugemfile.m"; // file name == function name (MATLAB rule)
    {
        std::ofstream os(mf);
        os << "function out = hugemfile()\n";
        for (int i = 1; i <= 300; ++i)
            os << "  a" << i << " = " << i << ";\n";
        os << "  out = a1;\nend\n";
    }
    StdEngine engine;
    engine.addPath(dir.string());
    bool threw = false;
    try {
        engine.eval("rr = hugemfile();"); // first reference → resolveMFile_ loads + VM-compiles
    } catch (const std::exception &e) {
        threw = true;
        EXPECT_NE(std::string(e.what()).find("255"), std::string::npos)
            << "m-file exhaustion should mention the 255-register limit; got: " << e.what();
    }
    EXPECT_TRUE(threw)
        << "a >255-register .m-file function must throw, not silently TW-fallback";
    std::error_code ec;
    fs::remove_all(dir, ec); // best-effort cleanup
}

TEST(CompilerRegisterAlloc, MFileWithSyntaxErrorReportsLoudly)
{
    // resolveMFile_ de-crutch: a path-matched .m file that fails to lex/parse
    // must surface the error (naming the file), not be silently skipped → a
    // misleading "undefined function". Matches MATLAB (first path match's error
    // is reported).
    namespace fs = std::filesystem;
    fs::path dir = fs::temp_directory_path() / "numkit_regtest_badmfile";
    fs::create_directories(dir);
    fs::path mf = dir / "brokenfn.m";
    {
        std::ofstream os(mf);
        os << "function y = brokenfn(x)\n  y = ?x;\nend\n"; // '?' → lex error
    }
    StdEngine engine;
    engine.addPath(dir.string());
    bool threw = false;
    std::string msg;
    try {
        engine.eval("z = brokenfn(3);"); // first reference → resolveMFile_ loads it
    } catch (const std::exception &e) {
        threw = true;
        msg = e.what();
    }
    EXPECT_TRUE(threw) << "a broken .m on the path must throw, not be silently skipped";
    EXPECT_NE(msg.find("brokenfn"), std::string::npos)
        << "error must reference the matched file; got: " << msg;
    EXPECT_EQ(msg.find("undefined function"), std::string::npos)
        << "must not masquerade as 'undefined function'; got: " << msg;
    std::error_code ec;
    fs::remove_all(dir, ec);
}
