# lang.command — `N-1` / `a/2` statements mislex as COMMAND calls when the head is a variable (MATLAB: expression)

- **Status:** ✅ FIXED (23ddec5ad, 2026-09-06)
- **Kind:** bug
- **Severity:** P1 wrong result (the statement dies with "undefined function 'N'" instead of evaluating)
- **Found:** 2026-09-06 while chasing the eval-family guard — `eval('N/2')` failed with a CALL-shaped error although `eval('N / 2')` worked; bisected to the lex level, no eval involved at all.

## Symptom

A statement whose head identifier names a VARIABLE, followed by a GLUED
`-` or `/` and an operand, lexes as a command-style call (`which -all`
idiom) and executes `N('/2')` → "VM: undefined function 'N'". MATLAB
evaluates the expression (probed R2025b: `x=5; x -1` → 4, `x/2` → 2.5,
`x-y` → 2; only an UNDEFINED head is a command call).

## Repro

```matlab
clear;
N = 42;
N-1
% numkit (pre-fix): VM: undefined function 'N' (in call to 'N')
% MATLAB R2025b:  41
```

## Root cause

`isCommandStyleCall` (parser.cpp) treats IDENT + glued MINUS/SLASH +
operand as command syntax — correct for `which -all sin`, but it cannot
know the head is a variable. Two engine-level gaps compounded it:
variables created earlier in the SAME script chunk live in VM registers
(not yet in workspaceEnv), and eval()/input() mid-chunk could not see
caller variables at all (the eval-family bug, fixed the same day).

## Fix

`Engine::rewriteVarHeadedCommands` (engine.cpp): after parsing, top-level
COMMAND_CALL statements whose head is a workspace variable and whose
single glued `-`/`/` argument parses as a number/identifier are REWRITTEN
in place into `EXPR_STMT(BINARY_OP(...))` — the exact shape
parseExpressionStatement produces, so the expr-statement compiler binds
and returns the value normally. Runs per-statement inside the eval split
loop (after earlier statements synced their variables) and before the
single-statement chunk; real commands keep their behavior (the head is a
function, not a variable). The EXPR_STMT shape is load-bearing: a bare
BINARY_OP statement skips the ans/value path and yields an empty chunk
result.

## References

- `src/core/src/engine.cpp` (Engine::rewriteVarHeadedCommands + the two
  per-statement call sites in Engine::eval)
- Guard: `EvalFamilyKnownBug.EvalFamilyCallerVarVisibility` (live; covers
  eval + input + the glued forms `N-1`/`N/2`)
- Probes: probe_cmd.m values (4 / 2.5 / 2 / command error for `zzz -1`)
