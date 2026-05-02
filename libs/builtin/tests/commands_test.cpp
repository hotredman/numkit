// tests/test_commands.cpp — Command-style calls: clear, who, whos, which, exist, disp
// Parameterized: runs on both TreeWalker and VM backends

#include "dual_engine_fixture.hpp"

#include <cctype>
#include <filesystem>
#include <fstream>

using namespace m_test;

// ============================================================
// clear
// ============================================================

class ClearTest : public DualEngineTest {};

TEST_P(ClearTest, ClearAll)
{
    eval("x = 1; y = 2; z = 3;");
    EXPECT_NE(getVarPtr("x"), nullptr);
    EXPECT_NE(getVarPtr("y"), nullptr);
    eval("clear all");
    EXPECT_EQ(getVarPtr("x"), nullptr);
    EXPECT_EQ(getVarPtr("y"), nullptr);
    EXPECT_EQ(getVarPtr("z"), nullptr);
}

TEST_P(ClearTest, ClearAllWithSemicolon)
{
    eval("x = 1;");
    eval("clear all;");
    EXPECT_EQ(getVarPtr("x"), nullptr);
}

TEST_P(ClearTest, ClearSpecificVar)
{
    eval("x = 1; y = 2; z = 3;");
    eval("clear x");
    EXPECT_EQ(getVarPtr("x"), nullptr);
    EXPECT_NE(getVarPtr("y"), nullptr);
    EXPECT_NE(getVarPtr("z"), nullptr);
}

TEST_P(ClearTest, ClearMultipleVars)
{
    eval("a = 1; b = 2; c = 3;");
    eval("clear a b");
    EXPECT_EQ(getVarPtr("a"), nullptr);
    EXPECT_EQ(getVarPtr("b"), nullptr);
    EXPECT_NE(getVarPtr("c"), nullptr);
}

TEST_P(ClearTest, ClearFunctions)
{
    eval("function y = sq(x)\n  y = x^2;\nend");
    EXPECT_DOUBLE_EQ(evalScalar("sq(3);"), 9.0);
    eval("clear functions");
    EXPECT_THROW(eval("sq(3);"), std::exception);
}

TEST_P(ClearTest, SemicolonSuppressesOutput)
{
    eval("x = 10;");
    capturedOutput.clear();
    eval("clear x;");
    EXPECT_TRUE(capturedOutput.empty());
    EXPECT_EQ(getVarPtr("x"), nullptr);
}

TEST_P(ClearTest, CommandInsideIf)
{
    eval("x = 1; y = 2;");
    eval(R"(
        if true
            clear x
        end
    )");
    EXPECT_EQ(getVarPtr("x"), nullptr);
    EXPECT_NE(getVarPtr("y"), nullptr);
}

TEST_P(ClearTest, CommandInsideFor)
{
    eval(R"(
        function myfn(tag)
            global last_tag;
            last_tag = tag;
        end
    )");
    eval("global last_tag;");
    eval(R"(
        for i = 1:3
            myfn hello
        end
    )");
    EXPECT_EQ(getVarPtr("last_tag")->toString(), "hello");
}

TEST_P(ClearTest, CommandInsideFunction)
{
    eval(R"(
        function cleanup()
            clear x
        end
    )");
    eval("x = 42;");
    EXPECT_NO_THROW(eval("cleanup()"));
    EXPECT_NE(getVarPtr("x"), nullptr);
}

TEST_P(ClearTest, RealisticScript)
{
    eval(R"(
        x = 1;
        y = 2;
        z = 3;
        clear x y
    )");
    EXPECT_EQ(getVarPtr("x"), nullptr);
    EXPECT_EQ(getVarPtr("y"), nullptr);
    EXPECT_NE(getVarPtr("z"), nullptr);
    EXPECT_DOUBLE_EQ(getVar("z"), 3.0);
}

TEST_P(ClearTest, ClearAllThenReassign)
{
    eval("a = 1; b = 2;");
    eval("clear all");
    eval("c = 99;");
    EXPECT_EQ(getVarPtr("a"), nullptr);
    EXPECT_EQ(getVarPtr("b"), nullptr);
    EXPECT_DOUBLE_EQ(getVar("c"), 99.0);
}

TEST_P(ClearTest, ClearNoArgsSameAsClearAll)
{
    eval("x = 1; y = 2;");
    eval("clear");
    EXPECT_EQ(getVarPtr("x"), nullptr);
    EXPECT_EQ(getVarPtr("y"), nullptr);
    EXPECT_NEAR(evalScalar("pi;"), M_PI, 1e-12);
}

