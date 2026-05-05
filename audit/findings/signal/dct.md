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
