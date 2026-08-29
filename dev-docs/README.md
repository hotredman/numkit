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

---

Entry points elsewhere: [`../CLAUDE.md`](../CLAUDE.md) (per-session repo notes),
[`../CONTRIBUTING.md`](../CONTRIBUTING.md), and [`../docs/`](../docs/) (Doxygen
API-reference source).
