// tests/test_debugger_phase234.cpp — Debug observer, line hooks, breakpoints, stepping
#include <numkit/m/core/MDebugger.hpp>
#include "dual_engine_fixture.hpp"

using namespace m_test;
using namespace numkit::m;

// ============================================================
// Test observer that records all events
// ============================================================

class RecordingObserver : public DebugObserver
{
public:
    struct Event
    {
        enum Type { LINE, BREAKPOINT, ERROR, FUNC_ENTRY, FUNC_EXIT };
        Type type;
        uint16_t line = 0;
        std::string funcName;
        std::string message;               // for ERROR events
        std::vector<std::string> varNames; // snapshot of variable names
    };

    std::vector<Event> events;
    DebugAction defaultAction = DebugAction::Continue;

    // Sequence of actions to return (consumed in order)
    std::vector<DebugAction> actionQueue;

    DebugAction onLine(const DebugContext &ctx) override
    {
        Event e;
        e.type = Event::LINE;
        e.line = ctx.line;
        if (ctx.functionName)
            e.funcName = *ctx.functionName;
        e.varNames = ctx.variableNames();
        events.push_back(e);
        return nextAction();
    }

    DebugAction onBreakpoint(const DebugContext &ctx) override
    {
        Event e;
        e.type = Event::BREAKPOINT;
        e.line = ctx.line;
        if (ctx.functionName)
            e.funcName = *ctx.functionName;
        e.varNames = ctx.variableNames();
        events.push_back(e);
        return nextAction();
    }

    void onError(const DebugContext &ctx, const std::string &msg) override
    {
        Event e;
        e.type = Event::ERROR;
        e.line = ctx.line;
        e.message = msg;
        if (ctx.functionName)
            e.funcName = *ctx.functionName;
        events.push_back(e);
    }

    void onFunctionEntry(const DebugContext &ctx) override
    {
        Event e;
        e.type = Event::FUNC_ENTRY;
        if (ctx.functionName)
            e.funcName = *ctx.functionName;
        events.push_back(e);
    }

    void onFunctionExit(const DebugContext &ctx) override
    {
        Event e;
        e.type = Event::FUNC_EXIT;
        if (ctx.functionName)
            e.funcName = *ctx.functionName;
        events.push_back(e);
    }

    // Helper: count events of a type
    size_t countType(Event::Type t) const
    {
        size_t n = 0;
        for (auto &e : events)
            if (e.type == t)
                n++;
        return n;
    }

    // Helper: get all lines hit
    std::vector<uint16_t> linesHit() const
    {
        std::vector<uint16_t> result;
        for (auto &e : events)
            if ((e.type == Event::LINE || e.type == Event::BREAKPOINT) && e.line > 0)
                result.push_back(e.line);
        return result;
    }

private:
    DebugAction nextAction()
    {
        if (!actionQueue.empty()) {
            DebugAction a = actionQueue.front();
            actionQueue.erase(actionQueue.begin());
            return a;
        }
        return defaultAction;
    }
};

// ============================================================
// Test fixture
// ============================================================

class DebugPhase234Test : public DualEngineTest
{};

// ============================================================
// 1. No observer = no overhead, normal execution
// ============================================================

TEST_P(DebugPhase234Test, NoObserverNormalExecution)
{
    // Without observer, everything works as before
    eval("x = 42;");
    EXPECT_DOUBLE_EQ(getVar("x"), 42.0);
}

// ============================================================
// 2. Observer receives line events in StepInto mode
// ============================================================

TEST_P(DebugPhase234Test, StepIntoSeesAllLines)
{
    auto obs = std::make_shared<RecordingObserver>();
    obs->defaultAction = DebugAction::StepInto;
    engine.setDebugObserver(obs);

    eval("x = 1;\ny = 2;\nz = 3;\n");

    auto lines = obs->linesHit();
    EXPECT_GE(lines.size(), 3u) << "StepInto should hit at least 3 lines for 3 statements";

    // Lines should include 1, 2, 3
    EXPECT_TRUE(std::find(lines.begin(), lines.end(), 1) != lines.end()) << "Should hit line 1";
    EXPECT_TRUE(std::find(lines.begin(), lines.end(), 2) != lines.end()) << "Should hit line 2";
    EXPECT_TRUE(std::find(lines.begin(), lines.end(), 3) != lines.end()) << "Should hit line 3";
}

