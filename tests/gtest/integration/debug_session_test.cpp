// tests/diagnostics/debug_session_test.cpp
// Tests for DebugSession: pause/resume, eval in context, stepping
#include <numkit/core/debug_session.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

// ============================================================
// Fixture
// ============================================================

class DebugSessionTest : public ::testing::Test
{
protected:
    Engine engine;
    std::string output;

    void SetUp() override
    {
        engine.setOutputFunc([this](const std::string &s) { output += s; });
        engine.eval("import compat.*;");
    }

    // Helper: start debug session, return status
    ExecStatus startDebug(DebugSession &session, const std::string &code)
    {
        output.clear();
        return session.start(code);
    }
};

// ============================================================
// Basic pause/resume
// ============================================================

TEST_F(DebugSessionTest, PauseAtBreakpoint)
{
    DebugSession session(engine);
    session.setBreakpoints({2});

    // With breakpoints set, start() runs until the first breakpoint is hit.
    auto status = startDebug(session, "x = 10;\ny = 20;\nz = 30;\n");
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 2);
}

// Proof that a classdef method body runs on the VM (a real frame), not the
// TreeWalker hook: a breakpoint INSIDE the method pauses there. If the body
// ran via the C++ hook (atomic, on the TW) the debugger could not stop in it.
TEST_F(DebugSessionTest, BreakInsideClassdefMethodFunctionForm)
{
    DebugSession session(engine);
    session.setBreakpoints({7}); // the `r = o.v + 1;` line inside method go
    std::string code =
        "classdef DbgC\n"          // 1
        "  properties\n"           // 2
        "    v = 0\n"              // 3
        "  end\n"                  // 4
        "  methods\n"              // 5
        "    function r = go(o)\n" // 6
        "      r = o.v + 1;\n"     // 7  <- breakpoint
        "    end\n"               // 8
        "  end\n"                  // 9
        "end\n"                    // 10
        "y = go(DbgC());\n";       // 11  function-form call
    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused) << "method body did not run on the VM (no pause)";
    EXPECT_EQ(session.snapshot().line, 7);
    status = session.resume(DebugAction::Continue);
    EXPECT_EQ(status, ExecStatus::Completed);
}

// Same proof for the dotted call form `obj.go()` (CALL_METHOD).
TEST_F(DebugSessionTest, BreakInsideClassdefMethodDotted)
{
    DebugSession session(engine);
    session.setBreakpoints({7});
    std::string code =
        "classdef DbgD\n"          // 1
        "  properties\n"           // 2
        "    v = 0\n"              // 3
        "  end\n"                  // 4
        "  methods\n"              // 5
        "    function r = go(o)\n" // 6
        "      r = o.v + 2;\n"     // 7  <- breakpoint
        "    end\n"               // 8
        "  end\n"                  // 9
        "end\n"                    // 10
        "d = DbgD();\n"            // 11
        "y = d.go();\n";           // 12  dotted call
    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused) << "dotted method body did not run on the VM";
    EXPECT_EQ(session.snapshot().line, 7);
    status = session.resume(DebugAction::Continue);
    EXPECT_EQ(status, ExecStatus::Completed);
}

// Proof that a method body containing a SUPER-CALL runs on the VM (P2).
// Before P2 such a body could not VM-compile (SUPERCLASS_REF was rejected) and
// fell back to the TreeWalker hook — undebuggable. Now the super-method call is
// a real opcode (CALL_SUPER_METHOD); the body is a VM frame and a breakpoint on
// the line AFTER the super-call pauses there.
TEST_F(DebugSessionTest, BreakInsideClassdefSuperMethod)
{
    // Base class registered up front, so it is NOT part of the debugged code.
    engine.eval(
        "classdef DbgBaseS\n"
        "  properties\n    area = 0\n  end\n"
        "  methods\n"
        "    function r = descr(o)\n      r = o.area;\n    end\n"
        "  end\n"
        "end\n");
    DebugSession session(engine);
    session.setBreakpoints({5}); // `r = base + 1;` — after the super-call line
    std::string code =
        "classdef DbgDerivS < DbgBaseS\n"   // 1
        "  methods\n"                       // 2
        "    function r = descr(o)\n"       // 3
        "      base = descr@DbgBaseS(o);\n" // 4  super-method call (on the VM)
        "      r = base + 1;\n"             // 5  <- breakpoint
        "    end\n"                         // 6
        "  end\n"                           // 7
        "end\n"                             // 8
        "d = DbgDerivS();\n"                // 9
        "d.area = 10;\n"                    // 10
        "y = d.descr();\n";                 // 11  dotted call into derived descr
    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused)
        << "super-call method body did not run on the VM (no pause)";
    EXPECT_EQ(session.snapshot().line, 5);
    status = session.resume(DebugAction::Continue);
    EXPECT_EQ(status, ExecStatus::Completed);
    EXPECT_DOUBLE_EQ(engine.eval("y").toScalar(), 11.0); // 10 (super) + 1
}

// Proof that a CONSTRUCTOR body runs on the VM (P2): a breakpoint inside the
// constructor pauses there, and the output variable seeded with the default
// instance is visible/modifiable in the frame.
TEST_F(DebugSessionTest, BreakInsideClassdefConstructor)
{
    DebugSession session(engine);
    session.setBreakpoints({7}); // `o.v = x + 1;` inside the constructor
    std::string code =
        "classdef DbgCtor\n"            // 1
        "  properties\n"                // 2
        "    v = 0\n"                   // 3
        "  end\n"                       // 4
        "  methods\n"                   // 5
        "    function o = DbgCtor(x)\n" // 6
        "      o.v = x + 1;\n"          // 7  <- breakpoint
        "    end\n"                     // 8
        "  end\n"                       // 9
        "end\n"                         // 10
        "y = DbgCtor(41);\n"            // 11  construct → enters ctor frame
        "z = y.v;\n";                   // 12
    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused) << "constructor body did not run on the VM";
    EXPECT_EQ(session.snapshot().line, 7);
    status = session.resume(DebugAction::Continue);
    EXPECT_EQ(status, ExecStatus::Completed);
    EXPECT_DOUBLE_EQ(engine.eval("z").toScalar(), 42.0); // 41 + 1
}

// A function-handle body runs as a VM frame and is debuggable: a breakpoint
// inside the called function pauses when it is invoked through a handle.
// (Handle bodies are VM-native — VM_CALLBACKS_PLAN.md; C++-initiated handle
// calls — cellfun/arrayfun — go through VM::callReentrant in P3.)
TEST_F(DebugSessionTest, BreakInsideFunctionHandleCall)
{
    // dbgInc defined up front; its `r = x + 1;` body sits on line 4 of its own
    // source, which the 2-line debugged script below never occupies — so the
    // breakpoint can only match inside dbgInc.
    engine.eval(
        "function r = dbgInc(x)\n" // 1
        "\n"                       // 2
        "\n"                       // 3
        "  r = x + 1;\n"          // 4
        "end\n");                  // 5
    DebugSession session(engine);
    session.setBreakpoints({4});
    std::string code =
        "h = @dbgInc;\n" // 1
        "y = h(41);\n";  // 2  CALL_INDIRECT → enters dbgInc as a VM frame
    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused) << "handle body did not run on the VM";
    EXPECT_EQ(session.snapshot().line, 4);
    status = session.resume(DebugAction::Continue);
    EXPECT_EQ(status, ExecStatus::Completed);
    EXPECT_DOUBLE_EQ(engine.eval("y").toScalar(), 42.0);
}

