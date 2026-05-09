# builtin/replacebetween — ТЗ for completion

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
- Closed in commit: pending (lowercase alias added)
- Closed date: 2026-05-09
- Notes: Initial closure was DEFERRED with note 'undefined'. The function replaceBetween (camelCase, MATLAB canonical) was already implemented in libs/builtin/src/language/strings/strings.cpp and registered. Fix: also register a lowercase alias 'replacebetween' so both spellings work. Bit-identical with MATLAB R2025b on probed input.
