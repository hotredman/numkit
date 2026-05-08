# signal/enbw — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 69ef496
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/signal/src/windows/windows.cpp` (`enbw`)
- Adapter: same file (registered in library.cpp)
- Spec: **none** (no `tools/parity/specs/enbw.json`)
- What works today:
  - `bw = enbw(window)` — equivalent noise bandwidth
  - `bw = enbw(window, fs)` — with sampling frequency

All probed values match MATLAB exactly.

## MATLAB R2025b — actual behavior

Documented signatures (`help enbw`):

- `bw = enbw(window)` — returns ENBW relative to bin width
- `bw = enbw(window, fs)` — scales by `fs`

ENBW = `N · sum(w.^2) / (sum(w))^2`.

## Gaps (numkit vs MATLAB)

**No major gap detected.** Numbers match for all 4 probed inputs
including the `fs`-scaled form.

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `enbw(hamming(8))` | `1.4970603238` | identical ✅ |
| `enbw(hann(64))` | `1.5238095238` | identical ✅ |
| `enbw(rectwin(64))` | `1.0000000000` | identical ✅ |
| `enbw(hamming(64), 100)` | `2.1536278498` | identical ✅ |

## Recommended fixes

1. **Spec creation:** `tools/parity/specs/enbw.json` does not exist.
   Create with fingerprint over `enbw(<window>)` for several windows
   (hamming, hann, rectwin, blackman, kaiser(beta=8.6)) and the
   `fs`-scaled form. `tol = 1e-12`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-08
- Notes: Pure spec creation, no impl change. Numkit enbw already
  matched MATLAB exactly across hamming / hann / rectwin /
  blackman with and without `fs`-scaled form.

  New spec at tools/parity/specs/enbw.json (didn't exist before)
  with 7 fingerprints. Parity OK numkit ↔ MATLAB at tol=1e-12.
  Octave's signal package does not yet implement enbw; we follow
  MATLAB. 5 TEST_F gtest + smoke.