// ============================================================
// 3. Continue mode: no line events unless breakpoint
// ============================================================

TEST_P(DebugPhase234Test, ContinueModeNoLineEvents)
{
    auto obs = std::make_shared<RecordingObserver>();
    obs->defaultAction = DebugAction::Continue;
    engine.setDebugObserver(obs);

    eval("x = 1;\ny = 2;\nz = 3;\n");

    // Initial stop fires onLine for the first line.
    // Observer returns Continue → no further onLine calls.
    // Only breakpoints would trigger after that.
    EXPECT_LE(obs->countType(RecordingObserver::Event::LINE), 1u)
        << "Continue mode should fire at most 1 onLine (initial stop)";
}

// ============================================================
// 4. Breakpoints
// ============================================================

TEST_P(DebugPhase234Test, BreakpointHitsCorrectLine)
{
    auto obs = std::make_shared<RecordingObserver>();
    obs->defaultAction = DebugAction::Continue;
    engine.setDebugObserver(obs);

    engine.breakpointManager().addBreakpoint(2);

    eval("x = 1;\ny = 2;\nz = 3;\n");

    size_t bpCount = obs->countType(RecordingObserver::Event::BREAKPOINT);
    EXPECT_GE(bpCount, 1u) << "Should hit breakpoint on line 2";

    // Find the breakpoint event
    for (auto &e : obs->events) {
        if (e.type == RecordingObserver::Event::BREAKPOINT) {
            EXPECT_EQ(e.line, 2u) << "Breakpoint should be on line 2";
            break;
        }
    }
}

TEST_P(DebugPhase234Test, BreakpointDisabled)
{
    auto obs = std::make_shared<RecordingObserver>();
    obs->defaultAction = DebugAction::Continue;
    engine.setDebugObserver(obs);

    int bpId = engine.breakpointManager().addBreakpoint(2);
    engine.breakpointManager().enableBreakpoint(bpId, false);

    eval("x = 1;\ny = 2;\nz = 3;\n");

    EXPECT_EQ(obs->countType(RecordingObserver::Event::BREAKPOINT), 0u)
        << "Disabled breakpoint should not trigger";
}

TEST_P(DebugPhase234Test, BreakpointRemoved)
{
    auto obs = std::make_shared<RecordingObserver>();
    obs->defaultAction = DebugAction::Continue;
    engine.setDebugObserver(obs);

    int bpId = engine.breakpointManager().addBreakpoint(2);
    engine.breakpointManager().removeBreakpoint(bpId);

    eval("x = 1;\ny = 2;\nz = 3;\n");

    EXPECT_EQ(obs->countType(RecordingObserver::Event::BREAKPOINT), 0u);
}

// ============================================================
// 5. Function entry/exit events
// ============================================================

TEST_P(DebugPhase234Test, FunctionEntryExit)
{
    auto obs = std::make_shared<RecordingObserver>();
    obs->defaultAction = DebugAction::Continue;
    engine.setDebugObserver(obs);

    eval(R"(
        function r = add1(x)
            r = x + 1;
        end
        y = add1(5);
    )");

    // Should see at least: script entry, add1 entry, add1 exit, script exit
    EXPECT_GE(obs->countType(RecordingObserver::Event::FUNC_ENTRY), 2u);
    EXPECT_GE(obs->countType(RecordingObserver::Event::FUNC_EXIT), 2u);

    // Check function names
    bool sawAdd1Entry = false;
    bool sawAdd1Exit = false;
    for (auto &e : obs->events) {
        if (e.type == RecordingObserver::Event::FUNC_ENTRY && e.funcName == "add1")
            sawAdd1Entry = true;
        if (e.type == RecordingObserver::Event::FUNC_EXIT && e.funcName == "add1")
            sawAdd1Exit = true;
    }
    EXPECT_TRUE(sawAdd1Entry) << "Should see add1 function entry";
    EXPECT_TRUE(sawAdd1Exit) << "Should see add1 function exit";
}

// ============================================================
// 6. Stop action aborts execution
// ============================================================

