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
| [full_fidelity.md](handbook/full_fidelity.md) | **The complete-function rule** (user rule 2026-09-06): a function is implemented only when it matches MATLAB for every documented argument form and input type, with per-branch test coverage; gaps are filed, never silent. Read before implementing any function. |
| [clean_code.md](handbook/clean_code.md) | **No crutches, no dirty solutions** (user rule 2026-09-06): root-cause fixes in the owning layer only; workarounds require a filed bug id, teaching-to-the-test and silent fallbacks are forbidden. Read before writing any fix. |
| [callback_pausability.md](handbook/callback_pausability.md) | How-to for making a VM callback pausable (debugger-visible): decision rule + per-mechanism recipes. |
| [svelte_architecture.md](handbook/svelte_architecture.md) | **Library-first Svelte rules + the Play strategy** (user rules 2026-09-12): Play is where the new IDE grows — converge, never rewrite; the React IDE stays the stable product. Pages compose library components only — no raw `<div>`/`style:`/`<style>` on pages, one component per file (each chart type its own file), design tokens over hex literals, pure logic in plain `.js`, props down / callbacks up, TypeScript end-to-end with the TYPED wire contract, the engine wire JSON is the plot library's input. Read before touching `play/src` or planning front-end work. |
| [core_architecture.md](handbook/core_architecture.md) | The core architecture article (RU): Value substrate (16-byte tagged, COW/PMR), layer DAG, dual engine, pausable VM. |
| [simd_naming.md](handbook/simd_naming.md) | **SIMD naming & multi-backend rules** (user rule, 2026-09-13): SIMD = feature level (`NK_WITH_SIMD`, presets), backend = implementation in TU suffixes (`*_highway.cpp`; bare `*_simd.cpp` impl TUs forbidden), portable = scalar fallback; plus the recipe for adding a second backend. Read before touching vectorized kernels or their build wiring. |
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
(future VM fusion design survey),
[play_new_ide_decision.md](memory/play_new_ide_decision.md) (the
Play-is-the-new-IDE decision: converge, never rewrite).

---

Entry points elsewhere: [`../AGENTS.md`](../AGENTS.md) (session rules — the
first thing to read), [`../bugs/README.md`](../bugs/README.md) (bug
tracker + protocol), [`../fieldtest/README.md`](../fieldtest/README.md)
(real-world differential testing).
