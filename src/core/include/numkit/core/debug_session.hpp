// include/debug_session.hpp
//
// DebugSession: owns compilation + VM state for pausable debug execution.
// Replaces the WASM replay hack with a clean start/resume/snapshot API.
//
// Usage:
//   DebugSession session(engine);
//   session.setBreakpoints({5, 10});
//   auto status = session.start("x = 1;\ny = 2;\n");
//   if (status == ExecStatus::Paused) {
//       auto snap = session.snapshot();
//       // show snap.line, snap.variables to the user
//       status = session.resume(DebugAction::Continue);
//   }
//
#pragma once

#include <numkit/core/ast.hpp>
#include <numkit/core/bytecode.hpp>
#include <numkit/core/debugger.hpp>
#include <numkit/core/debug_workspace.hpp>
#include <numkit/value/value.hpp>
#include <numkit/core/vm.hpp>

#include <memory>
#include <string>
#include <vector>

namespace numkit {

class Engine;
class Compiler;
struct BytecodeChunk;

class DebugSession
{
public:
    explicit DebugSession(Engine &engine);
    ~DebugSession();

    DebugSession(const DebugSession &) = delete;
    DebugSession &operator=(const DebugSession &) = delete;

    // Set breakpoints (line numbers). Must be called before start().
    void setBreakpoints(const std::vector<uint16_t> &lines);

    // Compile code and begin debug execution.
    // Returns Paused if a breakpoint/step was hit, Completed if finished.
    ExecStatus start(const std::string &code);

    // Resume from pause with the given action.
    ExecStatus resume(DebugAction action);

    // Stop the debug session (cleans up state).
    void stop();

    // ── dbstop if error ──────────────────────────────────────
    // When enabled, an uncaught error during the run pauses at the failing
    // line (frames intact) instead of ending the session, so state can be
    // inspected. Resuming lets the error finally propagate (session ends with
    // it). Set before start().
    void setStopOnError(bool enabled);
    bool atErrorPause() const;
    std::string errorPauseMessage() const;

    // ── Frame navigation (dbup / dbdown) ─────────────────────
    // Move the inspection focus one frame toward the base (frameUp / dbup) or
    // back toward the current frame (frameDown / dbdown), WITHOUT resuming.
    // snapshot() and eval() that follow operate on the newly selected frame —
    // so a caller's locals can be inspected and edited at a pause inside a
    // nested call. Returns true if the focus moved (false at the stack ends).
    // The focus resets to the deepest frame on every pause.
    bool frameUp();
    bool frameDown();
    // Current focus depth (0 = deepest/current frame) and the live frame count.
    size_t selectedFrame() const { return selectedDepth_; }
    size_t frameCount() const;

    // ── Watch expressions ────────────────────────────────────
    // Expressions re-evaluated at each pause for an IDE watch panel.
    struct WatchResult
    {
        std::string expr;
        std::string result; // eval display, or "Error: ..." if it failed
    };
    void addWatch(const std::string &expr);
    void removeWatch(size_t index);
    void clearWatches();
    const std::vector<std::string> &watchExpressions() const { return watches_; }
    // Evaluate every watch in the currently focused frame. Valid while paused.
    std::vector<WatchResult> evalWatches();

    // ── State inspection (valid when paused) ─────────────────

    struct Variable
    {
        std::string name;
        const Value *value = nullptr;
    };

    struct Snapshot
    {
        uint16_t line = 0;
        uint16_t col = 0;
        std::string functionName;
        std::vector<Variable> variables;
        std::vector<StackFrame> callStack;

        // Owned backing store for variables[].value. snapshot() copies each
        // value here (COW — cheap) and points Variable.value into it, so the
        // snapshot is self-contained: its pointers stay valid for the
        // Snapshot's own lifetime — including across a resume() that reuses the
        // VM registers the live values came from. shared_ptr so copies/moves of
        // the Snapshot keep the same backing alive.
        std::shared_ptr<std::vector<Value>> ownedValues;
    };

    Snapshot snapshot() const;

    // Evaluate an expression in the paused context (preserves debug state).
    // Returns the captured output (e.g. "x = 42").
    std::string eval(const std::string &code);

    // Get accumulated output since last start/resume.
    std::string takeOutput();

    // Get error message (valid when start/resume returned after an exception).
    const std::string &errorMessage() const { return errorMsg_; }
    int errorLine() const { return errorLine_; }

    bool isActive() const { return active_; }

private:
    // Tear down the shared "session is live" state (observer, script
    // scope, AST). Called from every exit path — natural completion,
    // error unwind, explicit stop.
    void deactivate();

    // One-line "In <fn> (line N)" status for the currently focused frame, and
    // the MATLAB-style dbstack listing (deepest first, '>' marks the focus).
    std::string frameFocusLine() const;
    std::string formatDbStack() const;

    // After a run lands on a breakpoint, transparently keep running past any
    // breakpoint whose condition evaluates false (only when `running` — a step
    // always stops). Returns the status of the real stop / completion.
    ExecStatus skipFalseConditionalBreakpoints(ExecStatus status, bool running);

    // Core of eval(): inject the paused frame's variables into the workspace,
    // run `code`, restore. When applyChanges is true (console input) the
    // changes are diffed back into the DebugWorkspace; when false (read-only
    // condition / watch evaluation) they are NOT — so a watch/condition never
    // pollutes the workspace (e.g. with `ans`). Sets lastEvalValue_; returns the
    // captured display output. Assumes the session is active.
    std::string runInDebugScope(const std::string &code, bool applyChanges);

    Engine &engine_;
    bool active_ = false;

    // Compiled chunk (must outlive VM execution)
    BytecodeChunk chunk_;

    // Parsed AST. Held for the lifetime of the debug session so the
    // engine's scriptLocalFuncs_ (populated in start()) keeps valid
    // pointers into it — clearUserFunctions walks that list whenever
    // a `clear all` fires from inside the paused script.
    ASTNodePtr ast_;

    // Error state
    std::string errorMsg_;
    int errorLine_ = 0;

    // Observer — always returns Stop so the VM pauses at every event
    class SessionObserver : public DebugObserver
    {
    public:
        DebugAction onLine(const DebugContext &) override { return DebugAction::Stop; }
        DebugAction onBreakpoint(const DebugContext &) override { return DebugAction::Stop; }
        void onError(const DebugContext &, const std::string &) override {}
        void onFunctionEntry(const DebugContext &) override {}
        void onFunctionExit(const DebugContext &) override {}
    };
    std::shared_ptr<SessionObserver> observer_;

    // Live view of variables at the pause point: frame pointers + overlay.
    DebugWorkspace ws_;

    // Inspection focus for dbup/dbdown: 0 = deepest (current) frame, increasing
    // toward the base. Reset to 0 on every pause; ws_ is bound to this frame.
    size_t selectedDepth_ = 0;

    // Watch expressions, re-evaluated at each pause (see evalWatches()).
    std::vector<std::string> watches_;

    // dbstop-if-error mode requested for this session (applied to the VM).
    bool stopOnError_ = false;

    // Result Value of the most recent eval() — used to read a breakpoint
    // condition's / watch's truthiness without re-parsing the display string.
    Value lastEvalValue_;

    // Output capture
    std::string outputBuf_;
    std::function<void(const std::string &)> savedOutputFunc_;
};

} // namespace numkit