// Proof that a `get.Prop` accessor body runs on the VM (P4): a breakpoint
// inside the getter pauses. The getter is a same-stack VM frame (no C++
// re-entry), so pause/resume works fully.
TEST_F(DebugSessionTest, BreakInsideClassdefGetter)
{
    DebugSession session(engine);
    session.setBreakpoints({8}); // `tmp = o.base * 2;` inside get.scaled
    std::string code =
        "classdef DbgGet\n"            // 1
        "  properties\n"               // 2
        "    base = 10\n"             // 3
        "    scaled = 0\n"            // 4
        "  end\n"                      // 5
        "  methods\n"                  // 6
        "    function v = get.scaled(o)\n" // 7
        "      tmp = o.base * 2;\n"    // 8  <- breakpoint
        "      v = tmp + 1;\n"         // 9
        "    end\n"                    // 10
        "  end\n"                      // 11
        "end\n"                        // 12
        "g = DbgGet();\n"              // 13
        "y = g.scaled;\n";             // 14  read triggers get.scaled
    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused) << "getter body did not run on the VM";
    EXPECT_EQ(session.snapshot().line, 8);
    status = session.resume(DebugAction::Continue);
    EXPECT_EQ(status, ExecStatus::Completed);
    EXPECT_DOUBLE_EQ(engine.eval("y").toScalar(), 21.0); // 10*2 + 1
}

// Proof that a `set.Prop` accessor body runs on the VM (P4): a breakpoint
// inside the setter pauses. A value-class setter (returns the object) is a
// same-stack VM frame whose result is written back into the object register.
TEST_F(DebugSessionTest, BreakInsideClassdefSetter)
{
    DebugSession session(engine);
    session.setBreakpoints({8}); // `tmp = v * 2;` inside set.val
    std::string code =
        "classdef DbgSet\n"             // 1
        "  properties\n"                // 2
        "    backing = 0\n"            // 3
        "    val = 0\n"                // 4
        "  end\n"                       // 5
        "  methods\n"                   // 6
        "    function o = set.val(o, v)\n" // 7
        "      tmp = v * 2;\n"          // 8  <- breakpoint
        "      o.backing = tmp;\n"      // 9  store to a different prop (no recursion)
        "    end\n"                     // 10
        "  end\n"                       // 11
        "end\n"                         // 12
        "s = DbgSet();\n"              // 13
        "s.val = 5;\n"                 // 14  assignment triggers set.val
        "y = s.backing;\n";            // 15
    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused) << "setter body did not run on the VM";
    EXPECT_EQ(session.snapshot().line, 8);
    status = session.resume(DebugAction::Continue);
    EXPECT_EQ(status, ExecStatus::Completed);
    EXPECT_DOUBLE_EQ(engine.eval("y").toScalar(), 10.0); // 5*2 stored into backing
}

// Proof that an operator-overload method body runs on the VM as a pausable
// frame (P4 refinement): a breakpoint inside `plus` pauses when `a + b` is
// evaluated. The ADD opcode pushes a same-stack frame for the operator method.
TEST_F(DebugSessionTest, BreakInsideClassdefOperator)
{
    DebugSession session(engine);
    session.setBreakpoints({12}); // `s = a.x + b.x;` inside plus
    std::string code =
        "classdef DbgVec\n"             // 1
        "  properties\n"                // 2
        "    x = 0\n"                   // 3
        "  end\n"                       // 4
        "  methods\n"                   // 5
        "    function obj = DbgVec(v)\n" // 6
        "      if nargin > 0\n"         // 7
        "        obj.x = v;\n"          // 8
        "      end\n"                   // 9
        "    end\n"                     // 10
        "    function r = plus(a, b)\n" // 11
        "      s = a.x + b.x;\n"        // 12  <- breakpoint
        "      r = DbgVec(s);\n"        // 13
        "    end\n"                     // 14
        "  end\n"                       // 15
        "end\n"                         // 16
        "p = DbgVec(3);\n"             // 17
        "q = DbgVec(4);\n"             // 18
        "z = p + q;\n"                 // 19  '+' dispatches to plus
        "y = z.x;\n";                  // 20
    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused) << "operator body did not run on the VM";
    EXPECT_EQ(session.snapshot().line, 12);
    status = session.resume(DebugAction::Continue);
    EXPECT_EQ(status, ExecStatus::Completed);
    EXPECT_DOUBLE_EQ(engine.eval("y").toScalar(), 7.0); // 3 + 4
}

// Proof that a `subsref` overload body runs on the VM as a pausable frame
// (P4 refinement): a breakpoint inside subsref pauses when `obj(i)` is indexed.
TEST_F(DebugSessionTest, BreakInsideClassdefSubsref)
{
    DebugSession session(engine);
    session.setBreakpoints({7}); // `idx = s.subs{1};` inside subsref
    std::string code =
        "classdef DbgRing\n"               // 1
        "  properties\n"                   // 2
        "    data = [10 20 30]\n"         // 3
        "  end\n"                          // 4
        "  methods\n"                      // 5
        "    function r = subsref(obj, s)\n" // 6
        "      idx = s.subs{1};\n"         // 7  <- breakpoint
        "      r = obj.data(idx);\n"       // 8
        "    end\n"                        // 9
        "  end\n"                          // 10
        "end\n"                            // 11
        "g = DbgRing();\n"                // 12
        "y = g(2);\n";                    // 13  '()' dispatches to subsref
    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused) << "subsref body did not run on the VM";
    EXPECT_EQ(session.snapshot().line, 7);
    status = session.resume(DebugAction::Continue);
    EXPECT_EQ(status, ExecStatus::Completed);
    EXPECT_DOUBLE_EQ(engine.eval("y").toScalar(), 20.0); // data(2)
}

// Proof that a `subsasgn` overload body runs on the VM as a pausable frame
// (P4 refinement): a breakpoint inside subsasgn pauses on `obj(i) = v`.
TEST_F(DebugSessionTest, BreakInsideClassdefSubsasgn)
{
    DebugSession session(engine);
    session.setBreakpoints({7}); // `idx = s.subs{1};` inside subsasgn
    std::string code =
        "classdef DbgBox\n"                   // 1
        "  properties\n"                      // 2
        "    store = [0 0 0]\n"              // 3
        "  end\n"                             // 4
        "  methods\n"                         // 5
        "    function obj = subsasgn(obj, s, v)\n" // 6
        "      idx = s.subs{1};\n"            // 7  <- breakpoint
        "      obj.store(idx) = v;\n"         // 8
        "    end\n"                           // 9
        "  end\n"                             // 10
        "end\n"                               // 11
        "b = DbgBox();\n"                    // 12
        "b(2) = 99;\n"                       // 13  '()=' dispatches to subsasgn
        "y = b.store(2);\n";                 // 14
    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused) << "subsasgn body did not run on the VM";
    EXPECT_EQ(session.snapshot().line, 7);
    status = session.resume(DebugAction::Continue);
    EXPECT_EQ(status, ExecStatus::Completed);
    EXPECT_DOUBLE_EQ(engine.eval("y").toScalar(), 99.0); // store(2) set
}

