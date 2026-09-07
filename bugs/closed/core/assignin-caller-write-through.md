# core.assignin — `assignin('caller', …)` names not visible to the caller's compiled frame

- **Status:** ✅ FIXED (8fee45c65, 2026-09-07)
- **Severity:** P1 wrong result
- **Found:** 2026-08-29 via npm CLI corpus run (examples/ through packages/numkit, WASM engine); reproduces identically on the native `numkit_repl` CLI, so it is engine-level, not WASM/packaging.

## Symptom
A function that receives values via `assignin('caller', name, value)` from a
callee cannot read those names back as ordinary locals: the VM reports
"Undefined function or variable". The statically-compiled frame apparently
does not observe dynamically injected workspace writes.

## Repro
```matlab
% examples/Frame_Introspection/assignin_setter.m (trimmed)
clear;
function setup_constants(prefix)
    assignin('caller', [prefix, '_one'],   1);
    assignin('caller', [prefix, '_two'],   2);
    assignin('caller', [prefix, '_three'], 3);
end

function r = f()
    setup_constants('cfg');
    disp(['  cfg_one   = ', num2str(cfg_one)])   % ← dies here
    r = cfg_one + cfg_two + cfg_three;
end

total = f();

% numkit:  Error (line 39): Undefined function or variable 'cfg_one'
% MATLAB:  Constants visible in the caller:
%              cfg_one   = 1 … total = 6
```

The script's own comment notes the compiler "must see those names statically
… for register write-through to surface them" — `f()` does reference all three
names directly, yet the write-through still does not land.

## Also reproduces (suspected same root cause: dynamic names vs compiled frames)
- `examples/Frame_Introspection/eval_dynamic_code.m` — `Error (line 34): Undefined function or variable 'result'`
- `examples/Frame_Introspection/workspace_introspection.m` — `Error (line 62): Undefined function or variable 'fs'`

## Notes
- Corpus impact: 3 of 184 examples; all other Frame_Introspection cases pass.
- Native repro: `build/.../numkit_repl.exe examples/Frame_Introspection/assignin_setter.m` → same error (and see `apps` exit-code bug: the native CLI still exits 0 on it).

## References
- **Guard:** deferred — engine-level; guard pending a compiled-frame fixture.


## Fix (portion 25, 2026-09-07)

Root cause (traced with instrumented WROTE/MISS/pop prints): the
VM-side write-through in VM::assignInCallerFrame lands in the caller
frame's register, but an INTERVENING CALL between the assignin and the
read leaves that register unset again (the write is lost across the
call boundary — see the g2/g3 isolation: a bare `num2str(1);` statement
between setup and the reads kills every injected name; without one it
works). Engine::assignToCaller already writes the env side — the
caller frame's lazily-allocated private env — so the fix makes ASSERT_DEF
consult it: function frames fall back to their OWN frame.env locals
(getLocal — no parent-chain walk, so NO base-variable leak into
function scope; ScopeIsolationTest stays green). The base frame keeps
its workspaceEnv fallback from the eval-family fix.

All three original repros now pass end-to-end on the CLI:
assignin_setter (Total: 6), eval_dynamic_code, workspace_introspection
(exit 0 each; the last two were the same dynamic-names-vs-compiled-
frames class).

Guard: TW_VM/AssignTest.AssigninCallerWriteThrough — dual-engine,
covers the intervening-call case (the register-path failure), the
direct-read case, and the no-base-leak assertion (exist('cfg_one')==0
after f returns).
