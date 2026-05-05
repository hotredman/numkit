# wavelet/cmorwavf — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** small
**Audited at commit:** ddf4218
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/wavelet/src/shape/shape.cpp` (`cmorwavf`)
- Adapter: same file
- Spec: `tools/parity/specs/cmorwavf.json`
- What works today:
  - `[psi, x] = cmorwavf(LB, UB, N, fb, fc)` — explicit fb, fc only
  - Throws "requires (LB, UB, N, fb, fc)" when fb/fc not supplied

## MATLAB R2025b — actual behavior

Documented signatures (`help cmorwavf`):

- `[psi, x] = cmorwavf(lb, ub, n)` — defaults fb=1, fc=1
- `[psi, x] = cmorwavf(lb, ub, n, fb, fc)` — explicit

Complex Morlet: `ψ(t) = (1/√(π·fb))·exp(2πi·fc·t)·exp(-t²/fb)`.

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `cmorwavf(LB, UB, N)` 3-arg form (no fb/fc) | use defaults fb=1, fc=1 | throws "cmorwavf: requires (LB, UB, N, fb, fc)" | medium |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `cmorwavf(-5, 5, 8)` (default) real | `[7.84e-12 -1.47e-6 0.00356 -0.0754 -0.0754 0.00356 -1.47e-6 7.84e-12]` | THROWS |
| `cmorwavf(-5, 5, 8, 1.5, 1)` real | `[2.66e-8 -8.42e-5 0.0135 -0.0730 -0.0730 0.0135 -8.42e-5 2.66e-8]` | identical ✅ |

## Recommended fixes

1. **Default fb=1, fc=1** when only 3 args supplied. Trivial
   adapter change:
   ```cpp
   const double fb = (args.size() >= 4) ? args[3].toScalar() : 1.0;
   const double fc = (args.size() >= 5) ? args[4].toScalar() : 1.0;
   ```
2. **Spec extension** — add fingerprint for default-args call and
   non-default `(fb, fc)` combinations. `tol = 1e-12`.

## Out of scope for this ТЗ

- N/A.
