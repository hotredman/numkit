# stats.dist/fpdf — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** e580a5c
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** Numbers match MATLAB exactly.

## Recommended fixes

1. **Spec extension** — fingerprint over parameter sweeps + edge
   cases. `tol = 1e-12`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-07
- Notes: ТЗ said "no major gap" but spec extension surfaced edge
  mismatch at x=0:
    numkit: always 0 (special-case `xi <= 0 → 0`)
    MATLAB: depends on v1 → finite if v1==2 (= 1 for v2=10),
            +Inf if v1<2, 0 if v1>2 (limit of x^(v1/2-1) as x→0+)
  Fixed by splitting x==0 case in fpdf into the three regimes.
  Octave still returns 0 (simplified) — we follow MATLAB.
  13-fingerprint spec; 7 TEST_F gtest + smoke .m. Parity OK
  numkit ↔ MATLAB at tol=1e-12.