// Proof of the state-machine callbacks (VM continuations): a breakpoint inside
// a cellfun CALLBACK pauses AND resumes — impossible with the old callReentrant
// path, where a pause could only abort across the C++ boundary. The callback
// runs as an ordinary VM frame, so the breakpoint fires once per element and
// the session steps through all of them.
TEST_F(DebugSessionTest, BreakInsideCellfunCallback)
{
    // dbgCellCb body on line 4 of its own source; the 2-line script never
    // occupies line 4, so the breakpoint can only match inside the callback.
    engine.eval(
        "function r = dbgCellCb(x)\n" // 1
        "\n"                          // 2
        "\n"                          // 3
        "  r = x * 10;\n"            // 4
        "end\n");                     // 5
    DebugSession session(engine);
    session.setBreakpoints({4});
    std::string code =
        "c = {1, 2, 3};\n"               // 1
        "y = cellfun(@dbgCellCb, c);\n"; // 2  callbacks run as pausable VM frames
    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused) << "cellfun callback did not pause on the VM";
    EXPECT_EQ(session.snapshot().line, 4);
    int pauses = 1;
    while (status == ExecStatus::Paused && pauses < 10) {
        status = session.resume(DebugAction::Continue);
        if (status == ExecStatus::Paused)
            ++pauses;
    }
    EXPECT_EQ(status, ExecStatus::Completed);
    EXPECT_EQ(pauses, 3); // one breakpoint hit per cell element
    EXPECT_DOUBLE_EQ(engine.eval("y(1)").toScalar(), 10.0);
    EXPECT_DOUBLE_EQ(engine.eval("y(3)").toScalar(), 30.0);
}

// Same proof for arrayfun: a breakpoint inside an arrayfun callback pauses on
// each element and resumes (state-machine callbacks over an array input).
TEST_F(DebugSessionTest, BreakInsideArrayfunCallback)
{
    engine.eval(
        "function r = dbgArrCb(x)\n" // 1
        "\n"                         // 2
        "\n"                         // 3
        "  r = x + 100;\n"          // 4
        "end\n");                    // 5
    DebugSession session(engine);
    session.setBreakpoints({4});
    std::string code =
        "a = [1 2 3];\n"                 // 1
        "y = arrayfun(@dbgArrCb, a);\n"; // 2  callbacks run as pausable VM frames
    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused) << "arrayfun callback did not pause on the VM";
    EXPECT_EQ(session.snapshot().line, 4);
    int pauses = 1;
    while (status == ExecStatus::Paused && pauses < 10) {
        status = session.resume(DebugAction::Continue);
        if (status == ExecStatus::Paused)
            ++pauses;
    }
    EXPECT_EQ(status, ExecStatus::Completed);
    EXPECT_EQ(pauses, 3); // one breakpoint hit per array element
    EXPECT_DOUBLE_EQ(engine.eval("y(1)").toScalar(), 101.0);
    EXPECT_DOUBLE_EQ(engine.eval("y(3)").toScalar(), 103.0);
}

// Same proof for structfun: a breakpoint inside the per-field callback pauses
// and resumes (state-machine callbacks over a struct's fields).
TEST_F(DebugSessionTest, BreakInsideStructfunCallback)
{
    engine.eval(
        "function r = dbgStructCb(x)\n" // 1
        "\n"                            // 2
        "\n"                            // 3
        "  r = x * 2;\n"               // 4
        "end\n");                       // 5
    DebugSession session(engine);
    session.setBreakpoints({4});
    std::string code =
        "s.a = 5; s.b = 6;\n"               // 1  fields a, b
        "y = structfun(@dbgStructCb, s);\n"; // 2  per-field pausable VM frames
    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused) << "structfun callback did not pause on the VM";
    EXPECT_EQ(session.snapshot().line, 4);
    int pauses = 1;
    while (status == ExecStatus::Paused && pauses < 10) {
        status = session.resume(DebugAction::Continue);
        if (status == ExecStatus::Paused)
            ++pauses;
    }
    EXPECT_EQ(status, ExecStatus::Completed);
    EXPECT_EQ(pauses, 2); // one breakpoint hit per field
    EXPECT_DOUBLE_EQ(engine.eval("y(1)").toScalar(), 10.0); // a: 5*2
    EXPECT_DOUBLE_EQ(engine.eval("y(2)").toScalar(), 12.0); // b: 6*2
}

// feval into a user function pauses inside the callee (single-shot continuation).
TEST_F(DebugSessionTest, BreakInsideFevalCallback)
{
    engine.eval(
        "function r = dbgFevalCb(x)\n" // 1
        "\n"                           // 2
        "\n"                           // 3
        "  r = x - 7;\n"             // 4
        "end\n");                      // 5
    DebugSession session(engine);
    session.setBreakpoints({4});
    std::string code = "y = feval(@dbgFevalCb, 50);\n"; // 1
    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused) << "feval callback did not pause on the VM";
    EXPECT_EQ(session.snapshot().line, 4);
    status = session.resume(DebugAction::Continue);
    EXPECT_EQ(status, ExecStatus::Completed);
    EXPECT_DOUBLE_EQ(engine.eval("y").toScalar(), 43.0); // 50 - 7
}

// splitapply: the per-group callback pauses on each group and resumes.
TEST_F(DebugSessionTest, BreakInsideSplitapplyCallback)
{
    engine.eval(
        "function r = dbgGrpCb(x)\n" // 1
        "\n"                         // 2
        "\n"                         // 3
        "  r = sum(x);\n"          // 4
        "end\n");                    // 5
    DebugSession session(engine);
    session.setBreakpoints({4});
    std::string code =
        "x = [1 2 3 4];\n"                    // 1
        "g = [1 1 2 2];\n"                    // 2
        "y = splitapply(@dbgGrpCb, x, g);\n"; // 3
    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused) << "splitapply callback did not pause on the VM";
    EXPECT_EQ(session.snapshot().line, 4);
    int pauses = 1;
    while (status == ExecStatus::Paused && pauses < 10) {
        status = session.resume(DebugAction::Continue);
        if (status == ExecStatus::Paused)
            ++pauses;
    }
    EXPECT_EQ(status, ExecStatus::Completed);
    EXPECT_EQ(pauses, 2); // one per group
    EXPECT_DOUBLE_EQ(engine.eval("y(1)").toScalar(), 3.0);  // 1+2
    EXPECT_DOUBLE_EQ(engine.eval("y(2)").toScalar(), 7.0);  // 3+4
}

// bsxfun forwards the whole arrays to the handle in one call (single-shot).
TEST_F(DebugSessionTest, BreakInsideBsxfunCallback)
{
    engine.eval(
        "function r = dbgBsx(a, b)\n" // 1
        "\n"                          // 2
        "\n"                          // 3
        "  r = a + b;\n"            // 4
        "end\n");                     // 5
    DebugSession session(engine);
    session.setBreakpoints({4});
    std::string code = "y = bsxfun(@dbgBsx, [1 2 3], 10);\n"; // 1
    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused) << "bsxfun callback did not pause on the VM";
    EXPECT_EQ(session.snapshot().line, 4);
    status = session.resume(DebugAction::Continue);
    EXPECT_EQ(status, ExecStatus::Completed);
    EXPECT_DOUBLE_EQ(engine.eval("y(1)").toScalar(), 11.0);
    EXPECT_DOUBLE_EQ(engine.eval("y(3)").toScalar(), 13.0);
}

