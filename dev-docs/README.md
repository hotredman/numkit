# dev-docs/ — developer & AI coding documentation

Documentation for whoever (human **or** AI session) is **writing numkit**. This
is deliberately separate from the user-facing Doxygen API reference, which is
generated from header comments and lives under [`../docs/`](../docs/).

Two groups, by when you reach for them:

## Guides & rules — consult these *while writing code* (`dev-docs/`)

| File | What it is |
|------|------------|
| [LIBRARY_API.md](LIBRARY_API.md) | **Authoritative public-API ruleset** for `toolboxes/<ns>/include/numkit/<ns>/**`: argument order, native scalars vs `const Value &` vs `Span<const double>`, `FnHandle` callbacks, no `Engine *` in public signatures, magic-polymorphism → typed overloads, the options-struct threshold, multi-output return shape. **Read before adding or refactoring any public `toolboxes/` function.** |
| [CALLBACK_PAUSABILITY.md](CALLBACK_PAUSABILITY.md) | How-to guide for making a VM callback pausable (debugger-visible): the decision rule (in-bytecode frame-push vs C++ state machine vs embedded-`.m` wrapper) + per-mechanism recipes and gotchas. |
| [FORMAT_HOMES.md](FORMAT_HOMES.md) | Number-display / `format` reference. |

## Architecture & process (`dev-docs/`)

| File | What it is |
|------|------------|
| [ARCHITECTURE_REVIEW.md](ARCHITECTURE_REVIEW.md) | Point-in-time health check + **risk register** (2026-06-13): layer-DAG invariants, metrics, resolved tech-debt risks, and the dual-backend duplication decision. |
| [STACK_SAFETY.md](STACK_SAFETY.md) | **Open crash problem + fix design**: unbounded input nesting (deep expressions / deep blocks) overflows the C++ stack in the bytecode compiler & TreeWalker and kills the process / WASM module with no diagnostic (repro + measured thresholds inside). Three-layer proper fix: parse-time nesting contract (≤255, justified by the `uint8_t` register file), watermark `StackGuard` in every recursive walker, regression + fuzzing — plus the anti-kludge list (what NOT to do). |
| [LAYERING_TARGET_ARCHITECTURE.md](LAYERING_TARGET_ARCHITECTURE.md) | The 11-layer dependency DAG, per-layer ownership, and the Phase-3-A layering-refactor spec + status. |
| [NAMESPACE_DESIGN.md](NAMESPACE_DESIGN.md) | `toolboxes/` namespace layout + conventions; how the MATLAB documentation taxonomy maps onto the source tree. |
| [OBJECT_MODEL.md](OBJECT_MODEL.md) | Object-model design: type + registry + clone infrastructure. |
| [VM_CALLBACKS_PLAN.md](VM_CALLBACKS_PLAN.md) | Build log + rationale behind VM-native pausable callbacks — the companion to the `CALLBACK_PAUSABILITY.md` guide. (Status: substantially implemented; a known tail is intentionally deferred.) |
| [COORDINATION.md](COORDINATION.md) | Multi-session worker-territory protocol. **Currently dormant** — the repo runs in single-session mode. |
| [PARITY_AGENT_PROMPT.md](PARITY_AGENT_PROMPT.md) | Cold-start runbook/prompt for the autonomous MATLAB-parity cron agent. **Reference only** — not currently scheduled. |
| [CORE_ARCHITECTURE.md](CORE_ARCHITECTURE.md) | The big architecture article (RU): Value substrate (16-byte tagged, COW/PMR), layer DAG, dual engine, pausable VM, object model — the "why" behind the core. |
| [OBJECT_MODEL.md](OBJECT_MODEL.md) | Object-model design: type + registry + clone infrastructure. (16 external refs — core tests, classdef work.) |
| [VM_CALLBACKS_PLAN.md](VM_CALLBACKS_PLAN.md) | Build log + rationale behind VM-native pausable callbacks — companion to CALLBACK_PAUSABILITY.md. (21 external refs from the callback machinery.) **Substantially implemented; a known tail intentionally deferred.** |
| [NAMESPACE_DESIGN.md](NAMESPACE_DESIGN.md) | `toolboxes/` namespace layout + conventions; how the MATLAB taxonomy maps onto the source tree. |
| [FORMAT_HOMES.md](FORMAT_HOMES.md) | Number-display / `format` reference. |
| [STACK_SAFETY.md](STACK_SAFETY.md) | Stack-safety design: nesting contract + watermark guards + the anti-kludge rationale. **Fixed 2026-08-30.** |
| [TODO.md](TODO.md) | Tracked tech-debt (e.g. per-Engine RNG streams) — deliberately out-of-scope improvements. |
| [OPCODE_FUSION_CATALOG.md](OPCODE_FUSION_CATALOG.md) | Design catalog of fused superinstructions (industry survey) — future VM optimisation, not started. |
| [CSL_FIRST_CLASS.md](CSL_FIRST_CLASS.md) | First-class comma-separated lists design. **Campaign closed** (2026-07) — historical rationale. |
| [LINALG_ROADMAP.md](LINALG_ROADMAP.md) | Owner doc of the linalg parity campaign (snapshot 2026-08-05). **Campaign closed** — linalg is at 0 missing in bugs/missing.md. |
| [LINALG_PERF_PLAN.md](LINALG_PERF_PLAN.md), [LINALG_PERF_CYCLE2.md](LINALG_PERF_CYCLE2.md), [LINALG_REVIEW_FOLLOWUP.md](LINALG_REVIEW_FOLLOWUP.md) | The linalg campaign's working logs (perf cycles + review follow-ups). **Campaign closed** — historical. |

---

Entry points elsewhere: [`../AGENTS.md`](../AGENTS.md) (per-session repo notes),
[`../CONTRIBUTING.md`](../CONTRIBUTING.md), and [`../docs/`](../docs/) (Doxygen
API-reference source).
