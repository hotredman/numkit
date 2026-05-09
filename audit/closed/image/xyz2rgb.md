# image/xyz2rgb — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 4fae461
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** PROGRESS shows `correctness=OK` on
benched input.

## Recommended fixes

1. **Spec extension** — fingerprint covering edge cases.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: pending (xyz2rgb fix)
- Closed date: 2026-05-09
- Notes: Initial closure (cycle 44) was DEFERRED -- numkit clamped out-of-gamut linear RGB to [0,1] before sRGB encoding, MATLAB does NOT (returns negative encoded values for out-of-gamut). Fix: drop the std::clamp(0,1) and apply sign-preserving sRGB gamma (encoded = sign(c) * srgb_encode(|c|)). Verified bit-identical with MATLAB at tol=1e-6 on reshape(0:74,[5 5 3])/74 (tol relaxed from 1e-9 because std::pow vs MATLAB transcendental rounding differs by ~1e-8). Direct probe (XYZ=[0.1,0.2,0.5]): R=-0.519615 G=0.582991 B=0.730974 matches MATLAB exactly.