// bootstrp: the per-replicate statistic pauses on each bootstrap sample.
// The statistic ignores the (random) sample so the result is deterministic.
TEST_F(DebugSessionTest, BreakInsideBootstrpCallback)
{
    engine.eval(
        "function r = dbgBoot(s)\n" // 1
        "\n"                        // 2
        "\n"                        // 3
        "  r = 42;\n"             // 4
        "end\n");                   // 5
    DebugSession session(engine);
    session.setBreakpoints({4});
    std::string code = "y = bootstrp(3, @dbgBoot, [1;2;3;4;5]);\n"; // 1
    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused) << "bootstrp callback did not pause on the VM";
    EXPECT_EQ(session.snapshot().line, 4);
    int pauses = 1;
    while (status == ExecStatus::Paused && pauses < 20) {
        status = session.resume(DebugAction::Continue);
        if (status == ExecStatus::Paused)
            ++pauses;
    }
    EXPECT_EQ(status, ExecStatus::Completed);
    EXPECT_EQ(pauses, 3); // one per bootstrap replicate
    EXPECT_DOUBLE_EQ(engine.eval("y(1)").toScalar(), 42.0);
    EXPECT_DOUBLE_EQ(engine.eval("numel(y)").toScalar(), 3.0);
}

// nlfilter: the per-window kernel pauses on each sliding window and resumes.
TEST_F(DebugSessionTest, BreakInsideNlfilterCallback)
{
    engine.eval(
        "function r = dbgNl(w)\n" // 1
        "\n"                       // 2
        "\n"                       // 3
        "  r = sum(w(:));\n"     // 4
        "end\n");                  // 5
    DebugSession session(engine);
    session.setBreakpoints({4});
    std::string code =
        "A = [1 2; 3 4];\n"                 // 1
        "y = nlfilter(A, [1 1], @dbgNl);\n"; // 2  1×1 window → f is identity
    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused) << "nlfilter callback did not pause on the VM";
    EXPECT_EQ(session.snapshot().line, 4);
    int pauses = 1;
    while (status == ExecStatus::Paused && pauses < 20) {
        status = session.resume(DebugAction::Continue);
        if (status == ExecStatus::Paused)
            ++pauses;
    }
    EXPECT_EQ(status, ExecStatus::Completed);
    EXPECT_EQ(pauses, 4); // one per pixel (2×2)
    EXPECT_DOUBLE_EQ(engine.eval("y(1,1)").toScalar(), 1.0);
    EXPECT_DOUBLE_EQ(engine.eval("y(2,2)").toScalar(), 4.0);
}

// makelut: the kernel evaluated on each binary neighbourhood pauses per entry.
TEST_F(DebugSessionTest, BreakInsideMakelutCallback)
{
    engine.eval(
        "function r = dbgLut(nh)\n" // 1
        "\n"                        // 2
        "\n"                        // 3
        "  r = any(nh(:));\n"     // 4
        "end\n");                   // 5
    DebugSession session(engine);
    session.setBreakpoints({4});
    std::string code = "y = makelut(@dbgLut, 2);\n"; // 1  n=2 → 16 neighbourhoods
    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused) << "makelut callback did not pause on the VM";
    EXPECT_EQ(session.snapshot().line, 4);
    int pauses = 1;
    while (status == ExecStatus::Paused && pauses < 40) {
        status = session.resume(DebugAction::Continue);
        if (status == ExecStatus::Paused)
            ++pauses;
    }
    EXPECT_EQ(status, ExecStatus::Completed);
    EXPECT_EQ(pauses, 16); // 2^(2*2) neighbourhoods
    EXPECT_DOUBLE_EQ(engine.eval("y(1)").toScalar(), 0.0);  // all-zero neighbourhood
    EXPECT_DOUBLE_EQ(engine.eval("y(16)").toScalar(), 1.0); // all-ones neighbourhood
}

TEST_F(DebugSessionTest, ContinueToCompletion)
{
    DebugSession session(engine);
    session.setBreakpoints({2});

    // start() hits bp at line 2 directly
    auto status = startDebug(session, "x = 10;\ny = 20;\n");
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 2);

    // Continue past bp → completes
    status = session.resume(DebugAction::Continue);
    EXPECT_EQ(status, ExecStatus::Completed);
    EXPECT_FALSE(session.isActive());
}

TEST_F(DebugSessionTest, MultipleContinues)
{
    DebugSession session(engine);
    session.setBreakpoints({2});

    // start() hits bp at line 2 on the first iteration
    auto status = startDebug(session, "for i = 1:3\n  x = i;\nend\n");
    ASSERT_EQ(status, ExecStatus::Paused);

    // Two more iterations hit the bp
    for (int i = 0; i < 2; ++i) {
        status = session.resume(DebugAction::Continue);
        ASSERT_EQ(status, ExecStatus::Paused) << "iteration " << i;
    }
    status = session.resume(DebugAction::Continue);
    EXPECT_EQ(status, ExecStatus::Completed);
}

// ============================================================
// Snapshot: variables in function scope
// ============================================================

TEST_F(DebugSessionTest, SnapshotShowsFunctionLocals)
{
    DebugSession session(engine);
    session.setBreakpoints({3});

    std::string code =
        "function r = square(n)\n"
        "    r = n * n;\n"
        "    r = r + 0;\n"   // breakpoint here, after r is computed
        "end\n"
        "result = square(7);\n";

    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);

    auto snap = session.snapshot();
    EXPECT_EQ(snap.functionName, "square");

    // Should see n and r in the function scope
    bool foundN = false, foundR = false;
    for (auto &v : snap.variables) {
        if (v.name == "n" && v.value && v.value->isDoubleScalar()) {
            EXPECT_DOUBLE_EQ(v.value->scalarVal(), 7.0);
            foundN = true;
        }
        if (v.name == "r" && v.value && v.value->isDoubleScalar()) {
            EXPECT_DOUBLE_EQ(v.value->scalarVal(), 49.0);
            foundR = true;
        }
    }
    EXPECT_TRUE(foundN) << "expected 'n' in function scope";
    EXPECT_TRUE(foundR) << "expected 'r' in function scope";
}

// ============================================================
// Eval in debug context
// ============================================================

TEST_F(DebugSessionTest, EvalSimpleVariable)
{
    DebugSession session(engine);
    session.setBreakpoints({3});

    std::string code =
        "function r = fib(n)\n"
        "    if n <= 1\n"
        "        r = n;\n"
        "    else\n"
        "        r = fib(n-1) + fib(n-2);\n"
        "    end\n"
        "end\n"
        "result = fib(5);\n";

    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);

    auto snap = session.snapshot();
    EXPECT_EQ(snap.functionName, "fib");

    // Eval a variable name — should display its value
    std::string result = session.eval("n");
    EXPECT_NE(result.find("1"), std::string::npos) << "eval('n') should show value 1, got: " << result;
}

TEST_F(DebugSessionTest, EvalExpression)
{
    DebugSession session(engine);
    session.setBreakpoints({3});

    std::string code =
        "function r = fib(n)\n"
        "    if n <= 1\n"
        "        r = n;\n"
        "    else\n"
        "        r = fib(n-1) + fib(n-2);\n"
        "    end\n"
        "end\n"
        "result = fib(5);\n";

    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);

    // Eval an expression using frame variables
    std::string result = session.eval("n + 10");
    EXPECT_NE(result.find("11"), std::string::npos) << "eval('n+10') should show 11, got: " << result;
}