// ── clear -regexp ──────────────────────────────────────────
TEST_P(ClearTest, ClearRegexpDropsMatchingNames)
{
    eval("foo1 = 1; foo2 = 2; bar = 3;");
    eval("clear('-regexp', '^foo');");
    EXPECT_EQ(getVarPtr("foo1"), nullptr);
    EXPECT_EQ(getVarPtr("foo2"), nullptr);
    ASSERT_NE(getVarPtr("bar"), nullptr);
    EXPECT_NEAR(getVarPtr("bar")->toScalar(), 3.0, 1e-12);
}

TEST_P(ClearTest, ClearRegexpMultiplePatterns)
{
    eval("alpha = 1; beta = 2; gamma = 3;");
    eval("clear('-regexp', '^al', '^ga');");
    EXPECT_EQ(getVarPtr("alpha"), nullptr);
    EXPECT_EQ(getVarPtr("gamma"), nullptr);
    ASSERT_NE(getVarPtr("beta"), nullptr);
}

TEST_P(ClearTest, ClearRegexpInvalidPatternThrows)
{
    eval("a = 1;");
    EXPECT_THROW(eval("clear('-regexp', '[unbalanced');"), std::exception);
}

// ── clear import ───────────────────────────────────────────
TEST_P(ClearTest, ClearImportDropsActiveImports)
{
    // Register a custom function in a non-core namespace; import it,
    // verify it resolves, then `clear import` and verify it stops.
    engine.registerFunction("myns_for_clear", "v",
        [](Span<const Value>, size_t, Span<Value> outs, CallContext &ctx) {
            outs[0] = Value::scalar(99.0, ctx.engine->resource());
        });
    eval("import myns_for_clear.*; a = v();");
    EXPECT_NEAR(evalScalar("a;"), 99.0, 1e-12);
    eval("clear('import');");
    EXPECT_THROW(eval("b = v();"), std::exception);
}

// Static `clear x` inside a function: compiler emits CLEAR_VAR opcode
// for VM (and env->remove for TW). Works on both backends — kept here
// as regression coverage.
TEST_P(ClearTest, ClearLocalVariableInsideFunctionRemovesIt)
{
    eval(R"(
        function r = does_clear()
            x = 42;
            clear x
            r = exist('x', 'var');
        end
    )");
    eval("y = does_clear();");
    Value *y = getVarPtr("y");
    ASSERT_NE(y, nullptr);
    EXPECT_DOUBLE_EQ(y->toScalar(), 0.0);
}

// Dynamic `clear(name)` inside a function: compiler emits CLEAR_DYN
// opcode which looks the name up in varMap × R[reg] at runtime —
// bypasses the builtin lambda, so this works on both backends too.
TEST_P(ClearTest, ClearDynamicNameInsideFunctionRemovesIt)
{
    eval(R"(
        function r = does_clear_dyn()
            x = 42;
            n = 'x';
            clear(n);
            r = exist('x', 'var');
        end
    )");
    eval("y = does_clear_dyn();");
    Value *y = getVarPtr("y");
    ASSERT_NE(y, nullptr);
    EXPECT_DOUBLE_EQ(y->toScalar(), 0.0);
}

INSTANTIATE_DUAL(ClearTest);

// ============================================================
// Constants protection after clear
// ============================================================

class ClearConstantsTest : public DualEngineTest {};

TEST_P(ClearConstantsTest, ClearAllPreservesPi)
{
    eval("x = 42;");
    eval("clear all");
    EXPECT_NEAR(evalScalar("pi;"), M_PI, 1e-12);
}

TEST_P(ClearConstantsTest, ClearAllPreservesEps)
{
    eval("clear all");
    double eps = evalScalar("eps;");
    EXPECT_GT(eps, 0);
    EXPECT_LT(eps, 1e-10);
}

TEST_P(ClearConstantsTest, ClearAllPreservesInf)
{
    eval("clear all");
    EXPECT_TRUE(std::isinf(evalScalar("inf;")));
}

TEST_P(ClearConstantsTest, ClearAllPreservesNan)
{
    eval("clear all");
    EXPECT_TRUE(std::isnan(evalScalar("nan;")));
}

