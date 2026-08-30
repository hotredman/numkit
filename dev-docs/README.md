# dev-docs/ — living developer documentation

Only **living documents** live in this directory: rules, references and
protocols an agent or developer consults *while working on the current
system*. Reasoning — campaign logs, completed specs, point-in-time reviews,
design rationale — lives in [`memory/`](memory/) per the project-memory
protocol (see AGENTS.md "Project memory").

| Document | What it is |
|------|------------|
| [LIBRARY_API.md](LIBRARY_API.md) | **Authoritative public-API ruleset** (argument order, types, overloads, Doxygen + layering standards). Read before adding/refactoring any public `toolboxes/` function. |
| [CALLBACK_PAUSABILITY.md](CALLBACK_PAUSABILITY.md) | How-to for making a VM callback pausable (debugger-visible): decision rule + per-mechanism recipes. |
| [CORE_ARCHITECTURE.md](CORE_ARCHITECTURE.md) | The core architecture article (RU): Value substrate (16-byte tagged, COW/PMR), layer DAG, dual engine, pausable VM. |
| [OBJECT_MODEL.md](OBJECT_MODEL.md) | Object-model design: type + registry + clone infrastructure. |
| [NAMESPACE_DESIGN.md](NAMESPACE_DESIGN.md) | `toolboxes/` namespace layout + conventions; MATLAB taxonomy → source tree. |
| [FORMAT_HOMES.md](FORMAT_HOMES.md) | Number-display / `format` reference. |
| [COORDINATION.md](COORDINATION.md) | Multi-session worker-territory protocol. **Dormant** — single-session mode. |
| [PARITY_AGENT_PROMPT.md](PARITY_AGENT_PROMPT.md) | Cold-start runbook for the autonomous MATLAB-parity cron agent. **Reference only.** |
| [TODO.md](TODO.md) | Tracked tech-debt (e.g. per-Engine RNG streams). |

## memory/ — the reasoning archive

[`memory/`](memory/) holds the project reasoning record: architectural
decision logs, completed campaign specs (CSL, linalg parity, layering
refactor, stack safety), point-in-time reviews, gotchas and performance
measurements. Written per the AGENTS.md protocol; entries never edited
into "current state" — they are history. Notable entries:
[vm_callbacks_plan.md](memory/vm_callbacks_plan.md) (the pausable-callback
build log behind CALLBACK_PAUSABILITY.md),
[linalg_roadmap.md](memory/linalg_roadmap.md) (the closed linalg parity
campaign), [stack_safety.md](memory/stack_safety.md) (the nesting/stack
crash design), [opcode_fusion_catalog.md](memory/opcode_fusion_catalog.md)
(future VM fusion design survey).

---

Entry points elsewhere: [`../AGENTS.md`](../AGENTS.md) (session rules — the
first thing to read), [`../bugs/README.md`](../bugs/README.md) (bug
tracker + protocol), [`../fieldtest/README.md`](../fieldtest/README.md)
(real-world differential testing).
