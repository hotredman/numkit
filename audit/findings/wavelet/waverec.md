# wavelet/waverec — ТЗ for completion

**Status:** open
**Priority:** **critical**
**Effort:** small (cascade from `idwt`)
**Audited at commit:** 0e895fe
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/wavelet/src/dwt/multilevel.cpp:91` (`waverec`)
- Adapter: `libs/wavelet/src/dwt/multilevel.cpp:232` (`waverec_reg`)
- Spec: `tools/parity/specs/waverec.json`
- What works today:
  - `x = waverec(c, l, wname)` — round-trips `wavedec` (≤ 1e-11 over
    4 levels), but only because `wavedec` and `waverec` are paired
    via numkit's own dwt/idwt convention

## MATLAB R2025b — actual behavior

Documented signatures (`help waverec`):

- `x = waverec(c, l, wname)`
- `x = waverec(c, l, LoR, HiR)`
- `x = waverec(___, Mode=extmode)`

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | reconstruction from MATLAB-computed `(c, l)` | exact x recovery | numkit's reconstruction expects numkit-flavour `c` (different downsample alignment) — would NOT recover x correctly from MATLAB-computed `(c, l)` | **critical** |
| 2 | `(LoR, HiR)` custom filters | runs with supplied filter pair | not supported | high |
| 3 | `Mode=extmode` N-V | matches wavedec's mode | silently ignored | high |

## Reference table (from probe)

Inputs: `x = [1..16]'`, `[c, l] = wavedec(x, 3, 'db2')`

| Inputs | MATLAB | numkit |
|---|---|---|
| `waverec(c, l, 'db2')` round-trip (numkit-paired) | first 6 = `[1 2 3 4 5 6]`, max diff `~1e-11` | first 6 = `[1 2 3 4 5 6]` ✅ self-paired |
| Same `(c, l)` values fed across implementations | wouldn't round-trip — different conventions | n/a — wouldn't round-trip |

## Recommended fixes

1. **Land jointly with `idwt` fix** — `waverec` is a thin
   `repeat(idwt, n)` loop; once `idwt` follows MATLAB synthesis,
   parity is automatic provided `wavedec` also follows MATLAB.
2. **Accept `(c, l, LoR, HiR)`** custom-filter form.
3. **Add `Mode=` N-V parsing.**
4. **Spec extension:** new fingerprint testing reconstruction from
   MATLAB-flavour `(c, l)` (re-record after dwt/idwt land).
   `tol = 1e-12`.

## Out of scope for this ТЗ

- 2-D `waverec2`.