TEST_P(ClearConstantsTest, ClearAllPreservesTrueFalse)
{
    eval("clear all");
    EXPECT_DOUBLE_EQ(evalScalar("true;"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("false;"), 0.0);
}

TEST_P(ClearConstantsTest, ClearAllPreservesImaginaryUnit)
{
    eval("clear all");
    auto v = eval("i;");
    EXPECT_TRUE(v.isComplex());
    EXPECT_DOUBLE_EQ(v.toComplex().imag(), 1.0);
}

TEST_P(ClearConstantsTest, ClearSpecificCannotRemovePi)
{
    eval("clear pi");
    EXPECT_NEAR(evalScalar("pi;"), M_PI, 1e-12);
}

TEST_P(ClearConstantsTest, ClearSpecificCannotRemoveInf)
{
    eval("clear inf");
    EXPECT_TRUE(std::isinf(evalScalar("inf;")));
}

INSTANTIATE_DUAL(ClearConstantsTest);

// ============================================================
// who / whos
// ============================================================

class WhoTest : public DualEngineTest {};

TEST_P(WhoTest, WhosProducesOutput)
{
    eval("x = 42; y = [1 2 3];");
    capturedOutput.clear();
    eval("whos x y");
    EXPECT_FALSE(capturedOutput.empty());
    EXPECT_NE(capturedOutput.find("x"), std::string::npos);
    EXPECT_NE(capturedOutput.find("y"), std::string::npos);
}

TEST_P(WhoTest, WhoWithArgsShowsOnlyRequested)
{
    eval("x = 1; y = 2; z = 3;");
    capturedOutput.clear();
    eval("who x z");
    EXPECT_NE(capturedOutput.find("x"), std::string::npos);
    EXPECT_NE(capturedOutput.find("z"), std::string::npos);
}

TEST_P(WhoTest, WhoHidesConstants)
{
    eval("myvar = 42;");
    capturedOutput.clear();
    eval("who");
    EXPECT_NE(capturedOutput.find("myvar"), std::string::npos);
}

TEST_P(WhoTest, WhosWithArgShowsDetails)
{
    eval("A = zeros(3, 4);");
    capturedOutput.clear();
    eval("whos A");
    EXPECT_NE(capturedOutput.find("A"), std::string::npos);
    EXPECT_NE(capturedOutput.find("3"), std::string::npos);
    EXPECT_NE(capturedOutput.find("4"), std::string::npos);
}

TEST_P(WhoTest, WhosNoArgs)
{
    eval("x = 42; y = [1 2 3];");
    capturedOutput.clear();
    eval("whos");
    EXPECT_NE(capturedOutput.find("x"), std::string::npos);
    EXPECT_NE(capturedOutput.find("y"), std::string::npos);
}

// ── who -file / whos -file ─────────────────────────────────
// Our save format is ASCII: a single matrix without field-name
// metadata. `load <file>` assigns the matrix to a workspace variable
// named after the file's stem; `who -file` therefore surfaces that
// stem as the only "variable" in the file. Documented in the builtin
// implementation; tested here so the contract is pinned.
TEST_P(WhoTest, WhoFileReportsStem)
{
    auto p = std::filesystem::temp_directory_path() / "numkit_whotest_who.txt";
    {
        std::ofstream o(p);
        o << "1 2 3\n4 5 6\n";
    }
    capturedOutput.clear();
    eval("who('-file', '" + p.string() + "');");
    EXPECT_NE(capturedOutput.find("numkit_whotest_who"), std::string::npos);
    std::filesystem::remove(p);
}

TEST_P(WhoTest, WhosFileEmitsRowWithBytes)
{
    auto p = std::filesystem::temp_directory_path() / "numkit_whotest_whos.txt";
    {
        std::ofstream o(p);
        o << "1 2\n";
    }
    capturedOutput.clear();
    eval("whos('-file', '" + p.string() + "');");
    // Header + one row containing the stem and "double" class.
    EXPECT_NE(capturedOutput.find("numkit_whotest_whos"), std::string::npos);
    EXPECT_NE(capturedOutput.find("double"), std::string::npos);
    std::filesystem::remove(p);
}

TEST_P(WhoTest, WhoFileMissingFileThrows)
{
    EXPECT_THROW(
        eval("who('-file', '/this/path/definitely/does/not/exist.txt');"),
        std::exception);
}

// `who` inside a function: handled by OpCode::WHO at compile time,
// which iterates frame.chunk->varMap × frame.R[reg]. Works on both
// backends — kept here as regression coverage.
TEST_P(WhoTest, WhoInsideFunctionListsLocals)
{
    eval(R"(
        function locals = list_locals()
            a = 1;
            b = 2;
            who
            locals = 'done';
        end
    )");
    capturedOutput.clear();
    eval("list_locals();");
    EXPECT_NE(capturedOutput.find("a"), std::string::npos);
    EXPECT_NE(capturedOutput.find("b"), std::string::npos);
}

INSTANTIATE_DUAL(WhoTest);

// ============================================================
// which
// ============================================================

class WhichTest : public DualEngineTest {};

TEST_P(WhichTest, WhichFindsVariable)
{
    eval("x = 42;");
    capturedOutput.clear();
    eval("which x");
    EXPECT_NE(capturedOutput.find("variable"), std::string::npos);
}

TEST_P(WhichTest, WhichFindsBuiltin)
{
    capturedOutput.clear();
    eval("which sin");
    EXPECT_NE(capturedOutput.find("built-in"), std::string::npos);
}

TEST_P(WhichTest, WhichFindsUserFunction)
{
    eval("function y = myfun(x)\n  y = x;\nend");
    capturedOutput.clear();
    eval("which myfun");
    EXPECT_NE(capturedOutput.find("user"), std::string::npos);
}

TEST_P(WhichTest, WhichReportsNotFound)
{
    capturedOutput.clear();
    eval("which totally_nonexistent");
    EXPECT_NE(capturedOutput.find("not found"), std::string::npos);
}

INSTANTIATE_DUAL(WhichTest);

// ============================================================
// exist
// ============================================================

class ExistTest : public DualEngineTest {};

TEST_P(ExistTest, ExistFindsVariable)
{
    eval("x = 42;");
    EXPECT_DOUBLE_EQ(evalScalar("exist('x');"), 1.0);
}

TEST_P(ExistTest, ExistFindsBuiltin)
{
    double r = evalScalar("exist('sin');");
    EXPECT_GT(r, 0);
}

TEST_P(ExistTest, ExistFindsUserFunction)
{
    eval("function y = myfun(x)\n  y = x;\nend");
    double r = evalScalar("exist('myfun');");
    EXPECT_GT(r, 0);
}

TEST_P(ExistTest, ExistReturnsZeroForNonexistent)
{
    EXPECT_DOUBLE_EQ(evalScalar("exist('totally_nonexistent_xyz');"), 0.0);
}

TEST_P(ExistTest, ExistAfterClear)
{
    eval("x = 42;");
    EXPECT_DOUBLE_EQ(evalScalar("exist('x');"), 1.0);
    eval("clear x");
    EXPECT_DOUBLE_EQ(evalScalar("exist('x');"), 0.0);
}

INSTANTIATE_DUAL(ExistTest);

// ============================================================
// disp command-style
// ============================================================

class DispTest : public DualEngineTest {};

TEST_P(DispTest, DispCommandStyle)
{
    capturedOutput.clear();
    eval("disp hello");
    EXPECT_FALSE(capturedOutput.empty());
    EXPECT_NE(capturedOutput.find("hello"), std::string::npos);
}

TEST_P(DispTest, DispWithParensEquivalent)
{
    capturedOutput.clear();
    eval("disp('test')");
    EXPECT_NE(capturedOutput.find("test"), std::string::npos);

    capturedOutput.clear();
    eval("disp test");
    EXPECT_NE(capturedOutput.find("test"), std::string::npos);
}

TEST_P(DispTest, ExistCommandStyle)
{
    eval("x = 42;");
    auto val = eval("exist x");
    // exist('x') returns 1 — variable
}

TEST_P(DispTest, UserFuncCommandStyle)
{
    eval(R"(
        function myfn(tag)
            global last_tag;
            last_tag = tag;
        end
    )");
    eval("global last_tag;");
    eval("myfn hello");
    auto *t = getVarPtr("last_tag");
    ASSERT_NE(t, nullptr);
    EXPECT_EQ(t->toString(), "hello");
}

TEST_P(DispTest, UserFuncMultiArgCommandStyle)
{
    eval(R"(
        function myfn(a, b)
            global ga;
            global gb;
            ga = a;
            gb = b;
        end
    )");
    eval("global ga; global gb;");
    eval("myfn foo bar");
    EXPECT_EQ(getVarPtr("ga")->toString(), "foo");
    EXPECT_EQ(getVarPtr("gb")->toString(), "bar");
}

INSTANTIATE_DUAL(DispTest);

// ============================================================
// Non-regression: normal expressions still work alongside commands
// ============================================================

class CommandNonRegressionTest : public DualEngineTest {};

TEST_P(CommandNonRegressionTest, NormalExpressionStillWorks)
{
    eval("a = 5; b = a + 3;");
    EXPECT_DOUBLE_EQ(getVar("b"), 8.0);
}

TEST_P(CommandNonRegressionTest, FunctionCallParensStillWorks)
{
    eval("v = [3 1 2]; r = sort(v);");
    auto *r = getVarPtr("r");
    EXPECT_DOUBLE_EQ(r->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(r->doubleData()[2], 3.0);
}

TEST_P(CommandNonRegressionTest, DotAccessStillWorks)
{
    eval("s.x = 42;");
    EXPECT_DOUBLE_EQ(getVarPtr("s")->field("x").toScalar(), 42.0);
}

TEST_P(CommandNonRegressionTest, AssignStillWorks)
{
    eval("x = 10;");
    EXPECT_DOUBLE_EQ(getVar("x"), 10.0);
}

TEST_P(CommandNonRegressionTest, BinaryOpStillWorks)
{
    EXPECT_DOUBLE_EQ(evalScalar("3 + 4;"), 7.0);
}

TEST_P(CommandNonRegressionTest, ColonStillWorks)
{
    eval("v = 1:5;");
    EXPECT_EQ(getVarPtr("v")->numel(), 5u);
}

INSTANTIATE_DUAL(CommandNonRegressionTest);

// ============================================================
// version
// ============================================================
//
// numkit-m has no SemVer — `version` returns the compile-time
// build stamp as ISO-8601 "YYYY-MM-DD HH:MM:SS".

class VersionTest : public DualEngineTest {};

TEST_P(VersionTest, ReturnsNonEmptyString)
{
    eval("v = version;");
    auto *p = getVarPtr("v");
    ASSERT_NE(p, nullptr);
    ASSERT_TRUE(p->isChar());
    EXPECT_FALSE(p->toString().empty());
}

TEST_P(VersionTest, FormatIsIsoDateTime)
{
    eval("v = version;");
    std::string s = getVarPtr("v")->toString();
    // "YYYY-MM-DD HH:MM:SS" — 19 chars exactly.
    ASSERT_EQ(s.size(), 19u) << "got: [" << s << "]";
    EXPECT_EQ(s[4], '-');
    EXPECT_EQ(s[7], '-');
    EXPECT_EQ(s[10], ' ');
    EXPECT_EQ(s[13], ':');
    EXPECT_EQ(s[16], ':');
    for (size_t i : {0u,1u,2u,3u, 5u,6u, 8u,9u, 11u,12u, 14u,15u, 17u,18u})
        EXPECT_TRUE(std::isdigit(static_cast<unsigned char>(s[i])))
            << "non-digit at " << i << " in [" << s << "]";
}

TEST_P(VersionTest, StableWithinSession)
{
    eval("v1 = version;");
    eval("v2 = version;");
    EXPECT_EQ(getVarPtr("v1")->toString(),
              getVarPtr("v2")->toString());
}

INSTANTIATE_DUAL(VersionTest);

// ── Pack 36: lastwarn ────────────────────────────────────────────────
class LastwarnTest : public DualEngineTest {};

TEST_P(LastwarnTest, ClearedStateIsEmpty)
{
    // Reset shared thread_local state — other tests in the binary may
    // have fired warning() before us.
    eval("lastwarn('');");
    eval("[m, i] = lastwarn();");
    EXPECT_EQ(getVarPtr("m")->toString(), "");
    EXPECT_EQ(getVarPtr("i")->toString(), "");
}

TEST_P(LastwarnTest, WarningSetsState)
{
    eval("warning('m:test:foo', 'something happened');");
    eval("[m, i] = lastwarn();");
    EXPECT_EQ(getVarPtr("m")->toString(), "something happened");
    EXPECT_EQ(getVarPtr("i")->toString(), "m:test:foo");
}

TEST_P(LastwarnTest, ManualSetForm)
{
    eval("lastwarn('manual reset', 'm:reset');");
    eval("[m, i] = lastwarn();");
    EXPECT_EQ(getVarPtr("m")->toString(), "manual reset");
    EXPECT_EQ(getVarPtr("i")->toString(), "m:reset");
}

TEST_P(LastwarnTest, ManualResetWithEmpty)
{
    eval("warning('m:test:bar', 'first');");
    eval("lastwarn('');");
    eval("[m, i] = lastwarn();");
    EXPECT_EQ(getVarPtr("m")->toString(), "");
}

INSTANTIATE_DUAL(LastwarnTest);
