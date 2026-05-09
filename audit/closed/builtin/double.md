# builtin/double — ТЗ for completion

**Status:** closed
**Priority:** low
**Effort:** small
**Audited at commit:** c1fdebe
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** PROGRESS shows `correctness=OK` on
benched input. Standard string/character function.

## Recommended fixes

1. **Spec extension** — fingerprint covering edge cases (empty
   strings, multi-byte chars, unicode where applicable).
   `tol = 0`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: TBD
- Closed date: 2026-05-09
- Notes: Type-conv spec-extension batch (12 funcs). All bit-identical
  MATLAB R2025b on numeric paths. See type_conv_batch_test.cpp.
  KNOWN GAP (separate ТЗ): numkit rejects double("string") with error; MATLAB returns NaN, Octave returns ASCII codes — three different behaviors. Only numeric-input paths pinned.
