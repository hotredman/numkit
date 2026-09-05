# optim.linprog — rank-deficient internal solve surfaces as a hard error instead of a solution

- **Status:** 🔴 OPEN
- **Severity:** P2 (works in MATLAB, refused in numkit)
- **Kind:** bug
- **Found:** 2026-09-05 via fieldtest portion 10 (springer-math
  linear-programming book, example1a/1b/2a/2b — 4 scripts, same site)

## Symptom

`linprog(c, A, b, Aeq, beq, lb, ub)` throws
`mldivide: matrix is singular or rank-deficient` from an internal solve;
MATLAB solves the same problem (its mldivide produces a least-squares /
warning path rather than an error).

## Repro (self-contained)

```matlab
clear;
c = [-2; 4; -2; 2];
A = [-2 0 -3 0; -3 2 0 -4];
b = [-6; -8];
Aeq = [4 -3 8 -1; 1 0 0 1];
beq = [20; 18];
lb = [1; 0; 2; 0];
ub = [Inf; Inf; 10; Inf];
[x, f] = linprog(c, A, b, Aeq, beq, lb, ub);
disp(f)
% numkit:  Error: mldivide: matrix is singular or rank-deficient
% MATLAB R2025b: f = 14.4, x = [4.4 0 2 13.6]
```

## Root cause (hypothesis)

The linprog implementation's internal solve calls `\` (strict throw on
rank deficiency). MATLAB's mldivide handles rank-deficient systems via
least-squares with a warning — the shim should do the same (or use
pinv) internally.

## References

- **Guard:** `DISABLED_LinprogRankDeficientSolves` in
  `src/toolboxes/optim/tests/fminunc_test.cpp` (f = 14.4, x(1) = 4.4,
  MATLAB-probed) — RED under --gtest_also_run_disabled_tests.
