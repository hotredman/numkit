# lang.< > <= >= — relational ops on complex must compare REAL parts (MATLAB semantics), numkit refuses

- **Status:** 🔴 OPEN
- **Severity:** P1 wrong result (errors where MATLAB returns a value)
- **Kind:** bug
- **Found:** 2026-08-30 via fieldtest (real-world corpus `AHP.m`, batch 20260830-003212)

## Symptom

`<`, `>`, `<=`, `>=` on complex operands throw
`Operator '<' is not supported for complex operands`. MATLAB R2025b compares
the **real parts** and returns a logical — even for a nonzero imaginary part.

## Repro

```matlab
clear;
x = complex(1,0); disp(x < 2)
% numkit: Error: Operator '<' is not supported for complex operands
% MATLAB: 1
disp((0+1i) < 2)
% numkit: same error
% MATLAB: 1        (real(0+1i)=0 < 2)
```

Real-world trigger: AHP.m normalises an eigenvector; `eig` returns a
complex-typed matrix (zero or tiny imaginary parts) and the subsequent
comparison chain dies in numkit while MATLAB runs the script to completion.

## Root cause

The relational-op lowering checks `isComplex(dtype)` and refuses; MATLAB's
`lt/gt/le/ge` for complex compare `real(A) < real(B)` (verified on R2025b).
`==`/`~=` already compare complex correctly (exact complex equality) — only
the orderings diverge.

## Suggested fix

In the relational op implementation: if both operands are complex (or
complex-vs-real), compare `real()` of both sides (MATLAB ignores the
imaginary part entirely for orderings). Add transfer/inference parity:
result is logical, shape-preserving/broadcast — same as the real path.
Parity spec: `lt_complex` (`(0+1i)<2 → 1`, `(3+1i)<2 → 0`, matrices).

## References

fieldtest batch `reports/20260830-003212.json` (AHP.m → "unsupported");
regression test: `tests/gtest/integration/fieldtest_regressions_test.cpp`
(`DISABLED_FieldTest_ComplexRelationalComparesRealPart`).
