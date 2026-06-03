// include/callback_builtin.hpp
#pragma once

#include <numkit/core/value.hpp>

#include <cstddef>
#include <memory>

// State-machine callbacks (VM_CALLBACKS_PLAN.md).
//
// A higher-order builtin (cellfun / arrayfun / …) that calls user code in a
// loop normally drives that loop on the C++ stack and runs each callback to
// completion via Engine::callFunctionHandleMulti (the re-entrant callReentrant
// path). A breakpoint inside such a callback therefore can fire but cannot
// suspend — the pause cannot unwind through the builtin's C++ for-loop.
//
// The continuation mechanism removes that C++ loop: the builtin is expressed as
// a resumable state machine that, between callbacks, RETURNS control to the VM
// dispatch loop. Each callback then runs as an ordinary VM frame (pushed on
// frames_), which is fully pausable — the same property in-bytecode calls enjoy.
// No fiber / separate stack / Asyncify needed; works identically on every
// preset including WASM.

namespace numkit {

class VM;
class Engine;

// A suspended native computation driving user-code callbacks as VM frames.
// Owned via shared_ptr: it lives on the awaited frame (CallFrame::cont) and is
// handed back to itself on each resume so it can re-attach to the next frame.
struct VmContinuation
{
    virtual ~VmContinuation() = default;

    // Advance the state machine by one step.
    //   prevResult — the value the just-returned callback frame produced, or
    //                nullptr on the very first step (before any callback ran).
    //   self       — this continuation (shared ownership) so step() can pass it
    //                to VM::pushCallbackFrame when scheduling the next callback.
    // Returns true  → a new callback frame was pushed; the computation is
    //                 suspended and will resume when that frame returns.
    // Returns false → finished; the final result has already been written to the
    //                 captured destination (no frame pushed).
    virtual bool step(VM &vm, Value *prevResult,
                      const std::shared_ptr<VmContinuation> &self) = 0;
};

// A higher-order builtin that can run its callbacks on the VM. Registered on the
// Engine under the builtin's name (alongside its ordinary synchronous external
// registration). The VM consults it at the call site BEFORE the synchronous
// path: tryStart returns a continuation to drive, or nullptr to fall back to the
// ordinary builtin (e.g. when the function handle is itself a builtin — no user
// code to step through — or the argument form isn't one the state machine
// covers). `dest` is where the single output must be written: a stable pointer
// into the VM register stack, valid for the whole computation.
struct CallbackBuiltin
{
    virtual ~CallbackBuiltin() = default;
    virtual std::shared_ptr<VmContinuation> tryStart(Span<const Value> args, std::size_t nargout,
                                                     Value *dest, Engine &eng) = 0;
};

} // namespace numkit
