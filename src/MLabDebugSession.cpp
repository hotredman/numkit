// src/MLabDebugSession.cpp
#include "MLabDebugSession.hpp"
#include "MLabCompiler.hpp"
#include "MLabEngine.hpp"
#include "MLabLexer.hpp"
#include "MLabParser.hpp"

namespace mlab {

DebugSession::DebugSession(Engine &engine)
    : engine_(engine)
    , observer_(std::make_shared<SessionObserver>())
{
}

DebugSession::~DebugSession()
{
    stop();
}

void DebugSession::setBreakpoints(const std::vector<uint16_t> &lines)
{
    auto &bpm = engine_.breakpointManager();
    bpm.clearAll();
    for (auto line : lines)
        if (line > 0)
            bpm.addBreakpoint(line);
}

ExecStatus DebugSession::start(const std::string &code)
{
    stop(); // clean up any previous session

    errorMsg_.clear();
    errorLine_ = 0;
    outputBuf_.clear();

    // Capture output
    engine_.setOutputFunc([this](const std::string &s) { outputBuf_ += s; });

    // Attach debug observer
    engine_.setDebugObserver(observer_);

    active_ = true;

    try {
        // Compile (same as Engine::eval)
        Lexer lexer(code);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto ast = parser.parse();

        engine_.clearAllCalled_ = false;
        engine_.vm_->clearLastVarMap();

        auto src = std::make_shared<const std::string>(code);
        auto *compiler = engine_.compilerPtr();
        chunk_ = compiler->compile(ast.get(), src);
        engine_.vm_->setCompiledFuncs(&compiler->compiledFuncs());

        // Start execution via VM's debug-aware API (no cleanup on pause)
        ExecStatus status = engine_.vm_->startExecution(chunk_);

        if (status == ExecStatus::Completed) {
            engine_.syncVMToGlobalEnv();
            active_ = false;
            engine_.setDebugObserver(nullptr);
        }
        // If Paused: VM state preserved in frames_ for resume

        return status;
    } catch (const MLabError &e) {
        engine_.syncVMToGlobalEnv();
        errorMsg_ = e.what();
        errorLine_ = e.line();
        active_ = false;
        engine_.setDebugObserver(nullptr);
        return ExecStatus::Completed;
    } catch (const std::exception &e) {
        engine_.syncVMToGlobalEnv();
        errorMsg_ = e.what();
        active_ = false;
        engine_.setDebugObserver(nullptr);
        return ExecStatus::Completed;
    }
}

ExecStatus DebugSession::resume(DebugAction action)
{
    if (!active_)
        return ExecStatus::Completed;

    outputBuf_.clear();

    try {
        ExecStatus status = engine_.debugResume(action);

        if (status == ExecStatus::Completed) {
            active_ = false;
            engine_.setDebugObserver(nullptr);
        }

        return status;
    } catch (const MLabError &e) {
        errorMsg_ = e.what();
        errorLine_ = e.line();
        active_ = false;
        engine_.setDebugObserver(nullptr);
        return ExecStatus::Completed;
    } catch (const std::exception &e) {
        errorMsg_ = e.what();
        active_ = false;
        engine_.setDebugObserver(nullptr);
        return ExecStatus::Completed;
    }
}

void DebugSession::stop()
{
    if (!active_)
        return;

    active_ = false;
    engine_.setDebugObserver(nullptr);
    // Clean up VM paused state
    // The VM's frames will be cleared on next execution
}

DebugSession::Snapshot DebugSession::snapshot() const
{
    Snapshot snap;

    auto *ctl = engine_.debugController();
    if (!ctl)
        return snap;

    auto &stack = ctl->callStack();
    if (stack.empty())
        return snap;

    // Current frame info
    auto &frame = stack.back();
    snap.line = frame.line;
    snap.col = frame.col;
    snap.functionName = frame.functionName;

    // Variables from current frame
    auto vars = frame.variables();
    snap.variables.reserve(vars.size());
    for (auto &v : vars)
        snap.variables.push_back({v.name, v.value});

    // Full call stack
    snap.callStack = stack;

    return snap;
}

std::string DebugSession::eval(const std::string &code)
{
    if (!active_)
        return "Error: no active debug session";

    // 1. Snapshot and save debug controller callStack
    auto snap = snapshot();
    auto *ctl = engine_.debugController();
    std::vector<StackFrame> savedCallStack;
    if (ctl)
        savedCallStack = ctl->callStack();

    // 2. Save paused VM state
    auto savedState = engine_.vm_->savePausedState();

    // 3. Detach debug observer (so eval doesn't trigger debug hooks)
    engine_.setDebugObserver(nullptr);

    // 4. Inject current frame's variables into both global stores
    //    (compiler's varRegRead checks globalStore_ and globalEnv_)
    //    Save prior state so we can undo injection after eval.
    auto &genv = engine_.globalEnvironment();
    auto &gstore = engine_.globalVarStore();

    struct SavedVar { std::string name; bool hadEnv; MValue envVal; bool hadStore; MValue storeVal; };
    std::vector<SavedVar> savedVars;

    for (auto &v : snap.variables) {
        if (v.value && v.name != "nargin" && v.name != "nargout") {
            SavedVar sv;
            sv.name = v.name;
            auto *ge = genv.get(v.name);
            sv.hadEnv = ge && !ge->isEmpty();
            if (sv.hadEnv) sv.envVal = *ge;
            auto *gs = gstore.get(v.name);
            sv.hadStore = gs && !gs->isEmpty();
            if (sv.hadStore) sv.storeVal = *gs;

            genv.set(v.name, *v.value);
            gstore.set(v.name, *v.value);
            savedVars.push_back(std::move(sv));
        }
    }

    // 5. Execute expression (this will clear frames, but we restore after)
    std::string evalOutput;
    engine_.setOutputFunc([&evalOutput](const std::string &s) { evalOutput += s; });
    try {
        engine_.eval(code);
    } catch (const std::exception &e) {
        evalOutput = std::string("Error: ") + e.what();
    }

    // 6. Restore everything
    engine_.setOutputFunc([this](const std::string &s) { outputBuf_ += s; });
    engine_.setDebugObserver(observer_);  // creates new DebugController
    engine_.vm_->restorePausedState(std::move(savedState));

    // Restore debug controller's callStack and lastLine (was destroyed by setDebugObserver)
    ctl = engine_.debugController();
    if (ctl) {
        ctl->callStack() = std::move(savedCallStack);
        ctl->setLastLine(snap.line);  // prevent re-triggering bp on the same line
    }

    // Undo variable injection to prevent contaminating the real execution
    for (auto &sv : savedVars) {
        if (sv.hadEnv)  genv.set(sv.name, sv.envVal);
        else            genv.remove(sv.name);
        if (sv.hadStore) gstore.set(sv.name, sv.storeVal);
        else             gstore.remove(sv.name);
    }

    // Trim trailing whitespace
    while (!evalOutput.empty() &&
           (evalOutput.back() == '\n' || evalOutput.back() == ' '))
        evalOutput.pop_back();

    return evalOutput;
}

std::string DebugSession::takeOutput()
{
    std::string out = std::move(outputBuf_);
    outputBuf_.clear();
    return out;
}

} // namespace mlab
