# graphics.plot — `plot(x,y,x2,y2)` silently DROPS every series after the first

- **Status:** ✅ FIXED (portion 29; commit hash backfilled below)
- **Severity:** P1 (textbook multi-series form renders one line, no error)
- **Kind:** bug
- **Found:** 2026-09-07 during portion 29 (graphics-handle work) —
  `hs = plot(1:5,1:5,1:5,2:6)` returned `numel(hs) == 1` while the handle
  layer was being verified against MATLAB's 1×2 Line array.

## Symptom

The multi-series form `plot(X1,Y1,X2,Y2,…)` drew ONLY the first pair. The
remaining arguments were consumed by the N/V parser (or ignored), silently.

## Repro (self-contained)

```matlab
clear;
hs = plot(1:5, 1:5, 1:5, 2:6);
disp(numel(hs))
% numkit (before):  1   (one line rendered; second pair dropped)
% MATLAB R2025b:    2   (two Line objects, two lines rendered)
```

## Root cause

`parsePlotXYStyle` (src/graphics/src/plots/helpers.cpp) read exactly one
(x, y) pair; `parsePlotArgs` then walked the REST of the argument list as
name–value pairs, so numeric data args were skipped. Worse, a trailing
char like `'LineWidth'` could be captured as a LineSpec.

## Fix (portion 29)

The plot/plot3 bodies (src/graphics/src/plots/line.cpp) now parse the full
MATLAB grammar `(X, Y [, LineSpec] [, N/V…])*` (triples for plot3), one
DatasetInfo per series — so the adapter's handle post-hook binds one
handle per series and `hs(2)` works. Discriminator: a char argument that
is entirely linespec alphabet (`-:.o+*xsd^v<>phrgbcymw`) is a LineSpec;
any other char starts an N/V pair.

## References

- **Guard:** `TW_VM/GraphicsHandleTest.MultiSeriesPlotReturnsHandleArray`
  (dual-engine) in `src/graphics/tests/graphics_handle_test.cpp`.
