# stats.cluster/clusterdata — ТЗ for completion

**Status:** closed
**Priority:** low
**Effort:** small
**Audited at commit:** b2f133b
**Audit date:** 2026-05-06

## Gaps

Auditor's "no major gap" claim was wrong. Re-probe surfaced 4 bugs:

| # | Branch / case | MATLAB does | numkit did | Severity |
|---|---|---|---|---|
| 1 | `clusterdata(X, c)` scalar shortcut, c < 2 | cutoff (inconsistency) | maxclust(0) → all singletons | medium |
| 2 | N-V keys (case sensitivity) | `'MaxClust'` / `'Cutoff'` etc. | only lowercase matched | medium |
| 3 | `'Distance'` N-V | configurable metric | hardcoded euclidean | medium |
| 4 | `'Depth'` N-V | inconsistency-depth pass-through | unwired | low |

## Recommended fixes

1. Scalar shortcut: c >= 2 → maxclust, 0 < c < 2 → cutoff
   (inconsistency criterion).
2. Case-insensitive N-V key matching.
3. Wire 'Distance' N-V into `pdist()` call inside clusterdata.
4. Wire 'Depth' N-V into `cluster_from_linkage()` inconsistency calc.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: TBD
- Closed date: 2026-05-08
- Notes: All 4 gaps fixed in `clusterdata_reg`. Public
  `clusterdata()` extended with `distance_metric` + `p` parameters.
  Plus PMR refactor of `linkage.cpp` — every scratch buffer
  (`linkage`, `cluster_from_linkage`, `cophenet_full`,
  `inconsistent`) now uses `ScratchArena` + `ScratchVec` per
  per-call arena (was raw `std::vector` for ~12 sites). 7 fingerprints
  parity OK numkit ↔ MATLAB ↔ Octave (all label-permutation-
  invariant).

