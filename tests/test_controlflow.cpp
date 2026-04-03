// tests/test_controlflow.cpp — Control flow: if, for, while, switch, try/catch
// Parameterized: runs on both TreeWalker and VM backends

#include "dual_engine_fixture.hpp"

using namespace mlab_test;

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

INSTANTIATE_DUAL(ControlFlowTest);
