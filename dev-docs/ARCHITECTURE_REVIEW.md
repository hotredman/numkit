# Architecture Review — numkit-m (2026-06-13)

Point-in-time review taken right after the Phase-3-A layering refactor closed
out (11 guarded layers, lock-free compute, no `Engine` in compute signatures).
This document records the findings, the **risk register**, and how each risk was
resolved. For the layer DAG and per-layer ownership see
[LAYERING_TARGET_ARCHITECTURE.md](LAYERING_TARGET_ARCHITECTURE.md); this file is
the higher-level health check and the home for the decisions that closed the
review.

## Verdict

Mature, disciplined codebase. The headline property — **`core ⊥ everything`** —
holds: the compute layers (`value`/`fs`/`ops`/`figure`/`math`/`lang`/`graphics`/
`toolboxes`) build and link **without an `Engine`**; only `runtime`,
`scriptgraph` and `bundle` depend on the core engine. The DAG is acyclic and
**enforced by `tools/check_layering.py` with zero per-file exemptions**. The
compute layer is lock-free. Tech debt is bounded and, after this review, largely
closed.

## Metrics (at review time)

| | Value |
|---|---|
| Production (non-test) LOC | ~235,700 across 12 layer dirs |
| Test LOC / files | ~59,800 / 426 `.cpp` |
| gtest macros → runtime cases | ~3,922 → **11,657** (DualEngine ×2 backends) |
| Parity specs (vs MATLAB R2025b + Octave) | 1,524 |
| Bug catalog | 105 files (53 fixed / 49 open = 11 true bugs + 38 gaps/stubs) |
| `src/` line coverage (llvm-cov) | **78.62%** (region 74.81%, fn 84.80%, branch 64.96%) |
| Guarded layers | 11 + bundle |
| TODO/FIXME in non-test src | 4 |

## Risk register

| # | Risk | Severity | Status | Ref |
|---|------|----------|--------|-----|
| 1 | Benchmark suite API-rot (`*_bench.cpp` stale post-refactor; only built on bench presets → no live perf baseline) | 🔴 | ✅ FIXED | `0e7927e8` |
| 2 | TreeWalker/VM logic duplication (~10–15%) | 🟡 | ✅ DOCUMENTED (keep) | this doc |
| 3 | `Engine` god-class trend (wide public surface) | 🟡 | ✅ FIXED | `b412edf2` |
| 4 | `normalize()` violated LIBRARY_API §10/§15 (`const Value*` + out-pointers) | 🟢 | ✅ FIXED | `64b3ff11` |
| 5 | CMake include-dir aggregation fragility (silent empty `NUMKIT_*_INCLUDE_DIRS`) | 🟢 | ✅ FIXED | `10ff4557` |
| 6 | No code-coverage visibility | 🟢 | ✅ FIXED | `2d9952ee` |
| 7 | `apple-m` preset unvalidated | 🟢 | ⏸ DEFERRED (needs Apple hardware) | — |

### Risk #2 — TreeWalker/VM duplication (decision: keep)

The semantics both backends share are **already centralized**:

- **Object dispatch** — binary/unary operator overloads, `subsref`/`subsasgn`,
  property get/set, method/constructor invocation — flows through
  **`Engine`-owned hooks** (`tryObjectBinaryOp`, `resolveSubsrefChunk`,
  `classGetter`, `invokeClassMethod`, …) called identically from both engines.
  classdef logic is **not** duplicated across backends.
- **Lvalue assignment** was unified into a single *general recursive* path in
  each engine (Compiler `compileLValueStore`/`needsGeneralLValuePath`;
  TreeWalker's non-identifier lvalue path).

The residual parallelism the review flagged is the **irreducible cost of two
execution strategies**: the TreeWalker recursively walks the AST while the VM
dispatches bytecode. Each necessarily keeps its own call-stack representation
(`activeFrames_` vs `frames_`) and its own control flow (loops / conditionals /
calls). Collapsing that would mean collapsing to a single backend — which
defeats the design: the VM is the fast path, the TreeWalker is the fallback
(on `RegisterExhaustionError` / compile failure) and the reference engine, and
`DualEngineTest` runs **every** test on both to catch divergence.

**Decision:** keep. Shared semantics are already factored out via `Engine`
hooks; the remaining duplication is intentional and is guarded for correctness
by `DualEngineTest`. No further extraction is warranted unless backend churn
rises materially.

### Risk #7 — apple-m unvalidated

The `apple-m` preset (ARM64 macOS, SIMD currently off) has not been built/tested
— this machine is x86_64 Windows. Validate when Apple hardware is available.

## Strengths (for the record)

- **Enforced layering** — the guard is always-on with no exemptions, so the
  `core ⊥ everything` invariant cannot silently regress.
- **Context-decoupling pattern** applied consistently: `FsContext`,
  `FigureManager`, `GraphicsContext`, `RngContext` — Engine-owned state threaded
  through `_reg` adapters; compute layer is lock-free.
- **compute/register split** — 234 `_reg` adapters; one sanctioned `Engine`
  touchpoint per library (`install(Engine&)`), everything else core-free.
- **Dual-engine parity testing** + 1,524 cross-engine parity specs as the
  correctness gate; structured, kind-tagged bug catalog.

## Coverage

Run `scripts/coverage.ps1` (Ninja + clang-cl + llvm-cov; see the `coverage`
preset). Lightest areas at review time: `toolboxes/optim` ~21% and
`toolboxes/control` ~41% line coverage; `wavelet`/`signal`/`stats` ~85–90%.
