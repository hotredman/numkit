# builtin/uniquetol — ТЗ for completion

**Status:** open
**Priority:** medium (PROGRESS notes `correctness=MISMATCH` on bench)
**Effort:** small
**Audited at commit:** 6c0964f
**Audit date:** 2026-05-06

## Gaps

| # | Gap | Severity |
|---|---|---|
| 1 | Probe `uniquetol([1 1.001 2], 0.01)` matches MATLAB output `[1 2]` ✓ but PROGRESS bench (10k rounded vals at tol) flags MISMATCH — needs targeted probe to find the divergent input | medium |

## Recommended fixes

1. **Re-probe with the exact bench input** (10k values with rounded
   precision at tol) to find which entries diverge.
2. **Spec extension** after divergence is identified.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: pending (uniquetol fix)
- Closed date: 2026-05-09
- Notes: Initial closure (cycle 42) was DEFERRED -- numkit returned 501 unique values vs MATLAB 100 on probed input. Root cause: numkit used per-pair relative tolerance (tol*max(|x|,|y|)), MATLAB uses ABSOLUTE GLOBAL (tol*max(|A(:)|)). Fix: precompute dataScale = max(|A(:)|) once, then a candidate joins an existing cluster iff |v - rep| <= tol*dataScale. NaN handling: each NaN is its own cluster. Verified bit-identical with MATLAB R2025b on linspace(0,100,10000) probe (sum 4997.62, first 0, last 99.97).
