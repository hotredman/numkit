# graphics.plot-family — logical/integer/single inputs rejected ("Not a double array") across the plot family

- **Status:** ✅ FIXED (2c5a18370, 2026-09-06)
- **Kind:** bug
- **Severity:** P2 missing feature (textbook-breaking: `plot(t, t >= 0)` is the canonical step-response form)
- **Found:** 2026-09-06 via springer-math book scripts pr1_2/pr1_3 (after the clear-all fix let them reach the plot call)

## Symptom

Every plot-family function that read user data via raw `doubleData()`
rejected logical / integer / single inputs with "Not a double array":
plot, stem, stairs, bar, barh, area, plot3, stem3, surf, mesh, waterfall,
contour, imagesc, quiver, errorbar, pie. MATLAB converts any numeric
input to double at the boundary — `plot(t, t >= 0)` is in every textbook.

## Repro

```matlab
clear;
t = -5:0.01:5; u = (t >= 0);
plot(t, u);
% numkit (pre-fix): "Not a double array (in call to 'plot')"
% MATLAB R2025b: step plot drawn, u treated as double 0/1
```

## Root cause

The marshaling helpers assumed DOUBLE storage: `vecToJson`
(src/graphics/src/plots/helpers.cpp) looped `v.doubleData()[i]`, and the
per-family impls had ~80 direct `Arg->doubleData()[idx]` /
pointer-grab reads on user input.

## Fix

Boundary conversion to the generic element accessor:
- `vecToJson` keeps a fast path for plain double storage, otherwise
  reads `v.elemAsDouble(i)` (same conversion MATLAB applies);
- all user-input `doubleData()[idx]` sites across plots/*.cpp converted
  to `elemAsDouble(idx)`; pointer-grab sites (bar/area/TRI matrices,
  contour/surf `levels` vectors) materialize a `std::vector<double>`
  via `elemAsDouble` first.
(scatter already used a generic path — unaffected.)

Live guards: `FigureEngineTest.PlotFamilyAcceptsLogicalInput`,
`PlotLogicalValuesConvertToDouble`, `PlotIntegerInputConvertsToDouble`
(src/graphics/tests/figure_test.cpp) — full-family sweep + value-level
assertions on the emitted dataset JSON.

## References

- src/graphics/src/plots/{helpers,line,bar,contour,surface,shared,layout,polar}.cpp
- Verified sweep (all ok post-fix): plot stem stairs scatter bar barh
  area plot3 stem3 compass polarplot surf contour quiver imagesc mesh
  waterfall pie errorbar bar(x,y) contour(x,y,z,lev)
