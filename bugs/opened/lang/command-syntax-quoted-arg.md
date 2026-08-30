# lang.command-syntax — quoted-string command arguments are silently dropped (`disp 'abc'` → prints nothing)

- **Status:** 🔴 OPEN
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
- **Guard:** `DISABLED_CommandSyntaxQuotedArg`

Sibling: `bugs/closed/lang/command-syntax-url-args.md` (same command-syntax
family, different root — found during its fix). MATLAB probe 2026-08-30
confirms both quoted forms print their content.
