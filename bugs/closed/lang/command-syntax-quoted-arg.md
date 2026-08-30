# lang.command-syntax — quoted-string command arguments are silently dropped (`disp 'abc'` → prints nothing)

- **Status:** ✅ FIXED (2026-08-30)
- **Severity:** P1 wrong result (silent no-op)
- **Kind:** bug
- **Found:** 2026-08-30 while fixing command-syntax-url-args — verified PRE-EXISTING (identical behaviour with the fix stashed)

## Symptom

A quoted string as the (sole) command argument evaluates to an empty
statement: no output, no error, `ans = []` echoed. MATLAB prints the string.

## Repro

```matlab
clear;
disp 'abc'
% numkit: (nothing; ans = [])
% MATLAB: abc
disp 'hello world'
% numkit: (nothing)
% MATLAB: hello world
```

## Root cause (suspected)

Not the argument collector (the 2026-08-30 rewrite is irrelevant — verified
by stash-test). Likely the lexer/detection interplay: after
`disp<space>'abc'` the `'` may be lexed as TRANSPOSE (adjacency decided
without accounting for the intervening space) or detection rejects the
STRING first-token — the command form never engages and the statement
degrades to a bare `disp` expression.

## Suggested fix

Check the lexer's transpose-vs-string decision (space-sensitivity), then
confirm `isCommandStyleCall` reaches the STRING first-arg branch. Guard:
extend `tests/gtest/integration/fieldtest_regressions_test.cpp` or
`CommandStyleTest` with the two repro lines.

## References
- **Guard:** `CommandSyntaxQuotedArg` (live, dual-engine)

Sibling: `bugs/closed/lang/command-syntax-url-args.md` (same command-syntax
family, different root — found during its fix). MATLAB probe 2026-08-30
confirms both quoted forms print their content.

## Resolution (2026-08-30)

Root cause was exactly the suspected lexer ambiguity:
`isTransposeContext()` treated ANY quote after a value token as
transpose — including `disp<space>'abc'`, where the quote starts a
STRING. MATLAB's contract (probed on R2025b): transpose operators are
GLUED to their operand — `y = x '` is an Invalid-expression error in
MATLAB itself, and a quote after whitespace starts a string (which is
why `disp 'abc'` is command syntax). The fix adds the adjacency check to
isTransposeContext (previous token's end column == the quote's column,
same line).

Verified: disp 'abc' -> abc; disp 'hello world' -> hello world (space
inside the literal); glued x' and [1 2;3 4]' transposes unchanged; all
46 CommandStyle + 19 quoted/URL regression tests green; guard live on
both engines; full Release suite exit 0.
