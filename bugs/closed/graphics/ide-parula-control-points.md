# graphics.ide-colormaps — IDE parula (and jet/hot/bone/…) control points diverged from MATLAB by up to 50/255 in the interior

- **Status:** ✅ FIXED (portion 35, commit 951ab92d1)
- **Severity:** P3 (every IDE heatmap rendered subtly wrong colors vs
  MATLAB books output — endpoints happened to match, interiors did not)
- **Kind:** bug
- **Found:** 2026-09-11 portion 35 — while porting palettes into the
  engine export renderer, the JS control-point parula was checked
  against freshly probed MATLAB anchors: t=0.5 gave
  [0.1657, 0.7240, 0.5265] (JS stops) vs MATLAB [0.0770, 0.7468,
  0.7224] — a 50/255 error in blue.

## Symptom

`ide/src/components/plot/colormaps.js` built each named colormap from
~4–11 hand-ported control points with linear interpolation. Only the
endpoints were MATLAB-exact; the interior curved differently (parula is
a spline in MATLAB, not piecewise-linear over those stops).

## Repro (self-contained)

```matlab
clear;
cm = parula(256);
disp(cm(129));        % MATLAB R2025b: 0.0770 0.7468 0.7224
% numkit IDE before fix: rgb(42,185,134) = 0.1657 0.7240 0.5265
```

## Fix (portion 35)

colormaps.js now resolves named maps through
`ide/src/components/plot/colormapTables.js` — 256-entry hex tables
generated from the same MATLAB R2025b dump (`feval(<name>, 256)`) that
the engine export renderer baked into `figure_render.hpp`, so IDE and
export render byte-identical colors. Lookup uses MATLAB's
`round(t*(N-1))` entry indexing. viridis stays the matplotlib-approx
table (MATLAB R2025b has no viridis(); exist()==0) — now sampled to 256
once so engine and IDE agree there too.

## References

- **Guard:** `colormapTables.test.js` — five parula anchors + jet/hot/
  gray mid entries + endpoint/roundtrip checks (568-test frontend
  suite).
