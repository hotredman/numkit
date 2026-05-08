# stats.dist/exppdf — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** small
**Audited at commit:** 301e5a5
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/distributions/exponential.cpp` (`exppdf`)
- Spec: `tools/parity/specs/exppdf.json`
- Adapter throws on `exppdf(x)` (single arg) — requires explicit
  `mu`.

## MATLAB R2025b — actual behavior

- `y = exppdf(x)` — default `mu = 1`
- `y = exppdf(x, mu)` — explicit mean

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `exppdf(x)` 1-arg form | uses default `mu = 1` | adapter throws "exppdf: requires (x, mu)" | medium |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `exppdf(1, 2)` | `0.3032653299` | identical ✅ |
| `exppdf(1)` | `0.3678794412` | THROWS |
| `exppdf(1, 0)` (mu<=0) | `NaN` | `nan` ✅ |

## Recommended fixes

1. **Default `mu = 1`** when only 1 arg supplied.
2. **Spec extension** — add fingerprint with default-mu call.
   `tol = 1e-12`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-07
- Notes: Added 1-arg form `exppdf(x)` with default mu=1. 10
  fingerprint spec covers default + non-default mu + vector x +
  x<0 → 0 + mu<=0 → NaN. 5 TEST_F gtest + smoke. Parity OK
  numkit ↔ MATLAB.
