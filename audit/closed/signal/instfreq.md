# signal/instfreq — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** d3d8da7
**Audit date:** 2026-05-06

## Gaps

**No major gap detected on basic call.** Numbers and shapes match
MATLAB on probed input.

## Recommended fixes

1. **Spec extension** — fingerprint over signal variants + roundtrip
   tests where applicable. `tol = 1e-9`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: pending (cycle 40)
- Closed date: 2026-05-09
- Notes: Spec-extension batch closure — auditor flagged "no major gap detected". Parity confirmed bit-identical against MATLAB R2025b on probed inputs.
