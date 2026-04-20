// tests/test_controlflow.cpp — Control flow: if, for, while, switch, try/catch
// Parameterized: runs on both TreeWalker and VM backends

#include "dual_engine_fixture.hpp"

using namespace m_test;

class ControlFlowTest : public DualEngineTest {};

TEST_P(ControlFlowTest, IfTrue)
{
    eval("x = 0; if true, x = 1; end");
    EXPECT_DOUBLE_EQ(getVar("x"), 1.0);
}

TEST_P(ControlFlowTest, IfFalse)
{
    eval("x = 0; if false, x = 1; end");
    EXPECT_DOUBLE_EQ(getVar("x"), 0.0);
}

TEST_P(ControlFlowTest, IfElse)
{
    eval("x = 5; if x > 10, y = 1; else, y = 2; end");
    EXPECT_DOUBLE_EQ(getVar("y"), 2.0);
}

TEST_P(ControlFlowTest, IfElseif)
{
    eval("x = 5; if x > 10, y = 1; elseif x > 3, y = 2; else, y = 3; end");
    EXPECT_DOUBLE_EQ(getVar("y"), 2.0);
}

TEST_P(ControlFlowTest, ForLoop)
{
    eval("s = 0; for i = 1:5, s = s + i; end");
    EXPECT_DOUBLE_EQ(getVar("s"), 15.0);
}

TEST_P(ControlFlowTest, ForBreak)
{
    eval("s = 0; for i = 1:10\n  if i > 3, break; end\n  s = s + i;\nend");
    EXPECT_DOUBLE_EQ(getVar("s"), 6.0); // 1+2+3
}

TEST_P(ControlFlowTest, ForContinue)
{
    eval("s = 0; for i = 1:5\n  if mod(i, 2) == 0, continue; end\n  s = s + i;\nend");
    EXPECT_DOUBLE_EQ(getVar("s"), 9.0); // 1+3+5
}

TEST_P(ControlFlowTest, WhileLoop)
{
    eval("x = 1; while x < 100, x = x * 2; end");
    EXPECT_DOUBLE_EQ(getVar("x"), 128.0);
}

TEST_P(ControlFlowTest, SwitchCase)
{
    eval("x = 2; switch x\n  case 1, y = 10;\n  case 2, y = 20;\n  otherwise, y = 0;\nend");
    EXPECT_DOUBLE_EQ(getVar("y"), 20.0);
}

TEST_P(ControlFlowTest, SwitchOtherwise)
{
    eval("x = 99; switch x\n  case 1, y = 10;\n  otherwise, y = -1;\nend");
    EXPECT_DOUBLE_EQ(getVar("y"), -1.0);
}

TEST_P(ControlFlowTest, SwitchCellCase)
{
    eval("x = 2; switch x\n  case {1, 2, 3}, y = 1;\n  otherwise, y = 0;\nend");
    EXPECT_DOUBLE_EQ(getVar("y"), 1.0);
}

TEST_P(ControlFlowTest, TryCatch)
{
    eval("try\n  error('oops');\ncatch e\n  msg = e.message;\nend");
    auto *msg = getVarPtr("msg");
    EXPECT_EQ(msg->toString(), "oops");
}

TEST_P(ControlFlowTest, TryNoCatch)
{
    // try without catch — error is silently ignored
    eval("try\n  error('oops');\nend");
}

TEST_P(ControlFlowTest, NestedForLoops)
{
    eval(R"(
        s = 0;
        for i = 1:3
            for j = 1:3
                s = s + i * j;
            end
        end
    )");
    // (1+2+3)*(1+2+3) = 36
    EXPECT_DOUBLE_EQ(getVar("s"), 36.0);
}

TEST_P(ControlFlowTest, WhileBreak)
{
    eval(R"(
        x = 0;
        while true
            x = x + 1;
            if x >= 5, break; end
        end
    )");
    EXPECT_DOUBLE_EQ(getVar("x"), 5.0);
}

