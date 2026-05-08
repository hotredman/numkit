# stats.cluster/kmeans — ТЗ for completion

**Status:** closed
**Priority:** low
**Effort:** small
**Audited at commit:** b2f133b
**Audit date:** 2026-05-06

## Gaps

Auditor "no major gap" claim was wrong. Re-probe surfaced:

| # | Branch / case | MATLAB does | numkit did | Severity |
|---|---|---|---|---|
| 1 | 4-output `[idx, C, sumd, D]` | N×K squared-dist matrix | only 3-output | medium |
| 2 | N-V keys (case sensitivity) | accepts mixed case | matched only literal 'MaxIter' / 'Replicates' | medium |
| 3 | `'Distance'` / `'Start'` N-V | parsed | silently dropped | low |
| 4 | `'Display'` / `'EmptyAction'` / `'OnlinePhase'` | accepted (with side effects) | silently dropped (acceptable) | low |

## Recommended fixes

1. Add 4-output D = N×K squared distances from each row to each centroid.
2. Case-insensitive N-V key matching.
3. Parse 'Distance' (accept 'sqeuclidean' only — error on cityblock /
   cosine / hamming until separate algorithms are wired) + 'Start'
   (accept 'plus' only).
4. Silently accept 'Display' / 'EmptyAction' / 'OnlinePhase' / 'Options'.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: TBD
- Closed date: 2026-05-08
- Notes: All 4 gaps fixed. New `kmeans_full()` returns the optional
  4th D output (squared euclidean N×K). Adapter is now case-
  insensitive on N-V keys and rejects unsupported `Distance` /
  `Start` values with a clear message instead of silent fallback.

  PMR refactor: kmeans.cpp scratch buffers (Xv, kmeanspp_init's C+dist,
  Lloyd's idx/C/sumd/counts/Cnext) all on ScratchArena + ScratchVec.
  Zero raw `std::vector` left in the file.

  Parity OK numkit ↔ MATLAB ↔ Octave on 14 fingerprints
  (label-permutation-invariant — RNG init differs).

