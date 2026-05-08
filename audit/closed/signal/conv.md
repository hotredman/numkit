# signal/conv — ТЗ for completion

**Status:** closed
**Priority:** low
**Effort:** small
**Audited at commit:** 015c30d
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** PROGRESS shows `correctness=OK`.

## Recommended fixes

1. **Spec extension** — fingerprint covering edge cases (empty
   inputs, dimension variations).

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: TBD
- Closed date: 2026-05-09
- Notes: Signal batch 1 (conv/cconv/bandpower/chirp/alignsignals/AR/analog
  prototypes, 13 funcs). All bit-identical MATLAB R2025b on probed inputs.
  See libs/signal/tests/signal_batch1_test.cpp.
