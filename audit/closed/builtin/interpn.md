# builtin/interpn — ТЗ for completion

**Status:** closed
**Priority:** low
**Effort:** small
**Audited at commit:** 3cb06a1
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** PROGRESS shows `correctness=OK`.

## Recommended fixes

1. **Spec extension** — fingerprint covering edge cases.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: pending (cycle 6)
- Closed date: 2026-05-09
- Notes: Initial closure was DEFERRED -- same root cause as interp3. Fixed by the same readGridAxis rewrite. interpn dispatches to interp3 internally; bit-identical with MATLAB R2025b on ndgrid form.

## Re-confirmed -- 2026-05-09 (Phase 0a-2)

Re-probed via `python tools/parity/run_parity.py tools/parity/specs/interpn.json`:
- numkit: 7.5 (linear interp at center of 5x5x5 cube)
- matlab: 7.5
- octave: 7.5
- correctness=OK across all 3 engines

Status: NOT actually deferred -- the earlier "DEFERRED" mention in
the Notes is past-tense ("Initial closure was DEFERRED, fixed by..."),
which my filter false-positively treated as still-deferred. interpn
has been working correctly since cycle 6's readGridAxis rewrite.
