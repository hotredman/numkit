# signal/interpft — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 9bce106
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/signal/src/transforms/...` (`interpft`)
- Spec: `tools/parity/specs/interpft.json`
- `Y = interpft(X, n)` — matches MATLAB exactly on probed input.

## MATLAB R2025b — actual behavior

- `Y = interpft(X, n)` — band-limited interpolation (FFT-based)
- `Y = interpft(X, n, dim)` — along dim

## Gaps (numkit vs MATLAB)

| # | Gap | Severity |
|---|---|---|
| 1 | `dim` arg | likely not supported | low |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `interpft([1:8]', 16)` head | `[1 0.473 2 2.969 3 3.301 4 4.5]` | identical ✅ |

## Recommended fixes

1. **Add `dim` arg** support.
2. **Spec extension** — fingerprint for matrix input + dim.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-08
- Notes: Auditor flagged `dim` arg as "likely not supported" but
  numkit already implements it — confirmed via probe that
  `interpft(M, n, dim)` works for both dim=1 (column-wise, default)
  and dim=2 (row-wise) and matches MATLAB exactly. Pure spec
  coverage; no impl change needed.

  Spec extended from 1 to 11 fingerprints (vector originals at
  integer steps + matrix dim=1 + matrix dim=2). Parity OK numkit ↔
  MATLAB ↔ Octave at tol=1e-12. 3 new TEST_F (vector preservation,
  matrix dim=1, matrix dim=2) added on top of existing 2 C-API
  tests (length / pure-sinusoid). Smoke added.
