# core.ReactiveSession — multi-param tick sequence leaves AF/mag inconsistent (single-param equivalence holds)

- **Status:** 🟡 OPEN
- **Severity:** P2 (affects any script whose dirty tail contains a
  WILDCARD loop — single- or multi-param; precise (wildcard-free)
  scripts, element writes and parse/budget semantics are unaffected)
- **Kind:** bug
- **Found:** 2026-09-12, Play portion B3 — caught by BOTH equivalence
  tests (single-param theta0 and the N→d→theta0 sequence); element
  writes, parse/budget/cycles are green.

## Symptom

`load` a script with a wildcard tail (`for k=1:N … AF = AF + … end`),
then a sequence of param ticks. After the THIRD tick the workspace
holds `ph` CORRECT (matches full re-run) and params correctly, but
`AF`(read back) and `mag` are MUTUALLY INCONSISTENT: mag(1)=0.1469 =
|Σ₈(θ0=0)|/8 (the mathematically correct value), while AF(1)=1.118
(read right after) is neither the old nor the new accumulation. Tick1
(N) and tick2 (d) are each internally consistent and correct.

## Repro (self-contained; C++ — ReactiveSession is not script-visible yet)

```cpp
StandardEngine engine;
ReactiveSession s(engine);
s.load("N = 16;\ntheta0 = 25;\nd = 0.5;\ntheta = -90:0.2:90;\n"
       "ph = 2*pi*d*(sin(theta*pi/180) - sin(theta0*pi/180));\n"
       "AF = zeros(1, numel(theta));\n"
       "for k = 1:N\n  AF = AF + exp(1i*(k-1)*ph);\nend\n"
       "mag = abs(AF)/N;\n");
s.setParam("N", 8.0);   s.evalDirty();   // consistent
s.setParam("d", 0.7);   s.evalDirty();   // consistent
s.setParam("theta0", 0.0); s.evalDirty();
// ph OK; AF(1) and mag(1) disagree with each other.
```

## Root-cause hypothesis (unconfirmed)

The `for` statement re-runs as ONE chunk whose body assigns `AF` in
chunk-local registers; the workspace sync on chunk completion may
interact badly with the SAME statement re-executing across consecutive
ticks — as if one intermediate write-back wins over the final one on
the third execution. The wildcard-prefix rule itself is not the cause:
tick1/tick2 (also prefix-heavy) stay consistent, ph/params are right on
tick3, and mag sees the CORRECT new AF value while a later raw read of
AF sees a third value.

## References

- **Guards:** `ReactiveTest.DISABLED_DirtySkippingMatchesFullRerun`
  (single-param minimal repro) and
  `ReactiveTest.DISABLED_EquivalenceAcrossSequences` (sequence form) in
  `src/core/tests/reactive_session_test.cpp`.
- Green: element writes + their equivalence, tick budget, cycles
  guard, parse errors, generation (`ReactiveTest.*`, `ElemRegions.*`).
  Probe data (in the memory note): params + ph recompute CORRECTLY;
  the divergence lives in the for-chunk's AF write-back across ticks.
