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
| [LIBRARY_API.md](handbook/LIBRARY_API.md) | **Authoritative public-API ruleset** (argument order, types, overloads, Doxygen + layering standards). Read before adding/refactoring any public `toolboxes/` function. |
| [CALLBACK_PAUSABILITY.md](handbook/CALLBACK_PAUSABILITY.md) | How-to for making a VM callback pausable (debugger-visible): decision rule + per-mechanism recipes. |
| [CORE_ARCHITECTURE.md](handbook/CORE_ARCHITECTURE.md) | The core architecture article (RU): Value substrate (16-byte tagged, COW/PMR), layer DAG, dual engine, pausable VM. |
| [OBJECT_MODEL.md](handbook/OBJECT_MODEL.md) | Object-model design: type + registry + clone infrastructure. |
| [NAMESPACE_DESIGN.md](handbook/NAMESPACE_DESIGN.md) | `toolboxes/` namespace layout + conventions; MATLAB taxonomy → source tree. |
| [FORMAT_HOMES.md](handbook/FORMAT_HOMES.md) | Number-display / `format` reference. |
| [COORDINATION.md](handbook/COORDINATION.md) | Multi-session worker-territory protocol. **Dormant** — single-session mode. |
| [PARITY_AGENT_PROMPT.md](handbook/PARITY_AGENT_PROMPT.md) | Cold-start runbook for the autonomous MATLAB-parity cron agent. **Reference only.** |

Open tech-debt is tracked in [TODO.md](TODO.md) (kept beside this map).

## memory/ — notable entries

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
