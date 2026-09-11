# graphics.handles — deferred property/return surface of the handle layer

- **Status:** 🔴 OPEN (tracking stub — portion 29 shipped the core layer)
- **Severity:** P3 (each item is a documented MATLAB behaviour numkit
  accepts-but-ignores or approximates; none breaks the common path)
- **Kind:** stub
- **Found:** 2026-09-07, portion 29 close-out review.

## What is deferred

Portion 31 CLOSED items 1-3 and 8: title/xlabel/ylabel/zlabel/sgtitle/
subtitle return matlab.graphics.primitive.Text handles (String live
round-trip, FontSize 11 parity-verified), legend returns
matlab.graphics.illustration.Legend (Location/Box/Visible with
render-effect, `legend off/show/hide/on` preserve labels), groot is
matlab.ui.Root (figure.Parent chain, Children = figure array),
gcf.Children is the full ordered axes array, and get(gca,'Title'/
'XLabel'/…) returns Text handles. Plus: setSubplot adopts the default
axes (bugs/closed/graphics/subplot-replaces-default-axes.md), and the
indexed dotted form `gca.Parent.Children(2)` works on both engines
(call-then-fields + index). Still NOT MATLAB-complete:

1. ~~title/xlabel/legend return values~~ — CLOSED in portion 31.
2. ~~Root object / groot~~ — CLOSED in portion 31.
3. ~~gcf.Children full array~~ — CLOSED in portion 31.
4. **Chart class construction** — `matlab.graphics.chart.primitive.Line()`
   called directly errors instead of constructing.
5. **`get(h,'ZData')`** on surface/contour records (2-D matrix payload —
   needs a nested-JSON matrix decoder).
6. **`set(h)` listing output** — MATLAB prints the settable property
   catalogue; numkit is silent.
7. **Axes/figure property breadth** — only the render-effective subset
   is mapped; Text FontSize/Color/FontWeight on titles/labels store
   per-record without render effect (renderer has no per-label font
   field in the wire format yet).
8. ~~get(gca,'XLabel') returns a string~~ — CLOSED in portion 31.

## Repro (self-contained)

```matlab
clear;
h = plot(1:5);
matlab.graphics.chart.primitive.Line();   % numkit: error; MATLAB: constructs
s = surf(peaks); get(s, 'ZData');         % numkit: ''; MATLAB: 49x49 matrix
set(h);                                   % numkit: silent; MATLAB: property listing
```

## References

- **Guard:** deferred (Kind: stub — no DISABLED_ gtest per bugs/README
  convention for non-behavioural kinds). Core behaviours are guarded in
  `TW_VM/GraphicsHandleTest.*` (src/graphics/tests/graphics_handle_test.cpp).
