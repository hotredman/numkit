# core.compiler — register exhaustion (>255) surfaces as a user error on real code instead of the TreeWalker fallback

- **Status:** 🔴 OPEN (minimal repro distilled 2026-08-31; fix pending)
- **Severity:** P2 (works in MATLAB, refused in numkit)
- **Kind:** bug
- **Found:** 2026-08-30 via fieldtest (real-world `sa_tsp.m` — simulated
  annealing TSP); minimal shape distilled 2026-08-31

## Symptom

```
Error: Compiler: register exhaustion (>255 registers needed in chunk)
```

MATLAB R2025b runs the same code to completion. The documented design
(CORE_ARCHITECTURE / dual-engine) says VM compile failure — including
register exhaustion — falls back to the TreeWalker reference engine; here
the error surfaced to the user instead.

## Repro (self-contained)

```matlab
clear;
y = [1 1 1 1 ... 1];   % a row literal with 253 elements
disp(numel(y));
% numkit: Error: Compiler: register exhaustion (>255 registers needed in chunk)
% MATLAB R2025b: 253
```

Exact ceiling (binary-searched 2026-08-31): **252 elements compile, 253
fail** (253 + chunk overhead > 255). The same error reproduces for a
253-operand `1+1+…` chain and a 300-argument call — every shape that keeps
>~252 values live in one chunk.

Generate the literal: `python -c "print('y = [' + ' '.join(['1']*253) + '']; disp(numel(y));')" > r.m && numkit r.m`

## Root cause (hypotheses to check)

1. The TreeWalker fallback exists only for FUNCTION bodies, not top-level
   scripts (script chunk compile throws straight through);
2. or the fallback triggers but the TreeWalker itself also fails, and the
   original compile error is what gets reported;
3. or the expression legitimately needs that many live registers and the
   compiler could reuse them (register-lifetime bug) — a real compiler fix
   (a matrix literal does NOT need 253 simultaneously-live registers: each
   element can be folded into the accumulator as it is emitted).

Hypothesis 3 is the most likely real defect: the literal builder appears to
allocate one register per element instead of accumulating.

## Suggested fix

Whatever the branch: a real-world script that MATLAB executes must never
die on an internal register ceiling. For matrix literals specifically,
accumulate element-by-element (constant live set). The fallback contract
(register exhaustion → TreeWalker) must hold at every throw site in
`src/core/src/compiler.cpp` (currently 3: preImportGlobals,
pre-allocation of assigned vars, chunk alloc).

## References

- **Guard:** `DISABLED_RegisterExhaustionMatrixLiteralFallsBack` in
  `src/core/tests/vm_test.cpp` (asserts the 253-element literal evaluates;
  remove the prefix when fixed).
- Related: stack_safety.md (the same >255 `uint8_t` register-file ceiling).