TEST_P(ControlFlowTest, ForOverMatrix)
{
    eval(R"(
        A = [1 2 3; 4 5 6];
        s = 0;
        for i = 1:size(A, 1)
            for j = 1:size(A, 2)
                s = s + A(i, j);
            end
        end
    )");
    EXPECT_DOUBLE_EQ(getVar("s"), 21.0);
}

// ============================================================
// Try/Catch comprehensive tests
// ============================================================

TEST_P(ControlFlowTest, TryCatchMessage)
{
    eval("try; error('hello'); catch e; m = e.message; end");
    EXPECT_EQ(getVarPtr("m")->toString(), "hello");
}

TEST_P(ControlFlowTest, TryCatchIdentifier)
{
    eval("try; error('mylib:badarg', 'bad value'); catch e; id = e.identifier; m = e.message; end");
    EXPECT_EQ(getVarPtr("id")->toString(), "mylib:badarg");
    EXPECT_EQ(getVarPtr("m")->toString(), "bad value");
}

TEST_P(ControlFlowTest, TryCatchDefaultIdentifier)
{
    // error without explicit ID → "m:error"
    eval("try; error('oops'); catch e; id = e.identifier; end");
    EXPECT_EQ(getVarPtr("id")->toString(), "m:error");
}

TEST_P(ControlFlowTest, TryCatchSprintf)
{
    eval("try; error('value is %d', 42); catch e; m = e.message; end");
    EXPECT_EQ(getVarPtr("m")->toString(), "value is 42");
}

TEST_P(ControlFlowTest, TryCatchIdSprintf)
{
    eval("try; error('my:id', 'x=%d y=%d', 3, 7); catch e; m = e.message; id = e.identifier; end");
    EXPECT_EQ(getVarPtr("m")->toString(), "x=3 y=7");
    EXPECT_EQ(getVarPtr("id")->toString(), "my:id");
}

TEST_P(ControlFlowTest, TryCatchNoError)
{
    // No error in try body → catch body is NOT executed
    eval("r = 0; try; r = 1; catch e; r = 2; end");
    EXPECT_DOUBLE_EQ(getVar("r"), 1.0);
}

TEST_P(ControlFlowTest, TryCatchWithoutVariable)
{
    // catch without variable name — just suppress error
    eval("r = 0; try; error('fail'); catch; r = 1; end");
    EXPECT_DOUBLE_EQ(getVar("r"), 1.0);
}

TEST_P(ControlFlowTest, TryWithoutCatchBody)
{
    // try without catch clause — silently ignore
    eval("r = 1; try; error('fail'); end; r = 2;");
    EXPECT_DOUBLE_EQ(getVar("r"), 2.0);
}

TEST_P(ControlFlowTest, NestedTryCatch)
{
    eval(R"(
        r = 0;
        try
            try
                error('inner');
            catch e1
                r = r + 1;
                error('outer');
            end
        catch e2
            r = r + 10;
            m = e2.message;
        end
    )");
    EXPECT_DOUBLE_EQ(getVar("r"), 11.0);
    EXPECT_EQ(getVarPtr("m")->toString(), "outer");
}

TEST_P(ControlFlowTest, TryCatchInFor)
{
    eval(R"(
        s = 0;
        for i = 1:5
            try
                if mod(i, 2) == 0
                    error('even');
                end
                s = s + i;
            catch
                s = s + 100;
            end
        end
    )");
    // i=1: +1, i=2: +100, i=3: +3, i=4: +100, i=5: +5
    EXPECT_DOUBLE_EQ(getVar("s"), 209.0);
}

TEST_P(ControlFlowTest, TryCatchPreservesVars)
{
    // Variables set in try body are visible after catch
    eval("x = 1; try; x = 2; y = 3; error('e'); catch; end");
    EXPECT_DOUBLE_EQ(getVar("x"), 2.0);
    EXPECT_DOUBLE_EQ(getVar("y"), 3.0);
}