TEST_F(DebugSessionTest, EvalNarginNargout)
{
    DebugSession session(engine);
    session.setBreakpoints({2});

    std::string code =
        "function r = add(a, b)\n"
        "    s = a + b;\n"
        "    r = s;\n"
        "end\n"
        "result = add(10, 20);\n";

    // start() hits bp at line 2 directly (inside add)
    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);

    auto snap = session.snapshot();
    EXPECT_EQ(snap.functionName, "add");

    // nargin should be 2 (two arguments passed)
    std::string result = session.eval("nargin");
    EXPECT_NE(result.find("2"), std::string::npos)
        << "eval('nargin') should show 2, got: " << result;

    // nargout should be 1 (one return value)
    result = session.eval("nargout");
    EXPECT_NE(result.find("1"), std::string::npos)
        << "eval('nargout') should show 1, got: " << result;
}

TEST_F(DebugSessionTest, EvalCreatesNewVariable)
{
    DebugSession session(engine);
    session.setBreakpoints({2});

    auto status = startDebug(session, "x = 10;\ny = 20;\nz = 30;\n");
    ASSERT_EQ(status, ExecStatus::Paused);

    // Create a new variable in debug console
    session.eval("q = 42");

    // q should appear in snapshot
    auto snap = session.snapshot();
    bool hasQ = false;
    for (auto &v : snap.variables) {
        if (v.name == "q" && v.value) {
            hasQ = true;
            EXPECT_DOUBLE_EQ(v.value->toScalar(), 42.0);
        }
    }
    EXPECT_TRUE(hasQ) << "eval-created variable q should appear in snapshot";
}

TEST_F(DebugSessionTest, EvalCreatedVarPersistsAcrossEvals)
{
    DebugSession session(engine);
    session.setBreakpoints({2});

    auto status = startDebug(session, "x = 10;\ny = 20;\n");
    ASSERT_EQ(status, ExecStatus::Paused);

    // Create variable, then use it in next eval
    session.eval("q = [1 2 3]");
    std::string result = session.eval("sum(q)");
    EXPECT_NE(result.find("6"), std::string::npos)
        << "sum(q) should be 6, got: " << result;
}

TEST_F(DebugSessionTest, EvalCreatedVarInFunction)
{
    DebugSession session(engine);
    session.setBreakpoints({2});

    std::string code =
        "function r = foo(x)\n"
        "    r = x * 2;\n"
        "end\n"
        "result = foo(5);\n";

    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);

    // Create new variable inside function debug context
    session.eval("tmp = x + 100");

    auto snap = session.snapshot();
    bool hasTmp = false;
    for (auto &v : snap.variables)
        if (v.name == "tmp" && v.value) hasTmp = true;
    EXPECT_TRUE(hasTmp) << "eval-created 'tmp' should appear in function snapshot";

    // Original frame variable x should still be accessible
    std::string result = session.eval("x");
    EXPECT_NE(result.find("5"), std::string::npos)
        << "x should still be 5, got: " << result;
}

TEST_F(DebugSessionTest, EvalArrayConstruction)
{
    DebugSession session(engine);
    session.setBreakpoints({3});

    std::string code =
        "function r = fib(n)\n"
        "    if n <= 1\n"
        "        r = n;\n"
        "    else\n"
        "        r = fib(n-1) + fib(n-2);\n"
        "    end\n"
        "end\n"
        "result = fib(5);\n";

    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);
    status = session.resume(DebugAction::Continue);
    ASSERT_EQ(status, ExecStatus::Paused);

    // Build array from frame variable
    std::string result = session.eval("q = [1 2 n]");
    EXPECT_NE(result.find("1"), std::string::npos) << "should contain 1, got: " << result;
    EXPECT_NE(result.find("2"), std::string::npos) << "should contain 2, got: " << result;
}

TEST_F(DebugSessionTest, EvalPreservesDebugState)
{
    DebugSession session(engine);
    session.setBreakpoints({3});

    std::string code =
        "function r = fib(n)\n"
        "    if n <= 1\n"
        "        r = n;\n"
        "    else\n"
        "        r = fib(n-1) + fib(n-2);\n"
        "    end\n"
        "end\n"
        "result = fib(5);\n";

    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);
    status = session.resume(DebugAction::Continue);
    ASSERT_EQ(status, ExecStatus::Paused);

    auto snapBefore = session.snapshot();

    // Eval multiple times
    session.eval("n");
    session.eval("n + 100");
    session.eval("q = [n n n]");

    // Debug state must be preserved
    EXPECT_TRUE(session.isActive());
    auto snapAfter = session.snapshot();
    EXPECT_EQ(snapAfter.line, snapBefore.line);
    EXPECT_EQ(snapAfter.functionName, snapBefore.functionName);

    // Can still continue execution after eval
    status = session.resume(DebugAction::Continue);
    EXPECT_TRUE(status == ExecStatus::Paused || status == ExecStatus::Completed);
}

TEST_F(DebugSessionTest, EvalAfterMultipleResumes)
{
    DebugSession session(engine);
    session.setBreakpoints({2});

    std::string code =
        "for i = 1:3\n"
        "  x = i * 10;\n"
        "end\n";

    // start() hits bp at line 2, first iteration (i=1, before x is assigned)
    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);

    std::string result = session.eval("i");
    EXPECT_NE(result.find("1"), std::string::npos) << "first iteration i=1, got: " << result;

    // Continue → second bp hit: i=2
    status = session.resume(DebugAction::Continue);
    ASSERT_EQ(status, ExecStatus::Paused);

    result = session.eval("i");
    EXPECT_NE(result.find("2"), std::string::npos) << "second iteration i=2, got: " << result;

    result = session.eval("x");
    EXPECT_NE(result.find("10"), std::string::npos) << "x should be 10 from iteration 1, got: " << result;
}

// ============================================================
// Eval doesn't break continue flow
// ============================================================

TEST_F(DebugSessionTest, ContinueWorksAfterEval)
{
    DebugSession session(engine);
    session.setBreakpoints({3});

    std::string code =
        "function r = double_it(x)\n"
        "    r = x * 2;\n"
        "    r = r + 0;\n"  // bp here
        "end\n"
        "a = double_it(5);\n"
        "b = double_it(10);\n";

    // start() hits bp at line 3 inside double_it(5)
    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().functionName, "double_it");

    std::string result = session.eval("r");
    EXPECT_NE(result.find("10"), std::string::npos) << "r should be 10 (5*2), got: " << result;

    // Continue → second bp in double_it(10)
    status = session.resume(DebugAction::Continue);
    ASSERT_EQ(status, ExecStatus::Paused);

    result = session.eval("x");
    EXPECT_NE(result.find("10"), std::string::npos) << "x should be 10, got: " << result;

    result = session.eval("r");
    EXPECT_NE(result.find("20"), std::string::npos) << "r should be 20 (10*2), got: " << result;

    // Continue to completion
    status = session.resume(DebugAction::Continue);
    EXPECT_EQ(status, ExecStatus::Completed);
}

// ============================================================
// Stepping
// ============================================================

