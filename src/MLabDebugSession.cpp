// src/MLabDebugSession.cpp
#include "MLabDebugSession.hpp"
#include "MLabCompiler.hpp"
#include "MLabEngine.hpp"
#include "MLabLexer.hpp"
#include "MLabParser.hpp"

#include <unordered_set>

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
            engine_.syncVMToWorkspace();
            active_ = false;
            engine_.setDebugObserver(nullptr);
        }
        // If Paused: VM state preserved in frames_ for resume

        return status;
    } catch (const MLabError &e) {
        engine_.syncVMToWorkspace();
        errorMsg_ = e.what();
        errorLine_ = e.line();
        active_ = false;
        engine_.setDebugObserver(nullptr);
        return ExecStatus::Completed;
    } catch (const std::exception &e) {
        engine_.syncVMToWorkspace();
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

    // Clear frame variable overrides from evalVars_ — after resume,
    // the VM registers have the authoritative values.
    // Keep only eval-created variables that have no register in the frame.
    {
        auto *ctl = engine_.debugController();
        if (ctl && !ctl->callStack().empty()) {
            auto &frame = ctl->callStack().back();
            if (frame.chunk) {
                for (auto &[vname, reg] : frame.chunk->varMap)
                    evalVars_.erase(vname);
            }
        }
    }

    // Attach dynVars to VM frame so ASSERT_DEF can find eval-created variables
    engine_.vm_->setFrameDynVars(evalVars_.empty() ? nullptr : &evalVars_);

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

    // Variables from current frame + eval-created variables
    auto vars = frame.variables();
    snap.variables.reserve(vars.size() + evalVars_.size());
    std::unordered_set<std::string> seen;
    for (auto &v : vars) {
        snap.variables.push_back({v.name, v.value});
        seen.insert(v.name);
    }

    // Merge in eval-created variables (not in frame, not cleared)
    for (auto &[name, val] : evalVars_) {
        if (seen.count(name) == 0 && !val.isEmpty() && !val.isUnset() && !val.isDeleted())
            snap.variables.push_back({name, &val});
    }

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

    // 4. Inject frame variables + evalVars into workspace for eval
    auto &genv = engine_.workspaceEnv();
    auto &gstore = engine_.globalsEnv();

    // Save workspace state to restore after eval
    struct SavedVar { std::string name; bool hadEnv; MValue envVal; bool hadStore; MValue storeVal; };
    std::vector<SavedVar> savedVars;

    auto injectVar = [&](const std::string &name, const MValue &val) {
        SavedVar sv;
        sv.name = name;
        auto *ge = genv.get(name);
        sv.hadEnv = ge && !ge->isEmpty();
        if (sv.hadEnv) sv.envVal = *ge;
        auto *gs = gstore.get(name);
        sv.hadStore = gs && !gs->isEmpty();
        if (sv.hadStore) sv.storeVal = *gs;
        genv.set(name, val);
        gstore.set(name, val);
        savedVars.push_back(std::move(sv));
    };

    // Inject VM frame variables
    for (auto &v : snap.variables)
        if (v.value) injectVar(v.name, *v.value);

    // Inject previously eval-created variables
    for (auto &[name, val] : evalVars_)
        injectVar(name, val);

    // 5. Execute expression
    std::string evalOutput;
    engine_.setOutputFunc([&evalOutput](const std::string &s) { evalOutput += s; });
    try {
        engine_.eval(code);
    } catch (const std::exception &e) {
        evalOutput = std::string("Error: ") + e.what();
    }

    // 6. Capture workspace changes into evalVars_
    {
        // Build set of names we injected before eval
        std::unordered_set<std::string> injectedNames;
        for (auto &sv : savedVars)
            injectedNames.insert(sv.name);

        // Detect cleared variables: if a variable was injected but is now
        // missing or empty in workspace, it was cleared by the eval.
        for (auto &name : injectedNames) {
            auto *val = genv.getLocal(name);
            if (!val || val->isEmpty() || val->isUnset()) {
                evalVars_[name] = MValue::deleted();
                engine_.vm_->setFrameVariable(name, MValue::deleted());
            }
        }

        // Capture new/modified variables
        auto wsNames = genv.localNames();
        for (auto &name : wsNames) {
            if (name == "ans" || name == "nargin" || name == "nargout") continue;
            auto *val = genv.getLocal(name);
            if (!val || val->isEmpty()) continue;
            evalVars_[name] = *val;
        }
        // Also capture from VM's lastVarMap (eval script's exported vars)
        for (auto &[name, val] : engine_.vm_->lastVarMap()) {
            if (name == "ans") continue;
            if (val.isDeleted())
                evalVars_[name] = val; // propagate clear
            else if (!val.isEmpty() && !val.isUnset())
                evalVars_[name] = val;
        }
    }

    // 7. Restore VM state and debug controller
    engine_.setOutputFunc([this](const std::string &s) { outputBuf_ += s; });
    engine_.setDebugObserver(observer_);
    engine_.vm_->restorePausedState(std::move(savedState));

    ctl = engine_.debugController();
    if (ctl) {
        ctl->callStack() = std::move(savedCallStack);
        ctl->setLastLine(snap.line);
    }

    // 8. Write back modified frame variables directly into VM registers
    for (auto &[name, val] : evalVars_)
        engine_.vm_->setFrameVariable(name, val);

    // 9. Restore workspace to pre-eval state
    for (auto &sv : savedVars) {
        if (sv.hadEnv)  genv.set(sv.name, sv.envVal);
        else            genv.remove(sv.name);
        if (sv.hadStore) gstore.set(sv.name, sv.storeVal);
        else             gstore.remove(sv.name);
    }
    // Clean up eval-created variables from workspace (they live in evalVars_)
    for (auto &[name, val] : evalVars_) {
        bool wasRestored = false;
        for (auto &sv : savedVars)
            if (sv.name == name) { wasRestored = true; break; }
        if (!wasRestored)
            genv.remove(name);
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
