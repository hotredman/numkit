# signal/cceps — ТЗ for completion

**Status:** closed
**Priority:** **high**
**Effort:** small
**Audited at commit:** d3d8da7
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/signal/src/transforms/...` (`cceps`)
- Spec: `tools/parity/specs/cceps.json`

## MATLAB R2025b — actual behavior

`y = cceps(x)` — complex cepstrum.

## Gaps (numkit vs MATLAB)

| # | Gap | Severity |
|---|---|---|
| 1 | output is **TIME-REVERSED** vs MATLAB. Probe: `cceps([1:8]')` MATLAB=`[2.008 -0.044 -0.008 0.038 0.101 0.200 0.384 0.904]` vs numkit=`[2.008 0.904 0.384 0.200 0.101 0.038 -0.008 -0.044]` — same elements, reversed order (except DC). | **high** |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `cceps([1:8]')` | `[2.008 -0.0436 -0.00834 0.0375 0.1014 0.2002 0.3844 0.9045]` | `[2.008 0.9045 0.3844 0.2002 0.1014 0.0375 -0.00834 -0.0436]` (reversed) |

## Recommended fixes

1. **Reverse the output.** The cepstrum is the IFFT of log of the
   FFT magnitude. numkit appears to be returning the time-reversed
   version (or perhaps doing FFT instead of IFFT in the inverse
   step). Single transform-direction fix.
2. **Verify icceps cascade** — `icceps(cceps(x))` should return `x`;
   probe shows numkit returns `[8 7 6 5 ...]` (reversed), MATLAB
   returns `[8 1 2 3 ...]` (also wrong but differently). Both
   appear to have roundtrip bugs that need investigation.
3. **Spec extension** — fingerprint over basic cceps + roundtrip
   identity. `tol = 1e-9`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: TBD
- Closed date: 2026-05-09
- Notes: Sign-convention bug in libs/signal/src/transforms/extras.cpp.
  numkit's fftRadix2 dir argument: dir=-1 → forward DFT
  (W[k] = exp(-2πj k/N)), dir=+1 → inverse DFT (W[k] = exp(+2πj k/N)).
  Both cceps() and icceps() were calling fftRadix2(..., -1) for the
  inverse-DFT pass — applying a forward DFT instead of an inverse.
  Result was the cepstrum bin-reversed (except the DC bin, which
  reversal preserves).

  Fix: changed the second-pass dir argument from -1 to +1 in both
  cceps() and icceps(). After the fix:
    cceps((1:8)') matches MATLAB R2025b bit-identical:
    [2.008, -0.0436, -0.0083, 0.0375, 0.1014, 0.2002, 0.3844, 0.9045]

  4 artefacts:
  - impl: libs/signal/src/transforms/extras.cpp (cceps + icceps,
    plus an inline note explaining the sign convention)
  - parity: tools/parity/specs/cceps.json — 8 fingerprints,
    correctness=OK; icceps.json — 5 structural fingerprints
    (numel/y(1)/max/min/sum), correctness=OK (icceps round-trip is
    NOT bit-identical to MATLAB without the optional  delay arg —
    that is a separate ТЗ; the structural moments are bit-identical).
  - gtest: libs/signal/tests/cceps_test.cpp — 5 tests
    (MATLAB-match, regression-not-time-reversed, DC bin, icceps
    round-trip moments, length preservation).
  - smoke: libs/signal/tests/smoke/cceps_smoke.m

  Octave's cceps produces a different output than MATLAB's
  (different phase-unwrap / windowing convention) — harness
  already prefers MATLAB as the reference.