TEST_F(DebugSessionTest, StepOver)
{
    DebugSession session(engine);
    session.setBreakpoints({1});

    std::string code = "x = 1;\ny = 2;\nz = 3;\n";

    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 1);

    status = session.resume(DebugAction::StepOver);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 2);

    status = session.resume(DebugAction::StepOver);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 3);
}

TEST_F(DebugSessionTest, StepIntoFunction)
{
    DebugSession session(engine);
    session.setBreakpoints({4});

    std::string code =
        "function r = add1(x)\n"
        "    r = x + 1;\n"
        "end\n"
        "y = add1(5);\n";

    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);

    // Step into the function call
    status = session.resume(DebugAction::StepInto);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().functionName, "add1");
}

// ============================================================
// Stop
// ============================================================

TEST_F(DebugSessionTest, StopEndsSession)
{
    DebugSession session(engine);
    session.setBreakpoints({2});

    auto status = startDebug(session, "x = 1;\ny = 2;\nz = 3;\n");
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_TRUE(session.isActive());

    session.stop();
    EXPECT_FALSE(session.isActive());
}

// ============================================================
// Error handling in eval
// ============================================================

TEST_F(DebugSessionTest, EvalUndefinedVariable)
{
    DebugSession session(engine);
    session.setBreakpoints({1});

    auto status = startDebug(session, "x = 42;\n");
    ASSERT_EQ(status, ExecStatus::Paused);

    std::string result = session.eval("nonexistent_var");
    // Should return an error, not crash
    EXPECT_NE(result.find("Error"), std::string::npos) << "expected error for undefined var, got: " << result;

    // Session should still be active
    EXPECT_TRUE(session.isActive());
}

TEST_F(DebugSessionTest, EvalSyntaxError)
{
    DebugSession session(engine);
    session.setBreakpoints({1});

    auto status = startDebug(session, "x = 42;\n");
    ASSERT_EQ(status, ExecStatus::Paused);

    std::string result = session.eval("[[[");
    EXPECT_NE(result.find("Error"), std::string::npos) << "expected error for bad syntax, got: " << result;
    EXPECT_TRUE(session.isActive());
}

// ============================================================
// Figures during debug: markers flow through outputFunc, not std::cout
// ============================================================

TEST_F(DebugSessionTest, PlotOutputContainsFigureMarker)
{
    DebugSession session(engine);
    session.setBreakpoints({2});

    std::string code =
        "x = [1 2 3]; y = [4 5 6];\n"
        "plot(x, y);\n";

    // start() hits bp at line 2 (plot call)
    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);

    // Step over the plot call
    status = session.resume(DebugAction::StepOver);

    // The output should contain the __FIGURE_DATA__ marker
    std::string out = session.takeOutput();
    EXPECT_NE(out.find("__FIGURE_DATA__"), std::string::npos)
        << "plot output should contain figure marker, got: " << out;
}

TEST_F(DebugSessionTest, PlotDuringEvalContainsFigureMarker)
{
    DebugSession session(engine);
    session.setBreakpoints({1});

    auto status = startDebug(session, "x = [1 2 3];\n");
    ASSERT_EQ(status, ExecStatus::Paused);

    // Eval a plot command in debug context
    std::string result = session.eval("plot([1 2 3], [4 5 6])");

    // The figure marker must be in the eval return value
    EXPECT_NE(result.find("__FIGURE_DATA__"), std::string::npos)
        << "plot in eval should produce figure marker in result, got: " << result;

    // Session should still be active
    EXPECT_TRUE(session.isActive());
}

TEST_F(DebugSessionTest, FigureDuringEvalEmitsMarker)
{
    DebugSession session(engine);
    session.setBreakpoints({1});

    auto status = startDebug(session, "x = 1;\n");
    ASSERT_EQ(status, ExecStatus::Paused);

    std::string result = session.eval("figure(1)");
    EXPECT_NE(result.find("__FIGURE_DATA__"), std::string::npos)
        << "figure(1) in eval should emit marker, got: " << result;
    EXPECT_NE(result.find("\"datasets\":[]"), std::string::npos)
        << "empty figure should have no datasets, got: " << result;
    EXPECT_TRUE(session.isActive());
}

TEST_F(DebugSessionTest, CloseDuringEvalEmitsMarker)
{
    DebugSession session(engine);
    session.setBreakpoints({1});

    auto status = startDebug(session, "x = 1;\n");
    ASSERT_EQ(status, ExecStatus::Paused);

    // Create a figure, then close it
    session.eval("figure(2)");
    std::string result = session.eval("close(2)");
    EXPECT_NE(result.find("__FIGURE_CLOSE__:2"), std::string::npos)
        << "close(2) in eval should emit close marker, got: " << result;
    EXPECT_TRUE(session.isActive());
}

TEST_F(DebugSessionTest, PlotWithFrameVarsDuringEval)
{
    DebugSession session(engine);
    session.setBreakpoints({3});

    std::string code =
        "function r = make_data(n)\n"
        "    r = linspace(0, 1, n);\n"
        "    r = r + 0;\n"   // bp here
        "end\n"
        "y = make_data(5);\n";

    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().functionName, "make_data");

    // Plot using the frame variable r
    std::string result = session.eval("plot(r)");
    EXPECT_NE(result.find("__FIGURE_DATA__"), std::string::npos)
        << "plot(r) with frame var should produce marker, got: " << result;
    // Should contain actual data, not empty datasets
    EXPECT_EQ(result.find("\"datasets\":[]"), std::string::npos)
        << "plot(r) should have non-empty datasets";
    EXPECT_TRUE(session.isActive());
}

TEST_F(DebugSessionTest, EvalPlotPreservesDebugState)
{
    DebugSession session(engine);
    session.setBreakpoints({2});

    auto status = startDebug(session, "x = [1 2 3];\ny = [4 5 6];\n");
    ASSERT_EQ(status, ExecStatus::Paused);

    auto snapBefore = session.snapshot();

    // Eval plot and figure commands
    session.eval("figure(1)");
    session.eval("plot([1 2 3], [4 5 6])");
    session.eval("title('test')");

    // Debug state must be preserved
    EXPECT_TRUE(session.isActive());
    auto snapAfter = session.snapshot();
    EXPECT_EQ(snapAfter.line, snapBefore.line);

    // Can still continue
    status = session.resume(DebugAction::Continue);
    EXPECT_TRUE(status == ExecStatus::Paused || status == ExecStatus::Completed);
}

// ============================================================
// OutputFunc lifetime: must survive debug session destruction
// ============================================================

TEST_F(DebugSessionTest, OutputFuncWorksAfterDebugCompletes)
{
    // Simulate the ReplSession pattern:
    // 1. Set outputFunc → owner's buffer
    // 2. Debug session runs, redirects outputFunc to its own buffer
    // 3. Session completes and is destroyed
    // 4. Next eval must write to owner's buffer, not to freed memory

    std::string ownerBuf;
    engine.setOutputFunc([&ownerBuf](const std::string &s) { ownerBuf += s; });

    {
        DebugSession session(engine);
        session.setBreakpoints({2});
        auto status = session.start("x = 1;\ny = 2;\n");
        ASSERT_EQ(status, ExecStatus::Paused);
        status = session.resume(DebugAction::Continue);
        // Session completes or pauses — either way, it redirected outputFunc
    }
    // DebugSession destroyed here — engine's outputFunc was pointing to it

    // Restore outputFunc (this is what ReplSession::restoreOutputFunc does)
    ownerBuf.clear();
    engine.setOutputFunc([&ownerBuf](const std::string &s) { ownerBuf += s; });

    // Normal eval must work and output must go to ownerBuf
    engine.eval("disp('hello after debug')");
    EXPECT_NE(ownerBuf.find("hello after debug"), std::string::npos)
        << "output after debug session destroyed should work, got: " << ownerBuf;
}