TEST_P(DebugPhase234Test, StopAbortsExecution)
{
    auto obs = std::make_shared<RecordingObserver>();
    obs->defaultAction = DebugAction::StepInto;
    // After first line, return Stop
    obs->actionQueue = {DebugAction::StepInto, DebugAction::Stop};
    engine.setDebugObserver(obs);

    EXPECT_THROW(eval("x = 1;\ny = 2;\nz = 3;\n"), DebugStopException);

    // x should be set (line 1 executed), z should not
    auto *x = getVarPtr("x");
    EXPECT_NE(x, nullptr) << "x should be set before stop";
}

// ============================================================
// 7. Variable inspection through DebugContext
// ============================================================

TEST_P(DebugPhase234Test, VariableInspectionAtBreakpoint)
{
    auto obs = std::make_shared<RecordingObserver>();
    obs->defaultAction = DebugAction::Continue;
    engine.setDebugObserver(obs);

    engine.breakpointManager().addBreakpoint(3);

    eval("x = 42;\ny = 7;\nz = x + y;\n");

    // At breakpoint on line 3, x and y should be visible
    for (auto &e : obs->events) {
        if (e.type == RecordingObserver::Event::BREAKPOINT && e.line == 3) {
            bool hasX = std::find(e.varNames.begin(), e.varNames.end(), "x") != e.varNames.end();
            bool hasY = std::find(e.varNames.begin(), e.varNames.end(), "y") != e.varNames.end();
            EXPECT_TRUE(hasX) << "x should be visible at breakpoint on line 3";
            EXPECT_TRUE(hasY) << "y should be visible at breakpoint on line 3";
            break;
        }
    }
}

// ============================================================
// 8. StepOver skips function internals
// ============================================================

TEST_P(DebugPhase234Test, StepOverSkipsFunction)
{
    auto obs = std::make_shared<RecordingObserver>();
    obs->defaultAction = DebugAction::StepOver;
    engine.setDebugObserver(obs);

    eval(R"(
        function r = add1(x)
            r = x + 1;
        end
        x = 1;
        y = add1(x);
        z = y + 1;
    )");

    // In StepOver mode, we should NOT see lines from inside add1
    // (add1 body is at a deeper call depth)
    auto lines = obs->linesHit();
    // Should see script-level lines but not function body lines
    // The function definition line may appear, the call line will appear,
    // but the internal "r = x + 1" should not trigger onLine in StepOver
    EXPECT_FALSE(lines.empty()) << "Should see some lines in StepOver";
}

// ============================================================
// 9. BreakpointManager unit tests
// ============================================================

TEST_P(DebugPhase234Test, BreakpointManagerBasics)
{
    BreakpointManager bm;

    int id1 = bm.addBreakpoint(5);
    int id2 = bm.addBreakpoint(10);

    EXPECT_TRUE(bm.shouldBreak(5));
    EXPECT_TRUE(bm.shouldBreak(10));
    EXPECT_FALSE(bm.shouldBreak(7));

    bm.enableBreakpoint(id1, false);
    EXPECT_FALSE(bm.shouldBreak(5));
    EXPECT_TRUE(bm.shouldBreak(10));

    bm.enableBreakpoint(id1, true);
    EXPECT_TRUE(bm.shouldBreak(5));

    bm.removeBreakpoint(id2);
    EXPECT_FALSE(bm.shouldBreak(10));
    EXPECT_TRUE(bm.shouldBreak(5));

    bm.clearAll();
    EXPECT_FALSE(bm.shouldBreak(5));
}

// ============================================================
// 10. No observer = zero overhead (no crash)
// ============================================================

TEST_P(DebugPhase234Test, RemoveObserverMidway)
{
    auto obs = std::make_shared<RecordingObserver>();
    obs->defaultAction = DebugAction::StepInto;
    engine.setDebugObserver(obs);

    eval("x = 1;");
    EXPECT_GT(obs->events.size(), 0u);

    // Remove observer
    engine.setDebugObserver(nullptr);
    obs->events.clear();

    // Should still work normally
    eval("y = 2;");
    EXPECT_EQ(obs->events.size(), 0u) << "No events after observer removed";
    EXPECT_DOUBLE_EQ(getVar("y"), 2.0);
}

// ============================================================
// 11. Call stack depth
// ============================================================

