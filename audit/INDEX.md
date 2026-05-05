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
