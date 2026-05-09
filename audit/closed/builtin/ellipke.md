# builtin/ellipke — ТЗ for completion

**Status:** closed
**Priority:** low
**Effort:** small
**Audited at commit:** 03244f9
**Audit date:** 2026-05-06

## Gaps

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | E precision | `E = 1.3506438810476753` | `E = 1.3506438810475703` (diff ~1e-13) | low (precision) |

Complete elliptic integral of the 2nd kind has a small precision
gap; K matches exactly.

## Recommended fixes

1. **Tighten the E series cutoff** in the AGM-based implementation.
2. **Spec extension** — `tol = 1e-12` currently, target `1e-14`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: TBD
- Closed date: 2026-05-09
- Notes: Misc batch 2 (string-extras + special-fn + helpers, 20 funcs).
  Bit-identical MATLAB R2025b on probed inputs. See misc2_batch_test.cpp.