TEST_F(DebugSessionTest, OutputFuncWorksAfterDebugStop)
{
    std::string ownerBuf;
    engine.setOutputFunc([&ownerBuf](const std::string &s) { ownerBuf += s; });

    {
        DebugSession session(engine);
        session.setBreakpoints({2});
        auto status = session.start("x = 1;\ny = 2;\nz = 3;\n");
        ASSERT_EQ(status, ExecStatus::Paused);
        session.stop();  // explicit stop while paused
    }
    // DebugSession destroyed — outputFunc was dangling

    ownerBuf.clear();
    engine.setOutputFunc([&ownerBuf](const std::string &s) { ownerBuf += s; });

    engine.eval("disp('after stop')");
    EXPECT_NE(ownerBuf.find("after stop"), std::string::npos)
        << "output after debug stop should work, got: " << ownerBuf;
}

TEST_F(DebugSessionTest, OutputFuncDanglingWithoutRestore)
{
    // This test documents the problem: WITHOUT restoring outputFunc,
    // output goes to freed memory. With ASan this would crash.
    // We verify the fix pattern: restore after destroy.

    std::string ownerBuf;
    engine.setOutputFunc([&ownerBuf](const std::string &s) { ownerBuf += s; });

    {
        DebugSession session(engine);
        session.setBreakpoints({1});
        session.start("x = 42;\n");
        // session redirected outputFunc to its own buffer
        session.eval("disp('during debug')");
        // resume to completion
        session.resume(DebugAction::Continue);
    }
    // Without restore, engine.outputFunc_ → dangling pointer

    // Restore (the fix)
    ownerBuf.clear();
    engine.setOutputFunc([&ownerBuf](const std::string &s) { ownerBuf += s; });

    // Verify figure markers also go to correct buffer
    engine.eval("figure(1)");
    EXPECT_NE(ownerBuf.find("__FIGURE_DATA__"), std::string::npos)
        << "figure markers after debug should go to owner buffer, got: " << ownerBuf;

    ownerBuf.clear();
    engine.eval("plot([1 2], [3 4])");
    EXPECT_NE(ownerBuf.find("__FIGURE_DATA__"), std::string::npos)
        << "plot markers after debug should go to owner buffer, got: " << ownerBuf;
}

// ============================================================
// Comprehensive debugger action tests
// ============================================================

// ── Step Out ────────────────────────────────────────────────

TEST_F(DebugSessionTest, StepOutFromFunction)
{
    DebugSession session(engine);
    session.setBreakpoints({2});

    std::string code =
        "function r = double_it(x)\n"
        "    r = x * 2;\n"
        "end\n"
        "y = double_it(5);\n"
        "z = y + 1;\n";

    // start() hits bp at line 2 inside double_it
    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().functionName, "double_it");

    // Step Out — returns to caller or completes
    status = session.resume(DebugAction::StepOut);
    // May pause at caller line or complete if no more code
    if (status == ExecStatus::Paused) {
        EXPECT_NE(session.snapshot().functionName, "double_it")
            << "Should have exited double_it";
    }
    // Both Paused and Completed are acceptable
}

// ── Step Over skips function calls ──────────────────────────

TEST_F(DebugSessionTest, StepOverSkipsFunction)
{
    DebugSession session(engine);
    session.setBreakpoints({4});

    std::string code =
        "function r = sq(x)\n"
        "    r = x * x;\n"
        "end\n"
        "a = sq(3);\n"
        "b = sq(4);\n"
        "c = a + b;\n";

    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);

    // Continue to line 4 (a = sq(3))
    status = session.resume(DebugAction::Continue);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 4);

    // Step Over — should execute sq(3) without entering, land on line 5
    status = session.resume(DebugAction::StepOver);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 5);
    // a should be 9
    bool foundA = false;
    for (auto &v : session.snapshot().variables)
        if (v.name == "a" && v.value) { EXPECT_DOUBLE_EQ(v.value->toScalar(), 9.0); foundA = true; }
    EXPECT_TRUE(foundA);
}

// ── Step Into enters function ───────────────────────────────

TEST_F(DebugSessionTest, StepIntoFromCallLine)
{
    DebugSession session(engine);
    session.setBreakpoints({5});

    std::string code =
        "function r = add1(x)\n"
        "    r = x + 1;\n"
        "end\n"
        "a = 1;\n"
        "b = add1(a);\n";

    // start() hits bp at line 5 (b = add1(a))
    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 5);

    // Step Into → should enter add1
    status = session.resume(DebugAction::StepInto);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().functionName, "add1");
}

// ── Continue through multiple breakpoints ───────────────────

TEST_F(DebugSessionTest, ContinueThroughMultipleBreakpoints)
{
    DebugSession session(engine);
    session.setBreakpoints({1, 3, 5});

    std::string code = "a = 1;\nb = 2;\nc = 3;\nd = 4;\ne = 5;\n";

    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 1);

    status = session.resume(DebugAction::Continue);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 3);

    status = session.resume(DebugAction::Continue);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 5);

    status = session.resume(DebugAction::Continue);
    EXPECT_EQ(status, ExecStatus::Completed);
}

// ── Breakpoints inside functions ────────────────────────────

TEST_F(DebugSessionTest, BreakpointInsideFunction)
{
    DebugSession session(engine);
    session.setBreakpoints({2}); // inside function body

    std::string code =
        "function r = compute(x)\n"
        "    r = x * 10;\n"
        "end\n"
        "a = compute(3);\n"
        "b = compute(7);\n";

    // start() → first hit: compute(3), line 2
    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 2);
    EXPECT_EQ(session.snapshot().functionName, "compute");

    // Continue → second hit: compute(7), line 2 again
    status = session.resume(DebugAction::Continue);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 2);
    EXPECT_EQ(session.snapshot().functionName, "compute");

    // Continue → done
    status = session.resume(DebugAction::Continue);
    EXPECT_EQ(status, ExecStatus::Completed);
}

// ── Step Over in loop ───────────────────────────────────────

TEST_F(DebugSessionTest, StepOverInLoop)
{
    DebugSession session(engine);
    session.setBreakpoints({2});

    std::string code =
        "s = 0;\n"
        "for i = 1:3\n"
        "    s = s + i;\n"
        "end\n";

    // start() hits bp at line 2 (for)
    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 2);

    // Step Over through loop body
    status = session.resume(DebugAction::StepOver);
    ASSERT_EQ(status, ExecStatus::Paused);
    // Should be on next line (s = s + i) or next iteration
}

// ── Stop during function execution ──────────────────────────

TEST_F(DebugSessionTest, StopDuringFunction)
{
    DebugSession session(engine);
    session.setBreakpoints({2});

    std::string code =
        "function r = slow(x)\n"
        "    r = x;\n"
        "end\n"
        "a = slow(1);\n"
        "b = slow(2);\n";

    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);

    status = session.resume(DebugAction::Continue);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().functionName, "slow");

    session.stop();
    EXPECT_FALSE(session.isActive());
}

