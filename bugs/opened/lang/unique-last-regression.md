# lang.unique — `'last'` option lost: ia/ib point to FIRST occurrences again — regression of the closed fix

- **Status:** 🔴 OPEN
- **Severity:** P1 wrong result
- **Kind:** bug
- **Found:** 2026-08-30 via the orphaned-DISABLED_-test audit (the guard was never enabled after the fix; force-run catches the regression)

## Symptom

`[b, ia] = unique(x, 'last')` returns `ia` pointing at FIRST occurrences
(`'first'` semantics) — the `'last'` option is silently ignored.

## Repro

```matlab
clear;
[b, ia] = unique('cbabc', 'last');
disp(ia)
% numkit:  3 2 1   (first-occurrence indices — 'last' ignored)
% MATLAB:  5 4 3   (last occurrences; sorted values a b c at their final positions)
```

## Root cause

Unknown yet — the option was handled when `closed/math/unique-last.md` was
fixed (the DISABLED_ guard from that fix asserts ia(1)=3 and now FAILS);
the option routing was lost somewhere later. Same pattern as today's
complex-chol regression: closed bug + never-enabled guard = silent return.

## Suggested fix

Restore `'last'` routing in the unique/iscore set; then enable the guard
(see below) and check the live twin `src/builtin/tests/unique_last_test.cpp`
— it passes today, so it evidently does not cover the option; extend it.

## References
- **Guard:** `DISABLED_UniqueStableLast`

`closed/math/unique-last.md` (the original fix); failing guard:
`src/bundle/tests/known_bugs_test.cpp` `DISABLED_UniqueStableLast` (keep
DISABLED_ until fixed — it is the reproducer).
