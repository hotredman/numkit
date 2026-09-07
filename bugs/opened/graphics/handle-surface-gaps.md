# graphics.handles — deferred property/return surface of the handle layer

- **Status:** 🔴 OPEN (tracking stub — portion 29 shipped the core layer)
- **Severity:** P3 (each item is a documented MATLAB behaviour numkit
  accepts-but-ignores or approximates; none breaks the common path)
- **Kind:** stub
- **Found:** 2026-09-07, portion 29 close-out review.

## What is deferred

The portion-29 handle layer ships: chart handles for the plot family,
set/get (+dot-syntax) with defaults and render-effect for the common
props, gca/gcf/figure handles, unified delete, eq/ne, get(h) struct.
Explicitly NOT yet MATLAB-complete:

1. **`hT = title(...)` / `hX = xlabel(...)` / `hL = legend(...)` return
   values** — MATLAB returns Text/Legend objects; numkit still returns
   the legacy no-op value. Needs Text/Legend handle kinds + property maps.
2. **Root object** — `groot` / `figure.Parent` above the Figure level.
3. **`gcf.Children`** — returns the CURRENT axes handle when any axes
   exist; MATLAB returns the full axes ARRAY (ordered).
4. **Chart class construction** — `matlab.graphics.chart.primitive.Line()`
   called directly errors instead of constructing.
5. **`get(h,'ZData')`** on surface/contour records (2-D matrix payload —
   needs a nested-JSON matrix decoder).
6. **`set(h)` listing output** — MATLAB prints the settable property
   catalogue; numkit is silent.
7. **Axes/figure property breadth** — only the render-effective subset
   (XLim/YLim/ZLim/XScale/YScale/grids/labels/Title, Number/Type/
   CurrentAxes) is mapped; everything else stores per-record without
   render effect (e.g. FontSize, Position, OuterPosition).
8. **`get(gca,'XLabel')`** returns the label STRING; MATLAB returns a
   Text handle.

## Repro (self-contained)

```matlab
clear;
h = plot(1:5);
hT = title('t');
% numkit: hT is the legacy no-op value; MATLAB R2025b: 1x1 Text object
disp(get(gca, 'XLabel'));
% numkit: 't' (char); MATLAB: 1x1 Text object (String 't')
```

## References

- **Guard:** deferred (Kind: stub — no DISABLED_ gtest per bugs/README
  convention for non-behavioural kinds). Core behaviours are guarded in
  `TW_VM/GraphicsHandleTest.*` (src/graphics/tests/graphics_handle_test.cpp).
