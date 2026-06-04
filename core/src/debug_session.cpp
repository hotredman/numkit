// src/debug_session.cpp
//
// DebugSession now routes all console-visible variable state through
// DebugWorkspace (see include/debug_workspace.hpp):
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
#include <numkit/core/debug_session.hpp>
#include <numkit/core/compiler.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/core/lexer.hpp>
#include <numkit/core/parser.hpp>

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
        status = skipFalseConditionalBreakpoints(status, initial == DebugAction::Continue);

        if (status == ExecStatus::Paused) {
            selectedDepth_ = 0; // new pause → focus the deepest frame
            ws_.bindVMFrame(*engine_.vm_, engine_, selectedDepth_);
        } else {
            engine_.syncVMToWorkspace();
            deactivate();
        }
        return status;
    } catch (const Error &e) {
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
        status = skipFalseConditionalBreakpoints(status, action == DebugAction::Continue);

        if (status == ExecStatus::Paused) {
            selectedDepth_ = 0; // new pause → focus the deepest frame
            ws_.bindVMFrame(*engine_.vm_, engine_, selectedDepth_);
        } else {
            deactivate();
        }

        return status;
    } catch (const Error &e) {
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

size_t DebugSession::frameCount() const
{
    return (active_ && engine_.vm_) ? engine_.vm_->frameCount() : 0;
}

bool DebugSession::frameUp()
{
    // Move focus toward the base (caller). dbup.
    if (!active_ || !engine_.vm_)
        return false;
    if (selectedDepth_ + 1 >= engine_.vm_->frameCount())
        return false; // already at the base frame
    ++selectedDepth_;
    ws_.bindVMFrame(*engine_.vm_, engine_, selectedDepth_);
    return true;
}

bool DebugSession::frameDown()
{
    // Move focus back toward the current (deepest) frame. dbdown.
    if (!active_ || selectedDepth_ == 0)
        return false;
    --selectedDepth_;
    ws_.bindVMFrame(*engine_.vm_, engine_, selectedDepth_);
    return true;
}

std::string DebugSession::frameFocusLine() const
{
    auto *ctl = engine_.debugController();
    if (!ctl || ctl->callStack().empty())
        return "";
    auto &stack = ctl->callStack();
    size_t depth = selectedDepth_ < stack.size() ? selectedDepth_ : stack.size() - 1;
    const auto &sf = stack[stack.size() - 1 - depth];
    std::string fn = sf.functionName.empty() ? "<script>" : sf.functionName;
    return "In " + fn + " (line " + std::to_string(sf.line) + ")";
}

std::string DebugSession::formatDbStack() const
{
    auto *ctl = engine_.debugController();
    if (!ctl)
        return "";
    auto &stack = ctl->callStack();
    std::string out;
    // Deepest frame first (MATLAB order); '>' marks the dbup/dbdown focus.
    for (size_t i = 0; i < stack.size(); ++i) {
        const auto &sf = stack[stack.size() - 1 - i];
        out += (i == selectedDepth_) ? "> " : "  ";
        std::string fn = sf.functionName.empty() ? "<script>" : sf.functionName;
        out += "In " + fn + " (line " + std::to_string(sf.line) + ")\n";
    }
    if (!out.empty() && out.back() == '\n')
        out.pop_back();
    return out;
}

void DebugSession::addWatch(const std::string &expr)
{
    watches_.push_back(expr);
}

void DebugSession::removeWatch(size_t index)
{
    if (index < watches_.size())
        watches_.erase(watches_.begin() + static_cast<std::ptrdiff_t>(index));
}

void DebugSession::clearWatches()
{
    watches_.clear();
}

std::vector<DebugSession::WatchResult> DebugSession::evalWatches()
{
    std::vector<WatchResult> out;
    out.reserve(watches_.size());
    // Each watch is evaluated in the currently focused frame via the same
    // inject/eval path as the console — so it sees the live (possibly edited)
    // values and dbup/dbdown focus. eval() already turns failures into
    // "Error: ..." strings, so a bad watch never throws here.
    for (auto &expr : watches_)
        out.push_back({expr, active_ ? eval(expr) : std::string()});
    return out;
}

ExecStatus DebugSession::skipFalseConditionalBreakpoints(ExecStatus status, bool running)
{
    // Only when running into breakpoints (Continue); a single-step always stops
    // where it lands, regardless of any breakpoint condition on that line.
    if (!running)
        return status;

    while (status == ExecStatus::Paused) {
        auto *ctl = engine_.debugController();
        if (!ctl || ctl->callStack().empty())
            break;
        uint16_t line = ctl->currentFrame()->line;
        std::string cond = engine_.breakpointManager().conditionForLine(line);
        if (cond.empty())
            break; // unconditional breakpoint → genuine stop

        // Evaluate the condition in the current (deepest) frame via the same
        // inject/eval path the console uses, then read its truthiness.
        ws_.bindVMFrame(*engine_.vm_, engine_, 0);
        bool hold = true; // default: stop if the condition can't be evaluated
        try {
            eval(cond);
            hold = lastEvalValue_.toBool();
        } catch (...) {
            hold = true;
        }
        ws_.unbindFrame();
        if (hold)
            break; // condition true → stop here

        // Condition false → keep running to the next breakpoint.
        engine_.vm_->setFrameDynVars(ws_.overlay().empty() ? nullptr : &ws_.overlay());
        status = engine_.debugResume(DebugAction::Continue);
    }
    return status;
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

    // Report the dbup/dbdown-selected frame (0 = deepest). ws_ is already bound
    // to this frame, so its variables match line/functionName below.
    size_t depth = selectedDepth_;
    if (depth >= stack.size())
        depth = stack.size() - 1;
    auto &frame = stack[stack.size() - 1 - depth];
    snap.line = frame.line;
    snap.col = frame.col;
    snap.functionName = frame.functionName;

    // Build the variables list from the DebugWorkspace (live frame pointers +
    // overlay; deleted/unset slots are filtered out by names()). Each value is
    // COPIED into snap.ownedValues so the Snapshot owns its data — its
    // Variable.value pointers then survive a later resume() that reuses the VM
    // registers. The vector is reserved up-front so push_back never reallocates
    // and the &back() pointers stay valid.
    auto names = ws_.names();
    snap.ownedValues = std::make_shared<std::vector<Value>>();
    snap.ownedValues->reserve(names.size());
    for (auto &name : names) {
        auto *val = ws_.get(name);
        if (val) {
            snap.ownedValues->push_back(*val);
            snap.variables.push_back({name, &snap.ownedValues->back()});
        }
    }

    snap.callStack = stack;
    return snap;
}

std::string DebugSession::eval(const std::string &code)
{
    if (!active_)
        return "Error: no active debug session";

    // Debugger meta-commands (dbup / dbdown / dbstack) move the inspection
    // focus or report the call stack — they are NOT workspace evaluations, so
    // intercept them here (MATLAB handles them the same way at K>>).
    {
        size_t b = code.find_first_not_of(" \t\r\n");
        size_t e = code.find_last_not_of(" \t\r\n;");
        std::string cmd = (b == std::string::npos) ? "" : code.substr(b, e - b + 1);
        if (cmd == "dbup")
            return frameUp() ? frameFocusLine() : "Already at the top of the stack.";
        if (cmd == "dbdown")
            return frameDown() ? frameFocusLine() : "Already at the bottom of the stack.";
        if (cmd == "dbstack")
            return formatDbStack();
    }

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
    std::unordered_map<std::string, Value> preEvalEnv;
    for (auto &n : genv.localNames()) {
        if (auto *v = genv.getLocal(n))
            preEvalEnv.emplace(n, *v);
    }
    // Global membership lives in workspaceEnv_->globals_, NOT local storage, so
    // localNames() misses it. clearAll() below wipes it — capture it so step 9
    // can restore it (the value is in globalsEnv_, untouched). Without this a
    // pre-existing base global would vanish after any debug-console eval.
    std::unordered_set<std::string> preEvalGlobals(genv.globalNames().begin(),
                                                   genv.globalNames().end());

    // 3. Detach the observer so the inner eval doesn't trigger debug hooks.
    engine_.setDebugObserver(nullptr);

    // 4. Inject current debug-workspace contents into workspaceEnv so the
    //    eval sees them like normal base-workspace variables. Value copy
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
    lastEvalValue_ = Value();
    try {
        lastEvalValue_ = engine_.eval(code);
    } catch (const std::exception &e) {
        evalOutput = std::string("Error: ") + e.what();
    }

    // Snapshot what the inner eval actually *wrote*. vm_->lastVarMap() only
    // contains names that the inner chunk assigned (compiler's assignedVars
    // drives this set), so it's the authoritative list of "what did the
    // console code change?" — untouched pass-through names don't show up
    // here and therefore won't get incorrectly tagged as shadowed.
    std::unordered_map<std::string, Value> innerAssigned;
    for (auto &[n, v] : engine_.vm_->lastVarMap())
        innerAssigned[n] = v;

    // 6. Restore paused VM state BEFORE diffing back so ws_'s frame pointers
    //    are pointing at the user's registers again. restorePausedState is a
    //    move-assign (does not throw); the rebind + diff-back below are guarded
    //    by doRestore so a throw there can't leave the engine inconsistent.
    engine_.vm_->restorePausedState(std::move(savedVMState));

    // Return the base workspace + engine I/O / observer / controller to their
    // pre-eval state. Defined as a lambda so it runs on the normal path AND if
    // the diff-back below throws (e.g. bad_alloc): a throw must never leave the
    // base workspace wiped, the observer detached, or output still redirected
    // into the local capture buffer.
    auto doRestore = [&]() {
        // markClearAll() may have been set by a console `clear`; reset it so a
        // later engine.eval() doesn't behave as if clearAll was requested.
        // Global membership (wiped by clearAll, values still in globalsEnv_) is
        // re-declared so base globals don't vanish.
        genv.clearAll();
        for (auto &[n, v] : preEvalEnv)
            genv.set(n, v);
        for (auto &g : preEvalGlobals)
            genv.declareGlobal(g);
        engine_.clearAllCalled_ = false;

        engine_.setOutputFunc([this](const std::string &s) { outputBuf_ += s; });
        engine_.setDebugObserver(observer_);
        if (auto *c = engine_.debugController()) {
            c->callStack() = savedCallStack;
            c->setLastLine(savedLine);
        }
    };

    // 7. Rebind ws_ to the restored frame, then apply the inner eval's effects:
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
    //
    //    Guarded: any failure still restores engine state before returning.
    try {
        ws_.bindVMFrame(*engine_.vm_, engine_, selectedDepth_);

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
                const Value &v = it->second;
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

        // Console-declared globals: a `global X` typed at the prompt records X
        // in workspaceEnv's global set with its value in globalsEnv_ — NOT in
        // lastVarMap (globals are excluded there). Route any NEW global into the
        // overlay so it persists across evals and injects on continue. A
        // pre-existing base global (in preEvalGlobals) is left alone — it's
        // restored as-is by doRestore.
        if (auto *gs = genv.globalsEnv()) {
            for (auto &g : genv.globalNames()) {
                if (preEvalGlobals.count(g) || injectedNames.count(g))
                    continue;
                if (auto *gv = gs->get(g))
                    if (!gv->isUnset() && !gv->isDeleted())
                        ws_.set(g, *gv);
            }
        }
    } catch (const std::exception &e) {
        doRestore();
        return std::string("Error: applying console changes: ") + e.what();
    }

    // 8-9. Restore engine state on the normal path.
    doRestore();

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