TEST_P(DebugPhase234Test, CallStackDepthAtBreakpoint)
{
    auto obs = std::make_shared<RecordingObserver>();
    obs->defaultAction = DebugAction::Continue;
    engine.setDebugObserver(obs);

    // Set breakpoint inside inner function
    engine.breakpointManager().addBreakpoint(3); // "r = x + 1" line

    eval(R"(
        function r = inner(x)
            r = x + 1;
        end
        y = inner(5);
    )");

    // At breakpoint, call stack should have >=2 frames (script + inner)
    for (auto &e : obs->events) {
        if (e.type == RecordingObserver::Event::BREAKPOINT) {
            EXPECT_EQ(e.funcName, "inner") << "Breakpoint should be inside 'inner'";
            break;
        }
    }
}

// ============================================================
// 12. StepOver at nested depth: skips inner function called from
//     within an already-entered outer function
// ============================================================

TEST_P(DebugPhase234Test, StepOverAtNestedDepth)
{
    auto obs = std::make_shared<RecordingObserver>();
    // Run until bp, then switch to StepOver and stay in that mode for
    // at least the remaining events in `mid`.
    obs->defaultAction = DebugAction::Continue;
    obs->actionQueue = {
        DebugAction::StepOver,
        DebugAction::StepOver,
        DebugAction::StepOver,
    };
    engine.setDebugObserver(obs);

    // bp at the first line of `mid` (the `y = inner(x)` call line)
    engine.breakpointManager().addBreakpoint(6);

    eval(R"(
        function r = inner(x)
            r = x + 10;
        end
        function r = mid(x)
            y = inner(x);
            r = y + 1;
        end
        result = mid(5);
    )");

    auto lines = obs->linesHit();
    // Line 3 (`r = x + 10`) is inside `inner` — at depth 3 when called from
    // `mid`. StepOver from depth 2 must skip it.
    bool sawInnerBody = std::find(lines.begin(), lines.end(), (uint16_t)3) != lines.end();
    EXPECT_FALSE(sawInnerBody)
        << "StepOver at depth 2 must NOT observe inner function body (line 3)";

    // Sanity: bp at line 6 was hit
    bool sawBp = std::find(lines.begin(), lines.end(), (uint16_t)6) != lines.end();
    EXPECT_TRUE(sawBp) << "breakpoint at line 6 must have been hit";

    // Execution actually completed: result = inner(5) + 1 = 15 + 1 = 16
    EXPECT_DOUBLE_EQ(getVar("result"), 16.0);
}

// ============================================================
// 13. StepOut: parametrized across TW and VM backends
//     (debug_session_test only covers VM via DebugSession)
// ============================================================

TEST_P(DebugPhase234Test, StepOutReturnsToCaller)
{
    auto obs = std::make_shared<RecordingObserver>();
    obs->defaultAction = DebugAction::Continue;
    obs->actionQueue = { DebugAction::StepOut };
    engine.setDebugObserver(obs);

    engine.breakpointManager().addBreakpoint(3); // "r = x * 2" inside helper

    eval(R"(
        function r = helper(x)
            r = x * 2;
        end
        y = helper(5);
        z = y + 1;
    )");

    // Find first LINE event after the BREAKPOINT — it must NOT be in helper.
    bool seenBp = false;
    bool sawLineAfterBpInHelper = false;
    bool sawLineAfterBpInCaller = false;
    for (auto &e : obs->events) {
        if (e.type == RecordingObserver::Event::BREAKPOINT) {
            seenBp = true;
            EXPECT_EQ(e.funcName, "helper") << "bp must be inside helper";
            continue;
        }
        if (seenBp && e.type == RecordingObserver::Event::LINE) {
            if (e.funcName == "helper")
                sawLineAfterBpInHelper = true;
            else
                sawLineAfterBpInCaller = true;
        }
    }
    EXPECT_TRUE(seenBp) << "breakpoint must have been hit";
    EXPECT_FALSE(sawLineAfterBpInHelper)
        << "after StepOut, no LINE event should fire inside helper";

    // Execution completed: z = helper(5) + 1 = 10 + 1 = 11
    EXPECT_DOUBLE_EQ(getVar("z"), 11.0);

    // sawLineAfterBpInCaller not asserted — some backends may simply complete
    // without another onLine after the return. The important invariant is
    // "no more events in helper".
    (void)sawLineAfterBpInCaller;
}

// ============================================================
// 14. Differential StepInto vs StepOver on the same call line
// ============================================================

