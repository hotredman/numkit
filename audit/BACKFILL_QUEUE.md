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
| 13 | wavelet.shape | `cgauwavf` | complex Gaussian wavelet |
| 14 | wavelet.dwt | `haart` | full multi-level + integer + matrix branches |
| 15 | wavelet.dwt | `ihaart` | inverse + partial level + integer + matrix |
| 16 | wavelet.dwt | `wrcoef` | single-band reconstruction (haar parity) |
| 17 | stats.fit | `normlike` | upgrade: censoring + freq + edge cases |

## Closed

| Function | Closed in commit | gtest TU | smoke .m |
|---|---|---|---|
| `wrev` | PENDING | libs/wavelet/tests/wrev_test.cpp | libs/wavelet/tests/smoke/wrev_smoke.m |
| `qmf` | PENDING | libs/wavelet/tests/qmf_test.cpp | libs/wavelet/tests/smoke/qmf_smoke.m |
| `dyaddown` | PENDING | libs/wavelet/tests/dyaddown_test.cpp | libs/wavelet/tests/smoke/dyaddown_smoke.m |
| `dyadup` | PENDING | libs/wavelet/tests/dyadup_test.cpp | libs/wavelet/tests/smoke/dyadup_smoke.m |
| `wmaxlev` | PENDING | libs/wavelet/tests/wmaxlev_test.cpp | libs/wavelet/tests/smoke/wmaxlev_smoke.m |
| `wkeep` | PENDING | libs/wavelet/tests/wkeep_test.cpp | libs/wavelet/tests/smoke/wkeep_smoke.m |
| `wextend` | PENDING | libs/wavelet/tests/wextend_test.cpp | libs/wavelet/tests/smoke/wextend_smoke.m |
| `dbwavf` | PENDING | libs/wavelet/tests/dbwavf_test.cpp | libs/wavelet/tests/smoke/dbwavf_smoke.m |
| `coifwavf` | PENDING | libs/wavelet/tests/coifwavf_test.cpp | libs/wavelet/tests/smoke/coifwavf_smoke.m |
| `symwavf` | PENDING | libs/wavelet/tests/symwavf_test.cpp | libs/wavelet/tests/smoke/symwavf_smoke.m |
| `orthfilt` | PENDING | libs/wavelet/tests/orthfilt_test.cpp | libs/wavelet/tests/smoke/orthfilt_smoke.m |
| `gauswavf` | PENDING | libs/wavelet/tests/gauswavf_test.cpp | libs/wavelet/tests/smoke/gauswavf_smoke.m |

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
