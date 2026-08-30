# lang.command-syntax — command-call args mangle/parse-fail on `:` `/` (URLs), MATLAB takes them literally

- **Status:** 🔴 OPEN
- **Severity:** P1 wrong result (silent arg truncation) + parse failure
- **Kind:** bug
- **Found:** 2026-08-30 via fieldtest (Chinese textbook corpus — `web -broswer http://…` is a common first-line idiom; batch 20260830-003212, `example02_05.m` / `example02_10.m`)

## Symptom

Two divergent shapes, both wrong vs MATLAB:

1. **Parse failure** — `foo -bar http://x.y/z` → `Error: Unexpected token '/'
   at line 1 col 15`. MATLAB parses the whole command form and then errors
   (or runs) at the FUNCTION level, never at the token level.
2. **Silent truncation** — `disp a//b:c` prints `a//b`, dropping `:c`.
   MATLAB prints `a//b:c` — in command syntax every whitespace-delimited
   run after the command name is ONE literal char argument.

## Repro

```matlab
clear;
disp a//b:c
% numkit: a//b
% MATLAB: a//b:c

web -broswer http://www.ilovematlab.cn/f
% numkit: Error: Unexpected token '/' at line 1 col 19
% MATLAB: opens the URL (or errors about `web`, never about '/')
```

Real-world damage: 3 corpus scripts die at line 1 (the idiom
`web -broswer http://…` header in textbook code).

## Root cause

The command-syntax argument lexer accepts identifier/number-ish tokens plus
`/`, but `:` terminates a token (silent truncation in the `disp` form) and
the two-token form (`web -broswer http://…`) never re-enters command parsing
after `-broswer`, so `http://` is lexed as division.

## Suggested fix

MATLAB's rule: after a command-call head, the rest of the line up to a
statement terminator is split on whitespace; each run is a char literal —
no token-level lexing of `/ : . -`. Implement "command-arg run" as a lexer
mode triggered by the parser's command-call detection. Guard: the `disp`
truncation is the worse half (wrong result, no error).

## References

fieldtest batches `reports/20260829-smoke1-15scripts.json`,
`reports/20260830-003212.json`; regression test:
`tests/gtest/integration/fieldtest_regressions_test.cpp`
(`DISABLED_FieldTest_CommandSyntaxUrlArgs`).
