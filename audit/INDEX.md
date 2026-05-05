# Audit findings — index

This is the registry of parity-completion ТЗ produced by the audit worker.
Each row links to one ТЗ document under `audit/findings/<namespace>/`.

The index is sorted by namespace, then by priority (high first).
Status legend:

- **open** — ТЗ written, not yet picked up
- **in_progress** — main worker is working on it (commit message
  references the ТЗ path)
- **closed** — ТЗ implemented; file moved to `audit/closed/<ns>/`

## Open ТЗ

| File | Function | Namespace | Priority | Effort | Audit commit |
|---|---|---|---|---|---|
| [findings/stats/betalike.md](findings/stats/betalike.md) | betalike | stats.fit | medium | small | bfda361 |
| [findings/stats/evlike.md](findings/stats/evlike.md) | evlike | stats.fit | medium | medium | bfda361 |
| [findings/stats/expfit.md](findings/stats/expfit.md) | expfit | stats.fit | medium | medium | bfda361 |
| [findings/stats/explike.md](findings/stats/explike.md) | explike | stats.fit | medium | medium | bfda361 |
| [findings/stats/gamlike.md](findings/stats/gamlike.md) | gamlike | stats.fit | medium | small | bfda361 |
| [findings/stats/gevlike.md](findings/stats/gevlike.md) | gevlike | stats.fit | medium | small | bfda361 |
| [findings/stats/gplike.md](findings/stats/gplike.md) | gplike | stats.fit | medium | small | bfda361 |
| [findings/stats/lognfit.md](findings/stats/lognfit.md) | lognfit | stats.fit | medium | medium | bfda361 |
| [findings/stats/lognlike.md](findings/stats/lognlike.md) | lognlike | stats.fit | medium | medium | bfda361 |
| [findings/stats/normfit.md](findings/stats/normfit.md) | normfit | stats.fit | medium | medium | bfda361 |
| [findings/stats/normlike.md](findings/stats/normlike.md) | normlike | stats.fit | medium | medium | bfda361 |
| [findings/stats/wbllike.md](findings/stats/wbllike.md) | wbllike | stats.fit | medium | medium | bfda361 |
| [findings/stats/binofit.md](findings/stats/binofit.md) | binofit | stats.fit | low | small | bfda361 |
| [findings/stats/poissfit.md](findings/stats/poissfit.md) | poissfit | stats.fit | low | small | bfda361 |
| [findings/stats/raylfit.md](findings/stats/raylfit.md) | raylfit | stats.fit | low | small | bfda361 |
| [findings/stats/unifit.md](findings/stats/unifit.md) | unifit | stats.fit | low | small | bfda361 |
| [findings/stats/chi2gof.md](findings/stats/chi2gof.md) | chi2gof | stats.test | medium | large | 69fab7c |
| [findings/stats/jbtest.md](findings/stats/jbtest.md) | jbtest | stats.test | medium | medium | 69fab7c |
| [findings/stats/kruskalwallis.md](findings/stats/kruskalwallis.md) | kruskalwallis | stats.test | medium | medium | 69fab7c |
| [findings/stats/kstest.md](findings/stats/kstest.md) | kstest | stats.test | high | medium | 69fab7c |
| [findings/stats/kstest2.md](findings/stats/kstest2.md) | kstest2 | stats.test | high | small | 69fab7c |
| [findings/stats/runstest.md](findings/stats/runstest.md) | runstest | stats.test | high | medium | 69fab7c |
| [findings/stats/ttest.md](findings/stats/ttest.md) | ttest | stats.test | high | medium | 69fab7c |
| [findings/stats/ttest2.md](findings/stats/ttest2.md) | ttest2 | stats.test | high | medium | 69fab7c |
| [findings/stats/vartest.md](findings/stats/vartest.md) | vartest | stats.test | medium | small | 69fab7c |
| [findings/stats/vartest2.md](findings/stats/vartest2.md) | vartest2 | stats.test | medium | small | 69fab7c |
| [findings/stats/vartestn.md](findings/stats/vartestn.md) | vartestn | stats.test | medium | medium | 69fab7c |
| [findings/stats/ztest.md](findings/stats/ztest.md) | ztest | stats.test | medium | small | 69fab7c |
| [findings/stats/fishertest.md](findings/stats/fishertest.md) | fishertest | stats.test | low | small | 69fab7c |
| [findings/stats/ranksum.md](findings/stats/ranksum.md) | ranksum | stats.test | low | small | 69fab7c |
| [findings/stats/signrank.md](findings/stats/signrank.md) | signrank | stats.test | low | small | 69fab7c |
| [findings/stats/signtest.md](findings/stats/signtest.md) | signtest | stats.test | low | small | 69fab7c |

## Closed ТЗ

| File | Function | Closed in commit | Closed date |
|---|---|---|---|
| _none yet_ | | | |

---

## How this file is updated

- **Auditor** appends new rows to "Open ТЗ" when creating ТЗ.
- **Main worker** moves rows from "Open ТЗ" to "Closed ТЗ" after fix
  lands and updates the file path to point at `audit/closed/...`.
- Neither worker rewrites historical rows once closed.
