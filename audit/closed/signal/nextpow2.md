# core/nextpow2 — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 9bce106
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/builtin/src/...` (`nextpow2`)
- Spec: `tools/parity/specs/nextpow2.json`
- Element-wise; matches MATLAB on probed inputs.

## MATLAB R2025b — actual behavior

`P = nextpow2(N)` — smallest p such that `2^p >= |N|`.
- `nextpow2(0)` = `0` per probe (older MATLAB returned `-Inf`;
  R2025b returns 0).

## Gaps (numkit vs MATLAB)

**No major gap detected.**

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `nextpow2([1 5 100 1024 1025])` | `[0 3 7 10 11]` | identical ✅ |
| `nextpow2(0)` | `0` | `0` ✅ |

## Recommended fixes

1. **Spec extension** — add fingerprint with negative inputs,
   non-integer inputs (MATLAB rounds towards-zero before applying),
   complex inputs (uses `abs`). `tol = 0`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-08
- Notes: Auditor "no major gap" wrong on three counts. Real bugs
  caught by spec extension:
  1. **Complex input threw** with `Cannot convert complex to
     double`. MATLAB uses `|z|`. Fixed: handle complex path with
     `std::abs(Complex)`.
  2. **NaN input returned 0** (silent fall-through to `n <= 0`
     branch). MATLAB returns NaN. Fixed: explicit `isnan` check.
  3. **±Inf input returned 0**. MATLAB returns +Inf for both.
     Fixed: explicit `isinf` check.

  Refactored scalar element rule into `nextpow2Element` helper used
  by both scalar and vector paths (real and complex).

  Spec extended from 3 fingerprints to 14 (vector + 0/-/+/fractional
  + complex + NaN/±Inf). Parity OK numkit ↔ MATLAB at tol=0. Octave
  rejects complex input (its own limitation); we follow MATLAB.
  9 TEST_F gtest (existing 4 + 5 new) + smoke.