TEST_P(ControlFlowTest, TryCatchBreakInFor)
{
    // break inside try inside for exits the for loop
    eval(R"(
        s = 0;
        for i = 1:10
            try
                s = s + i;
                if i == 3, break; end
            catch
            end
        end
    )");
    EXPECT_DOUBLE_EQ(getVar("s"), 6.0); // 1+2+3
}

TEST_P(ControlFlowTest, TryCatchContinueInFor)
{
    eval(R"(
        s = 0;
        for i = 1:5
            try
                if mod(i, 2) == 0, continue; end
                s = s + i;
            catch
            end
        end
    )");
    EXPECT_DOUBLE_EQ(getVar("s"), 9.0); // 1+3+5
}

TEST_P(ControlFlowTest, MExceptionCreate)
{
    eval("me = MException('mylib:test', 'value %d', 42);");
    auto *me = getVarPtr("me");
    ASSERT_NE(me, nullptr);
    EXPECT_TRUE(me->isStruct());
    EXPECT_EQ(me->field("identifier").toString(), "mylib:test");
    EXPECT_EQ(me->field("message").toString(), "value 42");
}

TEST_P(ControlFlowTest, MExceptionThrow)
{
    eval(R"(
        try
            me = MException('test:err', 'bad');
            throw(me);
        catch e
            id = e.identifier;
            m = e.message;
        end
    )");
    EXPECT_EQ(getVarPtr("id")->toString(), "test:err");
    EXPECT_EQ(getVarPtr("m")->toString(), "bad");
}

TEST_P(ControlFlowTest, Rethrow)
{
    eval(R"(
        try
            try
                error('deep:err', 'inner problem');
            catch e1
                rethrow(e1);
            end
        catch e2
            id = e2.identifier;
            m = e2.message;
        end
    )");
    EXPECT_EQ(getVarPtr("id")->toString(), "deep:err");
    EXPECT_EQ(getVarPtr("m")->toString(), "inner problem");
}

TEST_P(ControlFlowTest, ErrorWithStruct)
{
    // error(struct) — throw an MException-like struct directly
    eval(R"(
        me = struct();
        me.identifier = 'my:id';
        me.message = 'struct error';
        try
            error(me);
        catch e
            id = e.identifier;
            m = e.message;
        end
    )");
    EXPECT_EQ(getVarPtr("id")->toString(), "my:id");
    EXPECT_EQ(getVarPtr("m")->toString(), "struct error");
}

TEST_P(ControlFlowTest, AssertPass)
{
    // assert(true) should not throw
    eval("assert(true);");
    eval("assert(1);");
    eval("assert(5 > 3);");
}

TEST_P(ControlFlowTest, AssertFail)
{
    EXPECT_THROW(eval("assert(false);"), std::exception);
}

TEST_P(ControlFlowTest, AssertFailMessage)
{
    eval("try; assert(false, 'custom msg'); catch e; m = e.message; end");
    EXPECT_EQ(getVarPtr("m")->toString(), "custom msg");
}

TEST_P(ControlFlowTest, AssertFailIdentifier)
{
    eval("try; assert(false, 'my:check', 'val is %d', 7); catch e; id = e.identifier; m = e.message; end");
    EXPECT_EQ(getVarPtr("id")->toString(), "my:check");
    EXPECT_EQ(getVarPtr("m")->toString(), "val is 7");
}

TEST_P(ControlFlowTest, AssertDefaultIdentifier)
{
    eval("try; assert(false); catch e; id = e.identifier; end");
    EXPECT_EQ(getVarPtr("id")->toString(), "m:assert");
}

TEST_P(ControlFlowTest, TryCatchReturnInFunction)
{
    eval(R"(
        function r = testfun()
            try
                error('fail');
            catch
                r = 42;
                return;
            end
            r = 0;
        end
        result = testfun();
    )");
    EXPECT_DOUBLE_EQ(getVar("result"), 42.0);
}

