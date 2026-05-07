# stats.dist/normstat — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 8e48677
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/stats/src/distributions/normal.cpp:129` (`normstat`)
- Adapter: `libs/stats/src/distributions/normal.cpp:200` (`normstat_reg`)
- Spec: `tools/parity/specs/normstat.json`
- `[m, v] = normstat(mu, sigma)` — matches MATLAB exactly

## MATLAB R2025b — actual behavior

`[m, v] = normstat(mu, sigma)` — `m = mu`, `v = sigma²`. Vector
inputs supported (returns same-shape arrays).

## Gaps (numkit vs MATLAB)

**No major gap detected.**

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `[m,v] = normstat(3, 2)` | `m=3, v=4` | identical ✅ |

## Recommended fixes

1. **Spec extension** — add fingerprint with vector inputs (mu and
   sigma both vectors). `tol = 0`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-07
- Notes: Two fixes (ТЗ said "no major gap", spec extension surfaced
  both):
    1. Adapter was scalar-only — vectorisation now via shared
       `emit_vec_stat_2arg` helper (added in earlier sweep commit).
    2. Impl returned `(0, 0)` for sigma=0 instead of NaN/NaN; MATLAB
       and Octave both return NaN here. Fixed: sigma <= 0 ⇒ NaN/NaN.
  16-fingerprint spec covers scalar / vector / scalar+vector
  broadcasting / sigma=0 / sigma<0. 4 TEST_F gtest + smoke .m.
  Parity OK numkit ↔ MATLAB ↔ Octave at tol=1e-12.
