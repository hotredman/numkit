# stats/kstest2 — ТЗ for completion

**Status:** open
**Priority:** high
**Effort:** small
**Audited at commit:** 69fab7c
**Audit date:** 2026-05-05

## Текущая реализация

- Source: `libs/stats/src/test/hypothesis.cpp:428` (`kstest2`)
- Adapter: `libs/stats/src/test/hypothesis.cpp:1343` (`kstest2_reg`)
- Spec: **none**
- What works today:
  - `[h, p, D, cv] = kstest2(x, y[, alpha, tail])`
  - Asymptotic Smirnov-series for `p`/`cv`
  - `Tail` parsed as positional ('both'/'right'/'left')

## MATLAB R2025b — actual behavior

Documented signatures (`help kstest2`):

- `h = kstest2(x1, x2)`
- `h = kstest2(x1, x2, Name, Value)`
- `[h, p] = kstest2(___)` / `[h, p, ks2stat] = kstest2(___)`

Name-value: `Alpha`, `Tail` — `'unequal'` (default) / `'larger'` /
`'smaller'`. **No `cv` output** (MATLAB's signature stops at 3 outs).

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | Tail names `'larger'`/`'smaller'` | maps to D⁺/D⁻ | unrecognised ⇒ `Both` (silent fallback) | high |
| 2 | `'Alpha', 0.01` N-V | uses α=0.01 | likely ignored (same N-V bug) | high |
| 3 | 4th output `cv` | does NOT exist | numkit produces it as a 4th return | medium (extra output is harmless to scripts using only first 3, but creates an API divergence) |
| 4 | `p` precision | small samples may use exact dist | always asymptotic; e.g. n=7 each: numkit p=0.99999970 vs MATLAB p=0.99999518 | low |

## Reference table (from probe)

Inputs:
```
x = [1.2 2.4 3.1 4.5 5.0 6.2 7.1]'
y = [0.8 1.9 2.7 4.0 4.5 5.7 6.4]'
```

| Inputs | MATLAB | numkit |
|---|---|---|
| `[h,p,D] = kstest2(x, y)` | `h=0, p=0.99999518, D=0.1428571429` | `h=0, p=0.99999970, D=0.1428571429, cv=0.7259` |
| `kstest2(x, y, 'Tail', 'larger')` | `h=0, p=1.0, D=0` | likely silent `Both` fallback (unconfirmed; needs probe) |

## Recommended fixes

1. **Use the `parse_kstest_tail` helper** (see `kstest` ТЗ) so
   `unequal/larger/smaller` map correctly. Keep legacy
   `both/right/left` as aliases.
2. **Replace string-scan with proper N-V loop** so `'Alpha', 0.01`
   reaches `alpha`.
3. **(Optional) drop the 4th `cv` output** to match MATLAB's 3-output
   signature, OR keep it but document as a numkit extension. The
   former is cleaner.
4. **Spec creation:** `tools/parity/specs/kstest2.json` — fingerprint
   over basic / Tail=larger / Tail=smaller / Alpha=0.01.
5. **PROGRESS.md row update:** drop "two-sample KS"; note
   MATLAB-name Tail support.

## Out of scope for this ТЗ

- Exact two-sample p-value for small n — the asymptotic series is
  accurate to ~1e-5 even at n=7, far better than MATLAB-script needs.
