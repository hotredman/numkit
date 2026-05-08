# builtin/psi — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 03244f9
**Audit date:** 2026-05-06

## Gaps

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | precision | `psi(2) = 0.4227843350984675` | `psi(2) = 0.4227843350897080` (diff ~9e-12) | low (precision) |

The digamma function values are close but not exact; numkit's
implementation is precise to ~11 digits vs MATLAB's ~16. For most
applications this is irrelevant.

## Recommended fixes

1. **Verify the asymptotic-series cutoff** in numkit's psi
   implementation; switching to a higher-order expansion (or using
   reflection identities for small arguments) typically closes the
   gap to 1-ULP.
2. **Spec extension** — fingerprint covering small/medium/large
   argument regimes. `tol = 1e-10` for current accuracy;
   tighten after fix.

## Out of scope for this ТЗ

- N/A.
