# Svelte architecture rules (library-first, no page-level HTML)

*Living rule (user decision, 2026-09-12).* Applies to `play/` today and
to every future Svelte surface (the IDE migration rides the same
library).

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

## Layout

```
play/src/
  App.svelte            ← page: composition ONLY (R1–R2)
  lib/
    tokens.js           ← design tokens (R4)
    ui/                 ← layout & controls (Panel, Button, Slider…)
    plot/               ← figure renderer: one chart type per file (R3)
      wire.js           ← pure: __FIGURE_DATA__ parsing (R5)
      plotModel.js      ← pure: lims/scales/series math (R5)
      Figure.svelte     ← root: dispatches cartesian/polar/…
      PlotCartesian.svelte
      PlotPolar.svelte
      PlotLegend.svelte
```

## Enforcement

Review-time (this file is the checklist) + a mechanical guard is
planned: a tiny vitest scan asserting page files contain no `<div`,
`style=` or `<style` tokens (lib/ is exempt). Add it the moment a
second page appears.

## D1 enforcement state (2026-09-12)

The refactor landed: `App.svelte` is pure composition (grep for
`<div|style=|<style` → 0), the library lives in `lib/ui` (10 components)
+ `lib/plot` (Figure root + PlotCartesian + PlotPolar + PlotLegend,
pure `wire.js` / `plotModel.js`). TECHNICALITY discovered: Svelte
`<style>` blocks are STATIC — no JS interpolation — so tokens reach
them as CSS VARIABLES (`var(--nk-<name>)`), published once at boot by
`injectCssVars()` in `main.js` (tokens.js stays the single source of
truth; markup attributes keep using the JS constants).
