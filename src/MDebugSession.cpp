// src/MDebugSession.cpp
//
// DebugSession now routes all console-visible variable state through
// DebugWorkspace (see include/MDebugWorkspace.hpp):
//   - frame variables are updated via direct pointers into VM registers;
//   - console-only variables live in an explicit overlay map, which is
//     plugged into the VM as `dynVars` during `continue` so they resolve
//     like real variables for the running script.
//
// Semantics of console `clear` match MATLAB's K>>:
//   clear        — wipes the paused frame AND the overlay; on continue,
//                  any reference to a cleared name raises
//                  "Undefined function or variable 'X'" at that line
//                  and the session ends with an error.
//   clear x      — clears only x.
//   x = <expr>   — writes through to the frame register if x is a frame
//                  var, otherwise lands in the overlay.
//
#include "MDebugSession.hpp"
#include "MCompiler.hpp"
#include "MEngine.hpp"
#include "MLexer.hpp"
#include "MParser.hpp"

#include <unordered_set>

namespace numkit {

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
    ws_.reset();

    engine_.setOutputFunc([this](const std::string &s) { outputBuf_ += s; });
    engine_.setDebugObserver(observer_);

    active_ = true;

    try {
        Lexer lexer(code);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        ast_ = parser.parse();

        // Enter the engine's script scope: pointers into ast_ land in
        // engine.scriptLocalFuncs_ so a `clear all` from inside the
        // paused script re-installs its local functions. Matching
        // endScript is called in deactivate() on completion/error/stop.
        engine_.beginScript(ast_.get());

        engine_.vm_->clearLastVarMap();

        auto src = std::make_shared<const std::string>(code);
        auto *compiler = engine_.compilerPtr();
        chunk_ = compiler->compile(ast_.get(), src);
        engine_.vm_->setCompiledFuncs(&compiler->compiledFuncs(),
                                      &compiler->scriptLocalCompiledFuncs());

        // No breakpoints → pause on first line (StepInto) so the user can step.
        // Breakpoints set → run (Continue) until the first one is hit.
        DebugAction initial = engine_.breakpointManager().breakpoints().empty()
                                  ? DebugAction::StepInto
                                  : DebugAction::Continue;

        ExecStatus status = engine_.vm_->startExecution(chunk_, nullptr, 0, initial);

        if (status == ExecStatus::Paused) {
            ws_.bindVMFrame(*engine_.vm_, engine_);
        } else {
            engine_.syncVMToWorkspace();
            deactivate();
        }
        return status;
    } catch (const MError &e) {
        engine_.syncVMToWorkspace();
        errorMsg_ = e.what();
        errorLine_ = e.line();
        deactivate();
        return ExecStatus::Completed;
    } catch (const std::exception &e) {
        engine_.syncVMToWorkspace();
        errorMsg_ = e.what();
        deactivate();
        return ExecStatus::Completed;
    }
}

ExecStatus DebugSession::resume(DebugAction action)
{
    if (!active_)
        return ExecStatus::Completed;

    outputBuf_.clear();

    // Expose overlay to the running script so console-created names resolve
    // via ASSERT_DEF's dynVars fallback.
    engine_.vm_->setFrameDynVars(ws_.overlay().empty() ? nullptr : &ws_.overlay());

    // Frame pointers become stale as soon as execution resumes (new frames
    // may be pushed, call depth changes). Rebind on next pause.
    ws_.unbindFrame();

    try {
        ExecStatus status = engine_.debugResume(action);

        if (status == ExecStatus::Paused) {
            ws_.bindVMFrame(*engine_.vm_, engine_);
        } else {
            deactivate();
        }

        return status;
    } catch (const MError &e) {
        errorMsg_ = e.what();
        errorLine_ = e.line();
        deactivate();
        return ExecStatus::Completed;
    } catch (const std::exception &e) {
        errorMsg_ = e.what();
        deactivate();
        return ExecStatus::Completed;
    }
}

void DebugSession::stop()
{
    if (!active_)
        return;

    deactivate();
    ws_.reset();
}

void DebugSession::deactivate()
{
    if (!active_)
        return;
    active_ = false;
    engine_.setDebugObserver(nullptr);
    // Pair with the beginScript() at the start of this session.
    engine_.endScript();
    ast_.reset();
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

    auto &frame = stack.back();
    snap.line = frame.line;
    snap.col = frame.col;
    snap.functionName = frame.functionName;

    // Build variables list from the DebugWorkspace (live frame pointers +
    // overlay). Deleted/unset slots are filtered out by names().
    for (auto &name : ws_.names()) {
        auto *val = ws_.get(name);
        if (val)
            snap.variables.push_back({name, val});
    }

    snap.callStack = stack;
    return snap;
}

