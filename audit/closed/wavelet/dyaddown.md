# wavelet/dyaddown — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** small
**Audited at commit:** 0e895fe
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/wavelet/src/dwt/dyad.cpp:46` (`dyaddown_reg`)
- Spec: `tools/parity/specs/dyaddown.json`
- What works today:
  - `Y = dyaddown(X[, ODD])` — vector input, ODD=0 default
  - 2-D input — needs probe

## MATLAB R2025b — actual behavior

Documented signatures (`help dyaddown`):

- `Y = dyaddown(X)` — vector: ODD=0 default; matrix: column-wise
- `Y = dyaddown(X, EVENODD)` — ODD=0 (even) / 1 (odd)
- `Y = dyaddown(___, 'type')` — 'c' (column, default for matrix),
  'r' (row), 'm' (matrix — both)

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `dyaddown(M, 0, 'c')` 'type' arg | filter rows in column direction | needs probe; adapter likely throws on 3rd char arg | high |
| 2 | `dyaddown(M, 0, 'r')` | downsample row dimension | needs probe; likely throws | high |
| 3 | `dyaddown(M, 0, 'm')` | downsample both | needs probe; likely throws | high |
| 4 | matrix vs vector default | matrix: column-wise (`'c'` default) | needs verification | medium |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `dyaddown([1:8]')` (vector, default ODD=0) | `[2 4 6 8]'` | `[2 4 6 8]'` ✅ |
| `dyaddown([1:8]', 1)` (ODD=1) | `[1 3 5 7]'` | `[1 3 5 7]'` ✅ |
| `dyaddown(M, 0, 'c')` (M=[1 4;2 5;3 6;4 7]) | `[2 5; 4 7]` | needs probe — adapter currently treats `args[2].toScalar()` |
| `dyaddown(M, 0, 'r')` | `[1 4]` (row stride) | needs probe |
| `dyaddown(M, 0, 'm')` | scalar `5` | needs probe |

## Recommended fixes

1. **Adapter rewrite to accept the `'type'` 3rd arg.** Current
   adapter at `libs/wavelet/src/dwt/dyad.cpp:46` likely treats
   `args[2]` as a numeric flag; switch to:
   ```
   int evenodd = 0;
   char type = 'c';
   for (size_t i = 1; i < args.size(); ++i) {
     if (args[i].isChar() || args[i].isString())
       type = lower(args[i].toString())[0];
     else
       evenodd = (int)args[i].toScalar();
   }
   ```
2. **Implement column / row / matrix downsampling:** based on the
   `type` flag, slice along rows, columns, or both. For type='m',
   apply column-then-row.
3. **Default for matrix input:** when `args.size() == 1` and `X` is
   a matrix, MATLAB defaults to `'c'` (column-wise).
4. **Spec extension:** fingerprint over vector/matrix × ODD={0,1} ×
   type ∈ {'c','r','m'}. `tol = 0`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-08
- Notes: Joint closure with audit/closed/wavelet/dyadup.md.

  **Bug:** matrix path silently flattened to a 1-D vector and
  ignored the 3rd `type` arg. `dyaddown(M, 0)` returned numel/2
  flat values instead of MATLAB's per-axis downsampling.

  **Fix:** new `parseDyadArgs` helper handles `(X, evenodd[,
  type])` in any positional order, then a 2-D matrix branch
  applies the requested 'c' / 'r' / 'm' downsample (column-major
  data layout) with proper output sizing.

  Spec extended from 7 to 31 fingerprints (vector + matrix ×
  ODD={0,1} × type ∈ {c, r, m}). Parity OK numkit ↔ MATLAB at
  tol=0. Octave doesn't ship dyaddown; we follow MATLAB. 8 TEST_F
  gtest (existing 4 + 4 new). 19 wavelet-suite dyad tests still
  pass.
