# graphics.export — deferred surface of saveas / exportgraphics / print

- **Status:** 🔴 OPEN (tracking stub — portion 32 shipped the core path)
- **Severity:** P3 (each item is a documented MATLAB behaviour numkit
  refuses with a clear error or renders in simplified form)
- **Kind:** stub
- **Found:** 2026-09-11, portion 32 close-out.

## What shipped (portion 32)

Engine-side renderer (`src/figure/include/numkit/figure/figure_render.hpp`
— display list + SVG and RGB-raster backends, 5×7 bitmap font) behind
saveas / exportgraphics / print with the common argument forms (handle or
figure-number target, format flag `-dpng`, `-r###` resolution, Name-Value
accept-and-ignore) writing .svg plus every image-toolbox raster format.

## Deferred

1. **Formats**: `.pdf`, `.eps`, `.fig`, `.emf`, `.hdf` — refused with a
   clear error pointing here. (Vector PDF could ride the display list;
   .fig is a MATLAB container format — likely never.)
2. **Rendered chart types**: imagesc / image-rgb / heatmap (needs the
   colormap port + LOD sampling), 3-D surfaces / contours / quiver /
   stream, polar, text overlays, yyaxis right scale — silently skipped
   in the renderer for now (axes chrome still draws).
3. **Scoping**: exportgraphics(gca)/saveas(chart) export the containing
   FIGURE, not the tight axes region (MATLAB crops to the target).
4. **print extras**: `-append`, `-loose/-tight`, `-noui`, figure
   PaperPosition/Units sizing, `-djpg` quality levels.
5. **Aesthetic parity**: tick label layout is monospace-simple, the
   bitmap font is terminal-grade at high Resolution, and bar grouping
   collapses adjacent bar datasets into shared slots.

## Repro (self-contained)

```matlab
clear;
surf(peaks);
saveas(gcf, 's.pdf');
% numkit: error "format '.pdf' is not supported yet …" (filed above)
% MATLAB R2025b: writes a PDF
```

## References

- **Guard:** deferred (Kind: stub). Live coverage:
  `TW_VM/FigureExportTest.*` (src/graphics/tests/figure_export_test.cpp),
  parity `tools/parity/specs/figure_export.json` (contract level, OK).