std::string DebugSession::eval(const std::string &code)
{
    if (!active_)
        return "Error: no active debug session";

    // 1. Save debug controller + paused VM state. The inner engine.eval()
    //    runs a full VM pass and stomps both.
    auto *ctl = engine_.debugController();
    std::vector<StackFrame> savedCallStack;
    uint16_t savedLine = 0;
    if (ctl) {
        savedCallStack = ctl->callStack();
        if (ctl->currentFrame())
            savedLine = ctl->currentFrame()->line;
    }
    auto savedVMState = engine_.vm_->savePausedState();

    // 2. Snapshot the base workspace by value. engine.eval() may clearAll or
    //    add variables to workspaceEnv; we restore it completely afterwards.
    auto &genv = engine_.workspaceEnv();
    std::unordered_map<std::string, MValue> preEvalEnv;
    for (auto &n : genv.localNames()) {
        if (auto *v = genv.getLocal(n))
            preEvalEnv.emplace(n, *v);
    }

    // 3. Detach the observer so the inner eval doesn't trigger debug hooks.
    engine_.setDebugObserver(nullptr);

    // 4. Inject current debug-workspace contents into workspaceEnv so the
    //    eval sees them like normal base-workspace variables. MValue copy
    //    is tagged-pointer + COW, so this is cheap even for large matrices.
    std::unordered_set<std::string> injectedNames;
    // Start from a clean slate so `clear` in the console doesn't race with
    // unrelated REPL variables.
    genv.clearAll();
    // Inject ALL live names, including built-ins and pseudo-vars like nargin
    // — the console eval needs them reachable even though they won't appear
    // in the user-visible snapshot.
    for (auto &name : ws_.allNames()) {
        auto *val = ws_.get(name);
        if (val) {
            genv.set(name, *val);
            injectedNames.insert(name);
        }
    }

    // 5. Execute expression.
    std::string evalOutput;
    engine_.setOutputFunc([&evalOutput](const std::string &s) { evalOutput += s; });
    try {
        engine_.eval(code);
    } catch (const std::exception &e) {
        evalOutput = std::string("Error: ") + e.what();
    }

    // Snapshot what the inner eval actually *wrote*. vm_->lastVarMap() only
    // contains names that the inner chunk assigned (compiler's assignedVars
    // drives this set), so it's the authoritative list of "what did the
    // console code change?" — untouched pass-through names don't show up
    // here and therefore won't get incorrectly tagged as shadowed.
    std::unordered_map<std::string, MValue> innerAssigned;
    for (auto &[n, v] : engine_.vm_->lastVarMap())
        innerAssigned[n] = v;

    // 6. Restore paused VM state BEFORE diffing back so ws_'s frame pointers
    //    are pointing at the user's registers again.
    engine_.vm_->restorePausedState(std::move(savedVMState));
    ws_.bindVMFrame(*engine_.vm_, engine_);

    // 7. Apply the inner eval's effects to ws_.
    //
    //    - Names the inner eval ASSIGNED (present in innerAssigned): write
    //      through to ws_. A built-in written here becomes a real shadow.
    //    - Names that were injected but the inner eval REMOVED from the
    //      base workspace (i.e. `clear x`): ws_.remove().
    //    - Names that were injected and are still present unchanged: pure
    //      pass-through, no action — avoids falsely flagging every injected
    //      built-in as shadowed just because it survived the round trip.
    //    - Names the inner eval newly introduced (not injected, but present
    //      in innerAssigned with a non-empty value): ws_.set() lands them
    //      in the overlay. This is the `ans` path for bare-expression
    //      console inputs like `cos(10)`.
    {
        auto wsNames = genv.localNames();
        std::unordered_set<std::string> nowNames(wsNames.begin(), wsNames.end());

        // nargin/nargout are pseudo-vars bound per-function-call; don't
        // propagate them to the overlay where they'd masquerade as
        // persistent workspace state.
        auto isTransientPseudo = [](const std::string &n) {
            return n == "nargin" || n == "nargout";
        };

        for (auto &name : injectedNames) {
            auto it = innerAssigned.find(name);
            if (it != innerAssigned.end()) {
                const MValue &v = it->second;
                if (v.isUnset() || v.isDeleted())
                    ws_.remove(name);
                else
                    ws_.set(name, v);
            } else if (!nowNames.count(name)) {
                // Injected but now missing → cleared by the inner eval.
                ws_.remove(name);
            }
            // else: untouched, pass through.
        }
        for (auto &[name, val] : innerAssigned) {
            if (injectedNames.count(name))
                continue; // already handled above
            if (isTransientPseudo(name))
                continue;
            if (!val.isUnset() && !val.isDeleted())
                ws_.set(name, val);
        }
    }

    // 8. Restore workspaceEnv to its pre-eval state. The flag markClearAll()
    //    might have been set by a console `clear`; reset it so later
    //    engine.eval() calls don't behave as if clearAll was requested.
    genv.clearAll();
    for (auto &[n, v] : preEvalEnv)
        genv.set(n, v);
    engine_.clearAllCalled_ = false;

    // 9. Restore output capture, observer, and controller state.
    engine_.setOutputFunc([this](const std::string &s) { outputBuf_ += s; });
    engine_.setDebugObserver(observer_);

    ctl = engine_.debugController();
    if (ctl) {
        ctl->callStack() = std::move(savedCallStack);
        ctl->setLastLine(savedLine);
    }

    while (!evalOutput.empty()
           && (evalOutput.back() == '\n' || evalOutput.back() == ' '))
        evalOutput.pop_back();

    return evalOutput;
}

std::string DebugSession::takeOutput()
{
    std::string out = std::move(outputBuf_);
    outputBuf_.clear();
    return out;
}

} // namespace numkit
