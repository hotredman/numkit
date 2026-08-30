# dev-docs/ — documentation hub

The map lives here; the content lives in two wings:

- **[`handbook/`](handbook/)** — the living docs: rules, references,
  how-tos and protocols consulted while working on the current system.
- **[`memory/`](memory/)** — the reasoning archive: campaign logs,
  completed specs, design rationale, reviews (written per the AGENTS.md
  "Project memory" protocol; entries are history, never edited into
  current state).

## handbook/ contents

| Document | What it is |
|------|------------|
| [library_api.md](handbook/library_api.md) | **Authoritative public-API ruleset** (argument order, types, overloads, Doxygen + layering standards). Read before adding/refactoring any public `toolboxes/` function. |
| [callback_pausability.md](handbook/callback_pausability.md) | How-to for making a VM callback pausable (debugger-visible): decision rule + per-mechanism recipes. |
| [core_architecture.md](handbook/core_architecture.md) | The core architecture article (RU): Value substrate (16-byte tagged, COW/PMR), layer DAG, dual engine, pausable VM. |
| [object_model.md](handbook/object_model.md) | Object-model design: type + registry + clone infrastructure. |
| [namespace_design.md](handbook/namespace_design.md) | `toolboxes/` namespace layout + conventions; MATLAB taxonomy → source tree. |
| [format_homes.md](handbook/format_homes.md) | Number-display / `format` reference. |
| [coordination.md](handbook/coordination.md) | Multi-session worker-territory protocol. **Dormant** — single-session mode. |
| [parity_agent_prompt.md](handbook/parity_agent_prompt.md) | Cold-start runbook for the autonomous MATLAB-parity cron agent. **Reference only.** |

Open non-defect work (tech-debt / deferred design) is tracked one file per
item in [`todo/`](todo/) — lifecycle: open → done (record the outcome in
`memory/`, delete the file).

## memory/ — notable entries

[vm_callbacks_plan.md](memory/vm_callbacks_plan.md) (the pausable-callback
build log behind callback_pausability.md),
[linalg_roadmap.md](memory/linalg_roadmap.md) (the closed linalg parity
campaign), [stack_safety.md](memory/stack_safety.md) (the nesting/stack
crash design), [opcode_fusion_catalog.md](memory/opcode_fusion_catalog.md)
(future VM fusion design survey).

---

Entry points elsewhere: [`../AGENTS.md`](../AGENTS.md) (session rules — the
first thing to read), [`../bugs/README.md`](../bugs/README.md) (bug
tracker + protocol), [`../fieldtest/README.md`](../fieldtest/README.md)
(real-world differential testing).
