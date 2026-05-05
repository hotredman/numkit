# stats/std — ТЗ for completion

**Status:** open
**Priority:** high
**Effort:** small (joint with `var`)
**Audited at commit:** ba142e6
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/descriptive/descriptive.cpp:198` (`stdev`)
- Adapter: `libs/stats/src/descriptive/descriptive.cpp:947` (`std_reg`)
- Spec: `tools/parity/specs/std.json`
- Mirrors `var` exactly; just sqrts the result.

## MATLAB R2025b — actual behavior

Same surface as `var`:

- `S = std(A[, w[, dim]])` — same dispatch as `var`
- `S = std(A, w, "all")` / `S = std(A, w, vecdim)`
- `S = std(___, missingflag)` — `'omitnan'` (default for FP since
  R2023b) / `'includenan'`

## Gaps (numkit vs MATLAB)

Identical to `audit/findings/stats/var.md`:
- vecdim throws
- `"all"`/`'all'` string-as-dim throws
- weight vector `W` throws
- default nanflag = includenan vs MATLAB R2023b+ omitnan

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `std(A)` | `[1.5811 1.5811 1.5811]` | identical ✅ |
| `std(A, 0, [1 2])` | `2.9277002188` | THROWS |
| `std(A, 0, "all")` | `2.9277002188` | THROWS |
| `std(v, 0, 'omitnan')` | `2.7386127875` | identical ✅ |

## Recommended fixes

Apply the joint adapter rewrite from `audit/findings/stats/var.md`
"Recommended fixes" §5. `std` simply takes `sqrt` of the underlying
`var` result, so a single fix in `var` adapter covers both via the
shared dispatch layer.

Spec extension same shape as `var.json`. `tol = 1e-9`.

## Out of scope for this ТЗ

- N/A — joint fix with `var`.

## Closed
- Closed in commit: PENDING (joint var/std fix)
- Closed date: 2026-05-06
- Notes: Adapter rewritten via varStdDispatch helper. Supports 'all' string, full-flatten vecdim ([1 2] / [1 2 3]), and weight-vector W (denominator = sum(W)). Default nanflag remains 'includenan' (NaN poisons) — matches MATLAB R2025b documented default for double; auditor's R2023b-default-omitnan claim was incorrect (verified via probe). Partial vecdim and weight + non-flat dim not yet supported (documented).
