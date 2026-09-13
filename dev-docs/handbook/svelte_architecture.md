# Svelte architecture rules (library-first, no page-level HTML)

*Living rule (user decision, 2026-09-12).* Applies to `play/` today and
to every future Svelte surface (the IDE migration rides the same
library).

## Strategy: Play is where the new IDE grows (user decision, 2026-09-12)

**Converge, never rewrite.** `play/` is the growth line of the next nk
front-end: today a reactive gallery, growing into the IDE. The React
IDE (`ide/`) stays the stable product — it receives fixes and
wire-side features, but NO new front-end architecture; new UI work
lands on the Svelte side.

Why a Svelte line at all (and not more React): the product is web-first
around the wasm engine (Svelte ships ~no runtime vs the
React+reactflow payload), and the flagship UX — slider → `evalDirty` →
redraw — is fine-grained reactive by nature; Svelte 5 runes give that
structurally where React needs per-component hand memoization. The
expensive parts were deliberately moved out of any framework: engine +
wire (C++), plot geometry (pure TS modules, R5), editor/3D libraries
(framework-agnostic). The React-locked surface is small (reactflow
graph + component glue; Svelte Flow exists as the port path).

**Retirement criteria** — the React IDE may be archived only when the
Svelte side has ALL of:

1. renderer parity by chart category (every portions-29–41 category,
   incl. 3D),
2. an editor with syntax highlighting + the file tree / workspace,
3. an e2e grid covering the IDE's Playwright suites.

Until then both live in parallel — an explicit, accepted cost. Logic
stays framework-neutral (R5/R7/R8) precisely so this decision remains
reversible at every step. Full rationale:
[memory/play_new_ide_decision.md](../memory/play_new_ide_decision.md).

## The contract

**Pages compose; the library draws.** A page/screen file (App.svelte,
future route components) contains ONLY library components and logic
wiring — never raw HTML drawing primitives and never styling.

### R1 — No `<div>`/`<span>`/`<p>` on pages
Pages import components from `src/lib/`. Raw HTML elements exist ONLY
inside `src/lib/**` component files. If a page needs a box, that box is
a library component with a name that says what it IS (`Panel`,
`FigureCard`), not what tag it renders.

### R2 — No `style:` attributes and no `<style>` blocks on pages
Styling is a component concern. It lives scoped inside library
components or as tokens (R4). A page that wants a tweak either picks a
different component or passes a semantic prop (`variant="danger"`,
`size="compact"`) — never raw CSS.

### R3 — One component per file, one file per element
Every visual element — each chart type, each control, each layout
region — is its own `.svelte` file named after the component
(`PascalCase.svelte`). `PlotCartesian.svelte`, `PlotPolar.svelte`,
`SliderControl.svelte`. No god-files with five charts inside.

### R4 — Tokens, not hex literals
Colors / spacing / fonts live in `src/lib/tokens.js` as named constants.
Components import tokens; pages never see a color.

### R5 — Pure logic is plain `.js` beside the components
Scales, wire parsing, manifest flattening, tick math — framework-free
modules (`plotModel.js`, `wire.js`, `camelCase.js`), importable by any
framework and unit-testable in vitest without a DOM. Same pattern as
the IDE's framework-free `plotTicks.js` / `colormaps.js` — which this
library IMPORTS rather than forks.

### R6 — Props down, callbacks up
Components take data via props and report via callback props
(`onselect`, `oninput`). No global stores in v1; `$state` lives in the
owning component and flows down. (Escalate to stores only when a real
cross-tree need appears — then document it here.)

### R7 — The wire JSON is the plot library's input contract
`lib/plot/*` renders the ENGINE wire (`__FIGURE_DATA__` JSON) — the
parity-verified surface built in portions 29–41. Chart types land one
file at a time; each new type gets a pure-model module + component, and
anchors against the same MATLAB semantics the React renderer was
verified with.

### R8 — TypeScript in play/ (user rule, 2026-09-12)
`play/` is TS end-to-end (`<script lang="ts">`, `.ts` modules,
svelte-check at ZERO errors — `npx svelte-check --threshold error`).
The crown is `lib/charts/wire.ts`: the engine wire JSON + the @play
manifest TYPED — the contract portions 29–41 built, made compiler-
checked at the consumer. When the wire grows, wire.ts grows IN THE
SAME COMMIT. The IDE stays plain JS (its choice predates Play); the
boundary between them is the typed wire. IDE helpers imported from
`@ide-plot` are untyped JS — wrap them ONCE with a typed shim in
play (see `ticks()` in series.ts) instead of `as`-casting at call
sites.

## Layout

```
play/src/
  App.svelte            ← page: composition ONLY (R1–R2), lang="ts"
  lib/
    tokens.ts           ← design tokens + Tokens interface (R4)
    ui/                 ← layout & controls (Panel, Button, Slider…)
    charts/             ← the renderer: stages + charts (R3)
      wire.ts           ← THE TYPED WIRE CONTRACT + manifest (R5, R8)
      stage.ts          ← pure: viewport/projection/polar models (R5)
      series.ts         ← pure: line-series filter/style/paths (R5)
      Figure.svelte     ← root: dispatches per stage + legend
      CartesianStage.svelte   ← the coordinate system (frame/ticks)
      PolarStage.svelte
      LineChart.svelte  ← ONE chart type, stage-agnostic (any
                          projection) — cartesian and polar share it
      Legend.svelte
```

## Enforcement

Review-time (this file is the checklist) + a mechanical guard is
planned: a tiny vitest scan asserting page files contain no `<div`,
`style=` or `<style` tokens (lib/ is exempt). Add it the moment a
second page appears.

## D1 enforcement state (2026-09-12)

The refactor landed: `App.svelte` is pure composition (grep for
`<div|style=|<style` → 0), the library lives in `lib/ui` (10
components) + `lib/charts` (Figure root + CartesianStage + PolarStage
+ the STAGE-AGNOSTIC LineChart + Legend, pure `wire.ts` / `stage.ts`
/ `series.ts`, all TypeScript). TS migration caught a REAL latent
bug the wire smokes never hit: D1 called the IDE's niceTicks (which
returns {major, minor}) as if it were an array — polar r-rings would
have crashed at first render; the typed `ticks()` shim now guards
that boundary. TECHNICALITY discovered: Svelte
`<style>` blocks are STATIC — no JS interpolation — so tokens reach
them as CSS VARIABLES (`var(--nk-<name>)`), published once at boot by
`injectCssVars()` in `main.js` (tokens.js stays the single source of
truth; markup attributes keep using the JS constants).
