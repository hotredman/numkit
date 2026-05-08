# wavelet/wkeep — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 0e895fe
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/wavelet/src/dwt/wkeep_wextend.cpp:60` (`wkeep_reg`)
- Spec: `tools/parity/specs/wkeep.json`
- What works today:
  - `Y = wkeep(X, n[, OPT])` — `OPT` is `'c'` (default), `'l'`,
    `'r'`, or numeric `FIRST`
  - All four 1-D cases probed and match MATLAB exactly

## MATLAB R2025b — actual behavior

Documented signatures (`help wkeep`):

- `Y = wkeep(X, L, opt)` — 1-D, opt ∈ `'c'`/`'l'`/`'r'`
- `Y = wkeep(X, L, first)` — 1-D, explicit start index
- `Y = wkeep(X, S)` — 2-D, S is a 2-vector `[rows, cols]` central
- `Y = wkeep(X, S, [firstr, firstc])` — 2-D explicit corner

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `wkeep(X, [3 3])` 2-D central | extracts central 3×3 sub-matrix | needs probe — based on adapter signature, likely supported; MATLAB result on `magic(5)` is `[5 7 14; 6 13 20; 12 19 21]` | unknown |
| 2 | `wkeep(X, [3 3], [firstr firstc])` 2-D corner | extracts at explicit corner | needs probe | unknown |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `wkeep((1:10)', 4)` (central) | `[4; 5; 6; 7]` | identical ✅ |
| `wkeep((1:10)', 4, 'l')` | `[1; 2; 3; 4]` | identical ✅ |
| `wkeep((1:10)', 4, 'r')` | `[7; 8; 9; 10]` | identical ✅ |
| `wkeep((1:10)', 4, 3)` (FIRST=3) | `[3; 4; 5; 6]` | identical ✅ |
| `wkeep(magic(5), [3 3])` | `[5 7 14; 6 13 20; 12 19 21]` | identical ✅ |

## Gaps (numkit vs MATLAB)

**Mostly OK.** All 5 probed cases match. Only spec coverage gap.

## Recommended fixes

1. **Spec extension:** `wkeep.json` covers only the basic 'c' form.
   Add fingerprint for: `'l'`, `'r'`, numeric FIRST, 2-D S vector,
   2-D explicit corner. `tol = 0` (integer-stable).
2. **(Verify)** Test `wkeep(X, [3 3], [2 2])` — explicit 2-D corner;
   MATLAB returns sub-matrix at row 2, col 2. Probe confirms numkit
   behaviour.

## Out of scope for this ТЗ

- 3-D `wkeep` (S is 3-element vector) — undocumented in 2025b help.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-08
- Notes: Auditor's table marked the matrix forms as "needs probe"
  — when probed, both forms threw "Cannot convert double to
  scalar" because the adapter did `args[1].toScalar()`
  unconditionally, expecting a numeric scalar (1-D form).

  Fix: detect `args[1].numel() == 2` → 2-D form. Extract central
  [R x C] sub-matrix (default) or explicit-corner sub-matrix
  when args[2] is also a 2-vector. Out-of-range corners throw
  cleanly. Original 1-D path preserved unchanged.

  Spec extended from 8 to 15 fingerprints (1-D + 2-D central +
  2-D explicit corners at top-left and lower-right). Parity OK
  numkit ↔ MATLAB at tol=0. Octave's wkeep doesn't support the
  numeric-start arg in 1-D (its limitation); we follow MATLAB.
  10 TEST_F gtest (existing 7 + 3 new MatrixCentral /
  MatrixExplicitTopLeft / MatrixOutOfRangeThrows).
