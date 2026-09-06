# signal.zp2sos — section ORDER was inverted vs MATLAB (ascending |p|, ties ascending |Re|)

- **Status:** ✅ FIXED (c266701e3, 2026-09-06)
- **Kind:** bug
- **Severity:** P3 wrong order (values per-section are right; the section sequence differs)
- **Found:** 2026-09-06 via the interactive-book portion — pr5_1 (buttap + zp2sos) flags `sos: values diverge (max rel 5.9e-01)` after its z-typing was fixed

## Symptom

For an all-pole prototype (empty z, e.g. buttap), zp2sos emitted the
second-order sections in the OPPOSITE order to MATLAB. The probed rule
(buttap N=4/6, cheb1/cheb2/ellip N=4): sections by ASCENDING pole-pair
radius |p|, ties by ascending |Re(p)| — buttap's equal-radius pairs put
the pair nearest the imaginary axis first. numkit picked max-|p| first.

## Repro

```matlab
clear;
[~, p, k] = buttap(4);
sos = zp2sos(zeros(0,0), p, k);
% MATLAB R2025b:  sos = [0 0 1 1 0.7654 1; 0 0 1 1 1.8478 1]
% numkit (pre-fix):    [0 0 1 1 1.8478 1; 0 0 1 1 0.7654 1]
```

## Root cause

popPair (conversions.cpp) picked the max-|p| root first, emitting the
highest-radius pole pair as section 1 — the opposite of MATLAB's
ascending order. NOTE: the first probe attribution was inverted (the
"MATLAB" lines were numkit output) — the clean separated dual run
established MATLAB = LOW-frequency pair first; expected values in guards
always come from a separated probe.

## Fix

popPair now selects by lexicographic (|p|, |Re(p)|) MINIMUM. Live guard:
`Zp2sosSectionOrderButtap` (buttap N=4 and N=6 first/last sections).

## References

- Guard: `Zp2sosSectionOrderButtap` (live) in
  `src/toolboxes/signal/tests/known_bugs_test.cpp`
