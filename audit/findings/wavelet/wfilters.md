# wavelet/wfilters — ТЗ for completion

**Status:** open
**Priority:** **critical**
**Effort:** medium
**Audited at commit:** 1c2df89
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/wavelet/src/filter/wfilters.cpp` (`wfilters`)
- Spec: `tools/parity/specs/wfilters.json`
- What works today:
  - `[Lo_D, Hi_D, Lo_R, Hi_R] = wfilters(wname)` — 4-output form
  - `F = wfilters(wname, type)` — `'d'` / `'r'` / `'l'` / `'h'`
  - Supported wavelets: haar, db1..db4, sym2, sym4, coif1 (per
    error message)

## MATLAB R2025b — actual behavior

- `[Lo_D, Hi_D, Lo_R, Hi_R] = wfilters(wname)`
- `F = wfilters(wname, 'd')` / `'r'` / `'l'` / `'h'`

Wavelet families and supported orders:
- `'haar'`, `'db1'..'db45'`, `'sym2'..'sym45'`, `'coif1'..'coif5'`
- biorthogonal `'biorN.M'`, reverse-biorthogonal `'rbioN.M'`
- `'fk4'..'fk22'` (Fejér-Korovkin)
- discrete Meyer `'dmey'`

Standard DWT convention (MATLAB R2025b):
- `Lo_D` is the **decomposition** (analysis) low-pass filter —
  matches the **time-reversed** scaling-function coefficients.
- `Lo_R` is the **reconstruction** (synthesis) low-pass filter —
  matches the **forward** scaling-function coefficients.
- `Lo_D = wrev(Lo_R)` (column-reversed).

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | **`Lo_D` vs `Lo_R` labels** | `Lo_D = wrev(Lo_R)` (analysis is time-reversed reconstruction) | numkit returns the SAME values but with **labels swapped**: numkit's `Lo_D` matches MATLAB's `Lo_R` | **critical — root cause of `dwt` value mismatch** |
| 2 | `Hi_D` / `Hi_R` similarly swapped | analysis QMF of Lo_R | numkit's labels also swapped | **critical** |
| 3 | wavelet family coverage | dozens of supported wnames | only haar/db1..db4/sym2/sym4/coif1 | high |
| 4 | `wfilters(wname, 'd')` 1-output form | returns 2×Lf matrix `[Lo_D; Hi_D]` | returns 1×Lf (only one of the pair) | medium |
| 5 | `wfilters(wname, 'l')` | returns `[Lo_D; Lo_R]` 2×Lf | returns 1×Lf | medium |

## Reference table (from probe)

Inputs: wname `'db2'`

| Output | MATLAB | numkit |
|---|---|---|
| `Lo_D` | `[-0.1294 0.2241 0.8365 0.4830]` | `[0.4830 0.8365 0.2241 -0.1294]` ❌ (= MATLAB's Lo_R) |
| `Hi_D` | `[-0.4830 0.8365 -0.2241 -0.1294]` | `[0.1294 0.2241 -0.8365 0.4830]` ❌ (= MATLAB's Hi_R reversed) |
| `Lo_R` | `[0.4830 0.8365 0.2241 -0.1294]` | `[-0.1294 0.2241 0.8365 0.4830]` ❌ (= MATLAB's Lo_D) |
| `Hi_R` | `[-0.1294 -0.2241 0.8365 -0.4830]` | `[0.4830 -0.8365 0.2241 0.1294]` ❌ |

Same pattern on `sym4`. Haar (length-2 symmetric) accidentally
matches because `wrev` is identity-up-to-sign on length-2.

## Recommended fixes

1. **Swap `Lo_D` / `Lo_R` labels** in the wfilters output. The
   simplest implementation:
   ```cpp
   const auto coeffs = scaling_filter(wname);  // current Lo_D
   Lo_R = coeffs;                                // forward
   Lo_D = wrev(coeffs);                         // time-reversed
   Hi_R = qmf(Lo_R);                            // existing rule
   Hi_D = wrev(Hi_R);                           // time-reversed
   ```
2. **This change cascades into `dwt`/`idwt`/`wavedec`/`waverec`**:
   the `dwt` analysis convolution uses `Lo_D`, so the value swap
   here directly fixes the cD/cA values numkit currently misreports
   (see `audit/findings/wavelet/dwt.md`).
3. **Extend wavelet family support:**
   - dbN for N=1..20 (Daubechies polynomial roots; literature
     tables are public-domain).
   - symN for N=2..20 (Symlet — variant of Daubechies).
   - coifN for N=1..5 (Coiflet).
   - biorN.M and rbioN.M (biorthogonal pairs).
   - dmey (discrete Meyer — 62-tap filter).
   - fkN (Fejér-Korovkin).
4. **Fix the 1-output `'d'` / `'l'` forms** to return 2×Lf matrices:
   - `'d'` ⇒ `[Lo_D; Hi_D]`
   - `'r'` ⇒ `[Lo_R; Hi_R]`
   - `'l'` ⇒ `[Lo_D; Lo_R]`
   - `'h'` ⇒ `[Hi_D; Hi_R]`
5. **Spec extension:** PROGRESS notes `correctness=OK` but the
   numbers diverge — verify `wfilters.json` against the swap-fixed
   output. Add fingerprint per-wavelet for haar / db2 / db4 / sym4
   / coif1 / bior2.2 / dmey. `tol = 1e-12`.

## Out of scope for this ТЗ

- The cascade fixes for `dwt`/`idwt`/`wavedec`/`waverec` are
  separately ticketed under `audit/findings/wavelet/dwt.md` etc.
- The 5-output form `[LoD,HiD,LoR,HiR,F] = wfilters(...)` (extra
  filter family info) — undocumented in 2025b.
