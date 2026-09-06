# core.eval — variables created by eval() INSIDE FUNCTIONS are invisible later in the script (eval-family frame visibility)

- **Status:** ✅ FIXED (HASH, 2026-09-06) (deferred with the assignin family — same root, user decision)
- **Severity:** P2 (clear error, not silent)
- **Kind:** bug
- **Found:** 2026-08-30 — the two unfiled corpus failures behind PUBLISH.md's "3 known OPEN bugs" (only assignin had a catalog entry)

## Symptom

Scripts that create variables via `eval()`/`evalin()` inside functions and
read them afterwards fail with "Undefined function or variable". Top-level
`eval('x = 1;')` WORKS — the failure is the function-frame write-through,
the same compiled-frame visibility root as
`opened/core/assignin-caller-write-through.md`.

## Repro

```matlab
clear;
% examples/Frame_Introspection/eval_dynamic_code.m (line 34):
%   Undefined function or variable 'result'
% examples/Frame_Introspection/workspace_introspection.m (line 62):
%   Undefined function or variable 'fs'
node packages/numkit/bin/cli.js examples/Frame_Introspection/eval_dynamic_code.m
% numkit: Error (line 34): Undefined function or variable 'result'
% MATLAB: runs to completion
```

## Root cause

Same family as assignin-caller-write-through: names created through the
eval-family do not write through to the compiled caller frame's static
slots. Fixing one should fix all three corpus failures (assignin_setter +
these two); the Known-limitations README line covers the family for the
0.1.0 release.

## Suggested fix

With the assignin fix (deferred): route eval-family writes through the
frame's dynamic-variable mechanism (the DebugSession dynamic-vars path
already models this). Extract a minimal function-frame repro first —
top-level eval is NOT a reproducer (verified working).

## References

- Sibling: `opened/core/assignin-caller-write-through.md` (same root, P1,
  deferred by user decision 2026-08-30).
- **Guard:** `DISABLED_EvalFamilyCallerVarVisibility` in `src/bundle/tests/known_bugs_test.cpp` (both halves: eval() and input() cannot see mid-chunk caller variables; replaced the earlier
  assignin fix (the corpus scripts are the reliable reproducers).
- Corpus gate: these two + assignin_setter are PUBLISH.md's "3 known FAIL".


## Fix (2026-09-06, portion 23b)

Both directions fixed, plus the lex-level sibling found on the way:
1. READ side — eval()/input() mid-chunk now see the caller's variables:
   ASSERT_DEF (top-level frames) falls back to workspaceEnv (names synced
   by earlier split-statement chunks), and snapshotFrameVars resolves a
   workspaceEnv request to the BASE frame's live registers.
2. WRITE side — the same workspaceEnv ASSERT_DEF fallback resolves names
   CREATED by an inner eval in later statements of the running chunk.
3. SIBLING (its own file: lang/command-var-head-glued-op) — `N-1`/`a/2`
   statements mislex as COMMAND calls; now rewritten to expressions when
   the head is a workspace variable. This was the actual cause of the
   eval('N/2') CALL-shaped error that the original guard surfaced.

Live guard: EvalFamilyKnownBug.EvalFamilyCallerVarVisibility (eval + input
+ glued forms).
