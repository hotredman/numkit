// tests/test_commands.cpp — Command-style calls: clear, who, whos, which, exist, disp
// Parameterized: runs on both TreeWalker and VM backends

#include "dual_engine_fixture.hpp"

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
