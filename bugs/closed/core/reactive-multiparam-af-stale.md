# core.ReactiveSession — equivalence tests mismatched: FALSE ALARM (the REFERENCE was wrong, the session was correct)

- **Status:** ✅ FIXED (not a product bug — a test-reference bug; commit pending below)
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

## Resolution (2026-09-12, numeric proof)

NOT a VM sync bug. The test REFERENCE built the full run by
pre-pending `theta0 = -10;` to the script — but the script's own
`theta0 = 25;` line then OVERWRODE it, so the "reference" computed with
the default. The session's values were mathematically correct all
along (verified to 6 digits: session ph(1) = -2.596061 = exactly
2*pi*0.5*(sin(-90°) - sin(-10°)); the "full" ph(1) = -4.469287 =
exactly the theta0=25 value; session mag(1) = 0.061006 = the exact
|sum|/16 for theta0=-10). The AF-vs-mag "inconsistency" was real-part
(elemAsDouble on complex) vs magnitude (abs()) — not a divergence.

Reference construction fixed: param def lines REPLACED in the source
string, not pre-pended. Both equivalence tests enabled and green; the
wildcard-prefix rule fix from the SAME debugging session was a REAL
bug and stays (double accumulation, 18x mag error).

## Original (wrong) hypothesis, kept for the record

The `for` statement re-runs as ONE chunk whose body assigns `AF` in
chunk-local registers; the workspace sync on chunk completion may
interact badly with the SAME statement re-executing across consecutive
ticks — as if one intermediate write-back wins over the final one on
the third execution. The wildcard-prefix rule itself is not the cause:
tick1/tick2 (also prefix-heavy) stay consistent, ph/params are right on
tick3, and mag sees the CORRECT new AF value while a later raw read of
AF sees a third value.

## References

- **Guards:** `ReactiveTest.DirtySkippingMatchesFullRerun` and
  `ReactiveTest.EquivalenceAcrossSequences` (both ENABLED — they now
  assert the equivalence with corrected references).
- Green: element writes + their equivalence, tick budget, cycles
  guard, parse errors, generation (`ReactiveTest.*`, `ElemRegions.*`).
  Probe data (in the memory note): params + ph recompute CORRECTLY;
  the divergence lives in the for-chunk's AF write-back across ticks.
