# signal/dct — ТЗ for completion

**Status:** open
**Priority:** **high**
**Effort:** medium
**Audited at commit:** 9bce106
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/signal/src/transforms/...` (`dct`)
- Spec: `tools/parity/specs/dct.json`
- 1-D DCT-II matches MATLAB exactly.

## MATLAB R2025b — actual behavior

- `Y = dct(X)` — 1-D Type-II
- `Y = dct(X, n)` — pad/truncate to length n
- `Y = dct(X, n, dim)` — along dim
- `Y = dct(___, 'Type', t)` — t ∈ {1, 2, 3, 4} for DCT-I/II/III/IV

For matrix `X`, DCT operates **column-wise** by default.

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `dct(M)` matrix input | column-wise DCT | numkit treats as flattened (or uses just first column) — `dct([1 5; 2 6; 3 7; 4 8])` col 1 returns same as `dct([1:8])` instead of `dct([1 2 3 4]')` | **high** |
| 2 | `dct(X, 'Type', 1)` | DCT-I (different formula) | silently runs Type-II | **high** |
| 3 | `dct(X, n)` length override | pad/truncate | likely not supported | medium |
| 4 | `dct(X, n, dim)` along dim | per-dim transform | likely not supported | medium |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `dct([1:8]')` | `[12.728 -6.442 0 -0.673 0 -0.201 0 -0.051]` | identical ✅ |
| `dct([1 5; 2 6; 3 7; 4 8])` col 1 | `[5 -2.230 0 -0.159]` | `[12.728 -6.442 0 -0.673]` ❌ (= 1-D DCT of flattened) |
| `dct(x, 'Type', 1)` | `[12.610 -6.172 0.996 -1.462 0.996 -1.104 0.996 -0.737]` | same as Type-II output ❌ |

## Recommended fixes

1. **Implement matrix column-wise DCT:** detect 2-D input and run
   the 1-D transform per column. Optionally accept the `dim` arg to
   pick row-wise.
2. **Add `'Type', t` parser** — implement Type-I (length-N+1
   different formula), Type-III (inverse of Type-II), Type-IV.
3. **Length override `n`:** when `args[1]` is a numeric scalar, pad
   `X` with zeros (or truncate) to length `n` before transform.
4. **Spec extension** — fingerprint over (1-D, 2-D, length-override,
   each Type). `tol = 1e-12`.

## Out of scope for this ТЗ

- N-D dct — covered conceptually by the dim arg.

## Closed (partial)
- Closed in commit: PENDING
- Closed date: 2026-05-08
- Notes: Closed gaps #1, #3, #4. Deferred gap #2 (DCT-I/III/IV
  via 'Type'). Joint closure with audit/closed/signal/idct.md.

  **Implemented:**
  1. **Matrix column-wise** (gap #1): added `dct(mr, x, n, dim)`
     overload that iterates over columns (default) or rows
     (dim=2), runs the existing 1-D core per line. Fixes silent
     "flat as N-vector" output to per-column behaviour matching
     MATLAB.
  2. **Length override** (gap #3): adapter parses `args[1]` as
     `n`; per-line extraction pads with zeros / truncates to n
     before the 1-D transform.
  3. **Dim arg** (gap #4): `args[2]` selects axis; resolveDim()
     handles the "first non-singleton" default.
  4. **Type sanity**: `'Type'` N-V parsed but values other than 2
     now explicitly error (was silently doing Type-II — worst kind
     of divergence).

  **Deferred — gap #2 ('Type' values 1/3/4):** DCT-I uses a
  different formula (length-N+1 mirror), Type-III is the inverse
  of Type-II (currently exposed as `idct`), Type-IV is the
  Discrete Cosine of fourth kind. Implementing each requires a
  separate kernel; medium-effort follow-up. Spec covers Type=2
  only.

  Cross-lib fix: `libs/image/src/transform/transform.cpp` had
  `&numkit::signal::dct` resolving the now-ambiguous overload set
  for `apply_along_columns(..., &dct)`. Added explicit function-
  pointer typedef to disambiguate.

  Spec extended from 1 to 16 fingerprints (vector + matrix dim=1
  + length-override truncate/pad + dim=2 row-wise). Parity OK
  numkit ↔ MATLAB at tol=1e-12. Octave's dct lacks the pad-to-n
  branch (its own limitation); we follow MATLAB. 6 new TEST_P
  gtests + smoke. Sanity check: all 20 DCT-domain tests pass.