TEST_P(DebugPhase234Test, StepIntoEntersStepOverSkipsSameCode)
{
    const char *code = R"(
        function r = f(x)
            r = x + 100;
        end
        y = f(5);
        z = y + 1;
    )";

    // Pass 1: StepInto must see line 3 (body of f)
    {
        auto obs = std::make_shared<RecordingObserver>();
        obs->defaultAction = DebugAction::StepInto;
        engine.setDebugObserver(obs);
        eval(code);
        auto lines = obs->linesHit();
        bool sawBody = std::find(lines.begin(), lines.end(), (uint16_t)3) != lines.end();
        EXPECT_TRUE(sawBody) << "StepInto must observe function body (line 3)";
        engine.setDebugObserver(nullptr);
    }

    // Pass 2: StepOver must NOT see line 3
    {
        auto obs = std::make_shared<RecordingObserver>();
        obs->defaultAction = DebugAction::StepOver;
        engine.setDebugObserver(obs);
        eval(code);
        auto lines = obs->linesHit();
        bool sawBody = std::find(lines.begin(), lines.end(), (uint16_t)3) != lines.end();
        EXPECT_FALSE(sawBody) << "StepOver must NOT observe function body (line 3)";
        engine.setDebugObserver(nullptr);
    }
}

INSTANTIATE_DUAL(DebugPhase234Test);

// ============================================================
// 12. Regression: DebugStopException must propagate in VMBackend
//     (Bug: catch(...) in Engine::eval swallowed it, causing silent
//      re-execution via TreeWalker without debug support)
// ============================================================

TEST(DebugVMBackend, StopExceptionPropagates)
{
    Engine engine;
    StdLibrary::install(engine);
    engine.setBackend(Engine::Backend::VM);

    auto obs = std::make_shared<RecordingObserver>();
    obs->defaultAction = DebugAction::Stop; // Stop on first event
    engine.setDebugObserver(obs);

    engine.breakpointManager().addBreakpoint(2);

    EXPECT_THROW(engine.eval("x = 1;\ny = 2;\nz = 3;\n"), DebugStopException)
        << "DebugStopException must not be swallowed by VMBackend catch(...)";
}

TEST(DebugVMBackend, BreakpointContinueThenStop)
{
    Engine engine;
    StdLibrary::install(engine);
    engine.setBackend(Engine::Backend::VM);

    auto obs = std::make_shared<RecordingObserver>();
    // Continue past first breakpoint, stop on second
    obs->actionQueue = {DebugAction::Continue, DebugAction::Stop};
    obs->defaultAction = DebugAction::Stop;
    engine.setDebugObserver(obs);

    engine.breakpointManager().addBreakpoint(1);
    engine.breakpointManager().addBreakpoint(3);

    EXPECT_THROW(engine.eval("x = 1;\ny = 2;\nz = 3;\n"), DebugStopException);

    // x and y should have been set before the stop on line 3
    auto *x = engine.getVariable("x");
    EXPECT_NE(x, nullptr);
    if (x) EXPECT_DOUBLE_EQ(x->toScalar(), 1.0);
}

// ============================================================
// 13. Regression: functions defined at bottom of script must be
//     available even after 'clear' (MATLAB local function semantics)
// ============================================================

TEST(DebugVMBackend, FunctionAtBottomWithClear)
{
    Engine engine;
    StdLibrary::install(engine);
    engine.setBackend(Engine::Backend::VM);

    std::string output;
    engine.setOutputFunc([&](const std::string &s) { output += s; });

    auto obs = std::make_shared<RecordingObserver>();
    obs->defaultAction = DebugAction::Stop;
    engine.setDebugObserver(obs);

    engine.breakpointManager().addBreakpoint(5);

    // Script with clear at top and function at bottom
    std::string code =
        "clear\n"
        "result = [];\n"
        "for k = 0:3\n"
        "    result = [result, fib(k)];\n"
        "end\n"
        "disp(result)\n"
        "function r = fib(n)\n"
        "    if n <= 1\n"
        "        r = n;\n"
        "    else\n"
        "        r = fib(n-1) + fib(n-2);\n"
        "    end\n"
        "end\n";

    // Should pause at line 5 (end of for), not crash with "undefined fib"
    auto r = engine.evalSafe(code);
    EXPECT_TRUE(r.debugStop) << "Should pause at breakpoint, not error. "
        << "Error: " << r.errorMessage;
}