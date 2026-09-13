# control.tf — tf arithmetic operators (*, +, scalar*) error instead of composing

- **Status:** 🔴 OPEN
- **Severity:** P2 (documented MATLAB operator errors out; composition workflows break)
- **Kind:** bug
- **Found:** 2026-09-12 via Play gallery generation (the PID script had to
  compose polynomials by hand because `G1 * G2` throws).

## Symptom

Arithmetic on `tf` objects (`*`, `+`, scalar `*`) raises a type error
instead of performing series / parallel / gain composition. `series()`,
`parallel()` and `minreal()` themselves work — only the OPERATOR forms
are broken.

## Repro

```matlab
clear;
G1 = tf([1], [1 1]);
G2 = tf([1], [1 2]);
G = G1 * G2
% nk: Error — Cannot convert struct to scalar (in operator '*')
% MATLAB:
%          1
%  -------------
%  s^2 + 3 s + 2
%  Continuous-time transfer function.

P = G1 + G2
% nk: Error — Unsupported types for + (in operator '+')
% MATLAB:
%      2 s + 3
%  -------------
%  s^2 + 3 s + 2

S = 2 * G1
% nk: Error — Cannot convert struct to scalar (in operator '*')
% MATLAB:
%      2
%  -------
%  s + 1
```

Workaround (values verified equal to MATLAB): `series(G1, G2)` /
`parallel(G1, G2)` — both return the composed tf with correct
num/den.

## Root cause

`tf` objects are struct-kinded Values (`kind: 'tf'`, fields
`num`/`den`/`Ts`); the binary-operator resolution layer performs no
polymorphic dispatch on struct kinds — numeric paths try to convert
the struct to a scalar, `+` rejects the operand pair outright.

## Suggested fix

Dispatch binary `*`/`+` on `kind == 'tf'` operands to the EXISTING
toolbox composition functions (`series` / `parallel`,
`src/toolboxes/control/src/connect/connect.cpp`; scalar `*` scales
num). Keep it operator-layer thin — no new math. Follows the
object-model kind-dispatch design (dev-docs/handbook/object_model.md).

## References

- **Guard:** `DISABLED_TfArithmeticOperators`
  (`src/toolboxes/control/tests/known_bugs_test.cpp`)
- `src/toolboxes/control/src/connect/connect.cpp` (series/parallel)
- `bugs/missing.md` control section (minreal et al. — closed since)
