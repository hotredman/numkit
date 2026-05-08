# stats.cluster/dbscan — ТЗ for completion

**Status:** closed
**Priority:** low
**Effort:** small
**Audited at commit:** b2f133b
**Audit date:** 2026-05-06

## Gaps

Auditor said "no major gap". Re-probe surfaced FOUR real bugs:

| # | Branch / case | MATLAB does | numkit did | Severity |
|---|---|---|---|---|
| 1 | noise label | `-1` | `0` (with comment "MATLAB convention" — wrong) | medium |
| 2 | `'Distance'` N-V | required keyword | only positional 4th arg accepted | medium |
| 3 | `'Distance', 'precomputed'` | accepts N×N D matrix | not supported | medium |
| 4 | `'P'` Minkowski exponent | N-V | not supported | low |

## Recommended fixes

1. Change noise label from 0 to -1.
2. Parse `'Distance', metric` N-V (and tolerate the legacy positional
   form for back-compat).
3. Add 'precomputed' metric — when set, X is the N×N pairwise
   distance matrix.
4. Add 'P' N-V for Minkowski; widen the local Metric enum to include
   Minkowski / Cosine / Hamming / Jaccard.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: TBD
- Closed date: 2026-05-08
- Notes: All four gaps fixed. Noise → -1; `'Distance'` N-V keyword;
  precomputed mode; Minkowski `'P'`. Local Metric enum extended to
  cover 9 metrics. Parity OK numkit ↔ MATLAB across 18
  fingerprints (Octave does not ship `dbscan`, reports
  `correctness=N/A` from its side).