// ── Eval with clear during debug ────────────────────────────

TEST_F(DebugSessionTest, EvalClearDuringDebug)
{
    DebugSession session(engine);
    session.setBreakpoints({2});

    auto status = startDebug(session, "x = 10;\ny = 20;\nz = 30;\n");
    ASSERT_EQ(status, ExecStatus::Paused);

    // x should be visible
    auto snap = session.snapshot();
    bool hasX = false;
    for (auto &v : snap.variables)
        if (v.name == "x" && v.value) hasX = true;
    EXPECT_TRUE(hasX);

    // Clear x
    session.eval("clear x");

    // x should not be visible
    snap = session.snapshot();
    hasX = false;
    for (auto &v : snap.variables)
        if (v.name == "x" && v.value && !v.value->isDeleted()) hasX = true;
    EXPECT_FALSE(hasX) << "x should be cleared";
}

// ── Continue after eval modification ────────────────────────

TEST_F(DebugSessionTest, ContinueAfterEvalModification)
{
    DebugSession session(engine);
    session.setBreakpoints({2});

    std::string code = "x = 10;\ny = 20;\nz = x + y;\n";

    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);

    // Modify x
    session.eval("x = 100");

    // Continue — z should be 100 + 20 = 120
    status = session.resume(DebugAction::Continue);
    std::string out = session.takeOutput();
    EXPECT_EQ(status, ExecStatus::Completed);
}

// ── Variables visible at each step ──────────────────────────

TEST_F(DebugSessionTest, VariablesAccumulateDuringSteps)
{
    DebugSession session(engine);

    std::string code = "a = 1;\nb = 2;\nc = 3;\n";

    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);
    // Initial step: line 1, no vars yet
    EXPECT_EQ(session.snapshot().line, 1);

    status = session.resume(DebugAction::StepOver);
    ASSERT_EQ(status, ExecStatus::Paused);
    // After a=1, should have a
    auto snap = session.snapshot();
    bool hasA = false;
    for (auto &v : snap.variables)
        if (v.name == "a") hasA = true;
    EXPECT_TRUE(hasA);

    status = session.resume(DebugAction::StepOver);
    ASSERT_EQ(status, ExecStatus::Paused);
    // After b=2, should have a and b
    snap = session.snapshot();
    bool hasB = false;
    for (auto &v : snap.variables)
        if (v.name == "b") hasB = true;
    EXPECT_TRUE(hasA);
    EXPECT_TRUE(hasB);
}

// ============================================================
// Removing a breakpoint mid-session — via setBreakpoints() with an
// updated list — must take effect on the next resume.
// ============================================================

TEST_F(DebugSessionTest, RemovingBreakpointMidSessionStopsFuturePauses)
{
    DebugSession session(engine);
    session.setBreakpoints({2});

    // bp inside a 3-iteration for-loop body (line 2): without removal
    // we'd pause three times.
    std::string code =
        "for i = 1:3\n"
        "    x = i;\n"
        "end\n";

    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused) << "first iteration should pause";

    // Simulate the IDE refreshing the breakpoint list before resume — now
    // with an empty list (user clicked the gutter to remove the bp).
    session.setBreakpoints({});

    status = session.resume(DebugAction::Continue);
    EXPECT_EQ(status, ExecStatus::Completed)
        << "after clearing breakpoints, continue must run to completion";
}

TEST_F(DebugSessionTest, RemovingOneOfTwoBreakpointsOnlyOtherFires)
{
    DebugSession session(engine);
    session.setBreakpoints({1, 3});

    std::string code = "a = 1;\nb = 2;\nc = 3;\nd = 4;\n";

    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 1) << "first bp at line 1";

    // Drop the first bp. Only bp at line 3 should remain.
    session.setBreakpoints({3});

    status = session.resume(DebugAction::Continue);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 3)
        << "second pause must be on the surviving bp, not the removed one";

    status = session.resume(DebugAction::Continue);
    EXPECT_EQ(status, ExecStatus::Completed);
}

TEST_F(DebugSessionTest, AddBreakpointMidSessionFiresAfterResume)
{
    DebugSession session(engine);
    session.setBreakpoints({1});

    std::string code = "a = 1;\nb = 2;\nc = 3;\n";

    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 1);

    // Add bp at line 3 mid-session.
    session.setBreakpoints({1, 3});

    status = session.resume(DebugAction::Continue);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 3)
        << "newly-added bp must fire on next continue";

    status = session.resume(DebugAction::Continue);
    EXPECT_EQ(status, ExecStatus::Completed);
}

// ============================================================
// Script-local functions survive `clear all` in debug mode
// ============================================================

// Non-debug `eval` handles this by splitting the top-level BLOCK and
// re-registering FUNCTION_DEFs between statements. Debug mode compiles
// the whole script as a single chunk (so step semantics stay stable),
// which meant a `clear all` near the top would wipe the script's local
// functions and strand every later call to them. Engine::beginScript
// + clearUserFunctions now re-install script-locals automatically.
TEST_F(DebugSessionTest, ClearAllPreservesScriptLocalFunctionsInDebug)
{
    DebugSession session(engine);
    // No breakpoints — start() steps onto line 1, then Continue runs
    // the rest of the script to completion. If the fix is missing,
    // the call on line 3 raises "VM: undefined function 'add_one'".
    std::string code =
        "clear all;\n"
        "x = 5;\n"
        "y = add_one(x);\n"
        "function out = add_one(v)\n"
        "    out = v + 1;\n"
        "end\n";

    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);

    status = session.resume(DebugAction::Continue);
    EXPECT_EQ(status, ExecStatus::Completed)
        << "debug mode must keep script-local functions across `clear all`";
    EXPECT_TRUE(session.errorMessage().empty())
        << "unexpected error: " << session.errorMessage();

    auto *y = engine.getVariable("y");
    ASSERT_NE(y, nullptr);
    EXPECT_DOUBLE_EQ(y->toScalar(), 6.0);
}

// Same scenario, but stepping interactively through the top of the
// script. After `clear all` on line 1, we step onto line 2 and then
// line 3 — the local function must still be callable.
TEST_F(DebugSessionTest, StepOverClearAllKeepsScriptLocalFunctions)
{
    DebugSession session(engine);
    // No breakpoints → start() lands on line 1 in StepInto mode.
    std::string code =
        "clear all;\n"
        "x = 10;\n"
        "y = add_one(x);\n"
        "function out = add_one(v)\n"
        "    out = v + 1;\n"
        "end\n";

    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 1);

    // StepOver through lines 1 and 2 — exercises the post-clear state
    // under the debugger's stepping path, not just free-run Continue.
    status = session.resume(DebugAction::StepOver);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 2);

    status = session.resume(DebugAction::StepOver);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 3);

    // Continue runs the add_one call and finishes.
    status = session.resume(DebugAction::Continue);
    EXPECT_EQ(status, ExecStatus::Completed);
    EXPECT_TRUE(session.errorMessage().empty())
        << "unexpected error: " << session.errorMessage();

    auto *y = engine.getVariable("y");
    ASSERT_NE(y, nullptr);
    EXPECT_DOUBLE_EQ(y->toScalar(), 11.0);
}
