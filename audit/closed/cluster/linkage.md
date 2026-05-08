# stats.cluster/linkage — ТЗ for completion

**Status:** closed
**Priority:** medium
**Effort:** small
**Audited at commit:** b2f133b
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/cluster/...` (`linkage`)
- Spec: `tools/parity/specs/linkage.json`
- Methods all supported: single, complete, average, weighted,
  centroid, median, ward.
- Merge distances match MATLAB.
- **Tie-breaking convention differs** — when multiple pairs have
  the same merge distance, numkit picks a different pair than
  MATLAB. Final dendrogram structure is equivalent but not
  bit-identical.

## MATLAB R2025b — actual behavior

`Z = linkage(Y[, method[, metric]])`. Returns `(N-1) × 3` matrix
where each row is `[clusterA, clusterB, distance]`. Tie-breaking
follows MATLAB's internal heap order — typically picks the
lowest-index pair when distances tie.

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | tie-breaking on equal distances | picks lowest-index (or stable heap order) | picks DIFFERENT pair (probe: single linkage Z(1,:) numkit `[1 2 1]` vs MATLAB `[7 8 1]` — both are 1.0-distance pairs) | medium (cluster STRUCTURE matches but row ordering differs) |
| 2 | `linkage(Y, method, metric)` 3-arg form | computes pdist with `metric`, then linkage | needs probe — likely supports | unknown |
| 3 | `linkage(X, ...)` direct from data matrix (no pdist precompute) | also supports | needs probe | unknown |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `linkage(pdist(X), 'single')` Z(1,:) | `[7 8 1]` | `[1 2 1]` ❌ different pair (same distance) |
| `linkage(pdist(X), 'single')` Z(end,:) | `[9 14 6.40]` | `[13 14 6.40]` ❌ different cluster ID |
| `linkage(pdist(X), 'ward')` Z(end,:) | `[11 14 16.95]` | identical ✅ (no ties at this height) |

## Recommended fixes

1. **Match MATLAB tie-breaking:** in the heap-based pair-selection,
   prefer the pair with smaller `min(i, j)` index when distances
   tie (and smaller `max(i, j)` as secondary tiebreak). This makes
   the row ordering match MATLAB exactly.
2. **Verify `(Y, method, metric)` 3-arg form** is supported (3rd
   arg routed into pdist call).
3. **Spec extension** — add fingerprint with tied-distance dataset
   and a unique-distance dataset, plus method sweep. `tol = 1e-9`
   on distances; exact match on cluster indices.

## Out of scope for this ТЗ

- The downstream `cluster()` and `cophenet()` results are
  invariant under tie-break differences — they match MATLAB.

## Closed
- Closed in commit: TBD
- Closed date: 2026-05-08
- Notes: Three gaps closed:
  1. Tie-break alignment — switched `<` to `<=` in the inner
     find-min loop. With ascending i,j scan that prefers the
     last-scanned (largest pair lex) — verified to match MATLAB
     R2025b on the probe dataset and a separate tie-heavy 2x4-grid
     dataset.
  2. 3-arg `linkage(X, method, metric)` form — was silently
     ignoring the metric (always defaulted to euclidean). Now
     routes the metric (and optional p) to the implicit pdist call.
     Public `linkage()` got a 4-arg overload with metric+p; 2-arg
     wrapper preserved for back-compat.
  3. `linkage(X)` direct from N×D data matrix — already worked.

  Plus PMR refactor of `distance.cpp` (helpers `invert_small`,
  `data_cov`, `mahal_distance`, `row_distance`, `read_row` switched
  to raw-pointer style; pdist/pdist2/pdist2_topk/mahal callers use
  ScratchArena + ScratchVec for all temporaries; pdist2_reg's
  `filtered` Value vector is now `std::pmr::vector<Value>` on the
  arena). Zero raw `std::vector` left in the file.

  Bit-identical to MATLAB ↔ Octave on 14 fingerprints across all 7
  linkage methods + 3-arg metric form.

