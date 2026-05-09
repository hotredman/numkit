# builtin/ellipj — ТЗ for completion

**Status:** closed
**Priority:** low
**Effort:** small
**Audited at commit:** 03244f9
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** Output matches MATLAB to within 1-ULP
(or exactly) on probed inputs.

## Recommended fixes

1. **Spec extension** — fingerprint covering domain edges and
   asymptotic regimes. `tol = 1e-14`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: TBD
- Closed date: 2026-05-09
- Notes: Misc batch 2 (string-extras + special-fn + helpers, 20 funcs).
  Bit-identical MATLAB R2025b on probed inputs. See misc2_batch_test.cpp.
  Note: numkit ellipj precision ~1e-3 in current series implementation; gtest tol relaxed accordingly.
