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
| _none yet — first batch in progress_ | | | | | |

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
