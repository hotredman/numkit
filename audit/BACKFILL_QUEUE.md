# Backfill queue — gtest + smoke for pre-2026-05-04 functions

Functions shipped before the 4-artefact rule started (2026-05-04). Each
of these has only the C++ implementation + parity spec; gtest unit
test and smoke `.m` are missing.

Per [CLAUDE.md backfill rule](../CLAUDE.md#backfill-по-дороге-in-flight),
each /loop cycle now adds a backfill pair (gtest + smoke) for one entry
from this list **alongside** the new function. When the queue empties
the rule continues for new functions only.

Priority: simpler functions first (already validated, easy to write
gtest assertions against probe-captured values).

## Open

| # | Namespace | Function | Notes |
|---|---|---|---|
| 1 | wavelet.filt | `wrev` | trivial: y(k) = x(N-k+1) |
| 2 | wavelet.filt | `qmf` | y(k) = (-1)^(k-1+p) · x(N-k+1) |
| 3 | wavelet.dwt  | `dyaddown` | x(2:2:end) or x(1:2:end) |
| 4 | wavelet.dwt  | `dyadup` | zero-insert |
| 5 | wavelet.dwt  | `wmaxlev` | floor(log2(N/(Lf-1))) |
| 6 | wavelet.dwt  | `wkeep` | central / left / right / numeric-FIRST |
| 7 | wavelet.dwt  | `wextend` | sym / per / zpd / ppd |
| 8 | wavelet.filt | `dbwavf` | reverse(LO_R) / sqrt(2) |
| 9 | wavelet.filt | `coifwavf` | same |
| 10 | wavelet.filt | `symwavf` | same |
| 11 | wavelet.filt | `orthfilt` | quadruple from scaling filter |
| 12 | wavelet.shape | `gauswavf` | real Gaussian wavelet |
| 13 | wavelet.shape | `cgauwavf` | complex Gaussian wavelet |
| 14 | wavelet.dwt | `haart` | full multi-level + integer + matrix branches |
| 15 | wavelet.dwt | `ihaart` | inverse + partial level + integer + matrix |
| 16 | wavelet.dwt | `wrcoef` | single-band reconstruction (haar parity) |
| 17 | stats.fit | `normlike` | upgrade: censoring + freq + edge cases |

## Closed

| Function | Closed in commit | gtest TU | smoke .m |
|---|---|---|---|
| _none yet_ | | | |

---

## Notes

- Order picks "simpler first" so the first few backfills are cheap,
  proving the workflow before tackling complex ones (haart/ihaart/wrcoef).
- For each backfill, the gtest must cover **every** documented branch
  (matching the parity spec coverage). Smoke can be one or two
  representative invocations with `fprintf` of expected values.
- When closing a queue entry, move the row to the "Closed" table with
  the closing commit hash + new TU paths, and reference both the new
  function and the backfill in the cycle's commit message.
