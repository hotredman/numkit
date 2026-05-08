# wavelet/meyeraux — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** ddf4218
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/wavelet/src/shape/shape.cpp` (`meyeraux` impl)
- Adapter: same file
- Spec: `tools/parity/specs/meyeraux.json`
- `Y = meyeraux(X)` — element-wise polynomial; matches MATLAB exactly

## MATLAB R2025b — actual behavior

Documented signatures (`help meyeraux`):

- `Y = meyeraux(X)` — only signature; X is real-valued vector or
  matrix.

Polynomial: `35x⁴ − 84x⁵ + 70x⁶ − 20x⁷` evaluated element-wise.

## Gaps (numkit vs MATLAB)

**No major gap detected.** All probed values match exactly.

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `meyeraux([0 0.25 0.5 0.75 1])` | `[0 0.0706 0.5 0.9294 1]` | identical ✅ |

## Recommended fixes

1. **Spec extension** — add fingerprint covering matrix input,
   negative values (out-of-`[0,1]` domain — MATLAB still applies the
   polynomial, doesn't clip), and edge cases (X = exactly 0 / 1).
   `tol = 1e-12`.
2. **(Documentation only)** Note in PROGRESS comment that the
   function does NOT clip outside `[0, 1]` — MATLAB returns the
   polynomial value at any X.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-08
- Notes: **Auditor's "no major gap" + "MATLAB doesn't clip" both
  WRONG.** Probe revealed MATLAB R2025b clips outside [0, 1]:
  meyeraux(-0.5) = 0, meyeraux(2) = 1. Numkit was applying the
  raw polynomial (returning 6.0625 and -208 respectively) — a
  real bug.

  Fix: add `if (v <= 0.0) return 0.0;` and `if (v >= 1.0) return
  1.0;` guards in the per-element lambda. Auditor recommendation
  #2 ("note that the function does NOT clip") was based on the
  same wrong probe — REVERSED in this fix.

  Spec extended from 4 to 13 fingerprints (vector + matrix +
  out-of-support clipping + mixed). Parity OK numkit ↔ MATLAB
  at tol=1e-12. Octave doesn't clip (matches numkit's old buggy
  behavior — Octave bug); we follow MATLAB. 6 TEST_F gtest +
  smoke documenting the bug fix.