TEST_P(ControlFlowTest, TryCatchIndexError)
{
    eval(R"(
        try
            x = [1 2 3];
            y = x(10);
        catch e
            m = e.message;
        end
    )");
    auto *m = getVarPtr("m");
    ASSERT_NE(m, nullptr);
    // Message should mention index/bounds
    EXPECT_NE(m->toString().find("ndex"), std::string::npos);
}

TEST_P(ControlFlowTest, TryCatchDimError)
{
    eval(R"(
        try
            a = [1 2] + [1 2 3];
        catch e
            m = e.message;
        end
    )");
    auto *m = getVarPtr("m");
    ASSERT_NE(m, nullptr);
    EXPECT_NE(m->toString().find("imension"), std::string::npos);
}

TEST_P(ControlFlowTest, WarningNoThrow)
{
    // warning() should not throw
    eval("warning('test warning');");
    eval("warning('my:id', 'value is %d', 5);");
}

TEST_P(ControlFlowTest, TripleTryCatchNesting)
{
    eval(R"(
        r = '';
        try
            try
                try
                    error('level3');
                catch e3
                    r = e3.message;
                    error('level2');
                end
            catch e2
                r = strcat(r, '+', e2.message);
                error('level1');
            end
        catch e1
            r = strcat(r, '+', e1.message);
        end
    )");
    EXPECT_EQ(getVarPtr("r")->toString(), "level3+level2+level1");
}

TEST_P(ControlFlowTest, TryCatchWhile)
{
    eval(R"(
        n = 0;
        while true
            try
                n = n + 1;
                if n >= 3, break; end
            catch
            end
        end
    )");
    EXPECT_DOUBLE_EQ(getVar("n"), 3.0);
}

TEST_P(ControlFlowTest, ErrorNoArgs)
{
    // error() with no args should still throw
    EXPECT_THROW(eval("error()"), std::exception);
}

TEST_P(ControlFlowTest, AssertMException)
{
    // assert(false, MException('id','msg'))
    eval(R"(
        try
            me = MException('test:assert', 'custom assert');
            assert(false, me);
        catch e
            id = e.identifier;
            m = e.message;
        end
    )");
    EXPECT_EQ(getVarPtr("id")->toString(), "test:assert");
    EXPECT_EQ(getVarPtr("m")->toString(), "custom assert");
}

TEST_P(ControlFlowTest, ForTryCatchForStackCleanup)
{
    // Verify forStack_ is properly cleaned up when exception occurs inside nested for
    eval(R"(
        result = 0;
        for i = 1:3
            try
                for j = 1:5
                    if j == 3
                        error('bail');
                    end
                end
            catch
            end
            result = result + i;
        end
    )");
    EXPECT_DOUBLE_EQ(getVar("result"), 6.0); // 1+2+3
}

TEST_P(ControlFlowTest, TryCatchInAnonymousFunc)
{
    eval(R"(
        f = @() 42;
        try
            r = f();
        catch
            r = -1;
        end
    )");
    EXPECT_DOUBLE_EQ(getVar("r"), 42.0);
}

TEST_P(ControlFlowTest, WarningDoesNotThrow)
{
    // warning with various arg forms should never throw
    eval("warning('simple');");
    eval("warning('my:id', 'with id');");
    eval("warning('val %d', 42);");
    eval("warning('my:id', 'val %d', 42);");
}

TEST_P(ControlFlowTest, DeepForTryCatch)
{
    // 3-level nested for with try/catch at middle level
    eval(R"(
        s = 0;
        for i = 1:2
            for j = 1:2
                try
                    for k = 1:10
                        if k > 2, error('stop'); end
                        s = s + 1;
                    end
                catch
                end
            end
        end
    )");
    // Each (i,j) pair runs k=1,2 then error at k=3: 2*2*2 = 8
    EXPECT_DOUBLE_EQ(getVar("s"), 8.0);
}

INSTANTIATE_DUAL(ControlFlowTest);
