# Decision: Play (Svelte) is the growth line of the new IDE

*Date:* 2026-09-12 · *Kind:* architecture decision · *Status:* active

## Context

Two front-ends exist after the Play campaign (manifest/DAG/
ReactiveSession portions A–D2 + the 67-script gallery):

- `ide/` — the mature IDE: React 18 + reactflow (dataflow graph) +
  three.js (3D) + elkjs (layout), Electron-wrapped for desktop;
  10-category figure renderer verified against MATLAB in portions
  29–41; Playwright e2e suites (core/2d/3d/figures/regressions).
- `play/` — Svelte 5 + TypeScript, wasm engine in the browser;
  reactive gallery driven by `@play` manifests; renderer covers 3
  chart types (cartesian line, polar, heatmap) reading the same
  `__FIGURE_DATA__` wire.

The user asked whether moving off React onto Svelte is the right call
(discussion 2026-09-12) and then confirmed the strategic framing.

## Alternatives considered

1. **Stay React-only** (keep investing in `ide/`) — rejected: the
   product is web-first around the wasm engine (Svelte ships ~no
   runtime vs React+ReactDOM+reactflow payload), and the flagship UX
   (slider → `evalDirty` → redraw, tens of ticks/sec) is fine-grained
   reactive by nature; React needs per-component memoization to avoid
   jank there, Svelte 5 runes provide it structurally.
2. **Big-bang rewrite of the IDE in Svelte** — rejected: second-system
   trap; months of regression risk over a mature, e2e-tested app.
3. **Parallel forever** — rejected: two renderers and two UI stacks
   mean every wire improvement is paid for twice.

## Decision

**Converge, never rewrite** (user decision, 2026-09-12): Play is where
the new IDE grows. The React IDE is frozen at "stable product" —
fixes and wire-side features only, no new front-end architecture; new
UI work lands on the Svelte side. Recorded as the Strategy section of
`dev-docs/handbook/svelte_architecture.md` (the living rule).

Reversibility guarantee: the expensive parts are framework-neutral by
design — engine + wire contract in C++, plot geometry in pure TS
modules (rule R5), the wire typed at the consumer (`wire.ts`, R8).
Migrating a chart = porting a thin drawing layer, not the product.

## Retirement criteria (React IDE archivable only when Svelte has all)

1. Renderer parity by chart category — every portions-29–41 category,
   incl. 3D (framework-agnostic lib, e.g. three.js).
2. An editor with syntax highlighting + the file tree / workspace.
3. An e2e grid covering the IDE's Playwright suites.

## Known costs accepted

- Two stacks in parallel until the criteria close (explicit cost, not
  an accident).
- Svelte 5 runes are young: ecosystem and agent knowledge are thinner
  than React's — already hit `vite-plugin-svelte` 5.x (needs vite 6)
  and svelte-check rejecting TS 7. Mitigated by the typed contract +
  the handbook rules R1–R8.
