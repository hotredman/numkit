# control.minreal — minimal realization missing

- **Status:** ✅ FIXED (2026-06-19) — pole/zero cancellation
- **Severity:** P2 (missing function)
- **Kind:** missing-fn
- **Found:** 2026-06-04 via missing-fn sweep

## Symptom
`minreal(sys)` — cancel pole/zero pairs (transfer functions) or remove
uncontrollable/unobservable states (state space) to produce a minimal
realization — is not registered.

## Repro
```matlab
sysr = minreal(tf([1 1], [1 2 1]));   % (s+1)/(s+1)^2  ->  1/(s+1)
[n,d] = tfdata(sysr, 'v');
% MATLAB: n = [0 1], d = [1 1]
% numkit: Error — VM: undefined function 'minreal'
```

## Fix (2026-06-19)
Implemented `numkit::control::minreal` (`conversion.cpp`):

- **tf**: `roots(num)` / `roots(den)`, then greedily cancel each pole
  against the nearest surviving zero within relative tolerance `tol`
  (default `sqrt(eps)`), and rebuild `num = num_lead · ∏(surviving zeros)`,
  `den = den_lead · ∏(surviving poles)` by complex `∏(s − r)` expansion
  (real part taken — conjugate pairs cancel symmetrically, so the result is
  real). Leading-coefficient ratio is kept ⇒ gain preserved. Returns a `tf`.
- **zpk**: `zp2tf` → cancel → `tf` (kind change documented).
- **ss** (SISO): `ss2tf` → cancel → `tf2ss` ⇒ a reduced-order `ss`. The
  realization is not unique, so this matches MATLAB on **order + transfer
  function**, not the exact `A,B,C,D`. MIMO ss throws (documented gap).

`tfdata('v')` pads `num` to `den` length (existing MATLAB convention), so
`1/(s+1)` reads back as `n=[0 1], d=[1 1]`.

Verified vs MATLAB R2025b (parity `minreal.json` → OK): `(s+1)/(s+1)²`→
`[0 1]/[1 1]`; `(s+1)/((s+1)(s+2))`→`[0 1]/[1 2]`; `2(s+1)/(s+1)²`→`[0 2]/
[1 1]` (gain); `1/(s+1)`→unchanged; complex `(s²+1)/((s²+1)(s+3))`→`[0 1]/
[1 3]` (conjugate pair cancels cleanly); SISO ss with an uncontrollable
mode → order 2→1, tf `1/(s+1)`. Guards: `minreal_test.cpp` (8 TEST_F:
simple / one-of-two / gain / no-cancel / complex-pair / ss-order /
mimo-throws), `known_bugs_test.cpp` (`Minreal`, promoted live); smoke
`minreal_smoke.m`.

## References
- `src/toolboxes/control/src/conversion/conversion.cpp` (`minreal`,
  `cancelTf`, `polyFromRoots`), `.../include/numkit/control/conversion/conversion.hpp`,
  `src/bundle/src/register/control/lti/lti_reg.cpp` (`minreal_reg`).
- `tools/parity/specs/minreal.json`.
- shipped + reused: `tf`/`ss`/`tfdata`/`roots`/`tf2ss`/`ss2tf`
- MATLAB `doc minreal`
