# graphics.export-raster — every glyph after the uppercase region rendered +3 shifted ("time" → "wlph")

- **Status:** ✅ FIXED (portion 34, commit 6628a04ae)
- **Severity:** P3 (all raster-PNG text was subtly wrong; SVG export and the
  IDE renderer were unaffected — they use real font rendering)
- **Kind:** bug
- **Found:** 2026-09-11 portion 34 — the side-by-side MATLAB-vs-numkit visual
  comparison of the axes-props demo script. The analyzer OCR'd the numkit PNG
  labels as `wlph`/`Ydohxh` where MATLAB had `time`/`value`: every letter was
  exactly +3 in the alphabet.

## Symptom

`saveas(gcf, 'x.png')` on any figure with text: tick labels, axis labels,
titles and legend strings all render as the glyph three positions later in
the font table (`t`→`w`, `i`→`l`, `S`→`V`). Letters near the end of the
alphabet (`x`, `y`, `z`) render blank.

## Repro (self-contained)

```matlab
clear;
plot(1:10);
xlabel('time');
saveas(gcf, [tempdir 'font_probe.png']);
% numkit (before fix): the xlabel reads "wlph"
% MATLAB R2025b: the xlabel reads "time"
```

## Root cause

`font5x7()` in `figure_render.hpp` declared `static const std::uint8_t
F[95][5]` but its initializer listed only **92** rows (three glyphs dropped
from the uppercase region of a hand-typed variant table). C++ zero-fills the
remainder silently, so the table compiled clean — but every glyph after the
gap sat at `index = char - 0x20 - 3`... i.e. looking up `'t'` returned the
`'w'` bitmap, and lookups past the 92 initialized rows hit zero rows (blank).

## Fix (portion 34)

- The table is replaced wholesale with the canonical public-domain 5x7 font
  (Adafruit `glcdfont.c`, chars 0x20..0x7E — same column encoding, LSB =
  top row, byte-verified against the reference, 1:1, no transcription).
  Five reference glyphs carry descender pixels in bit 7 (`g p q y ,`);
  the raster draws bits 0..6, so descenders clip — a cosmetic known limit,
  not a shift.
- The array is now unsized `F[][5]` with
  `static_assert(sizeof F / sizeof F[0] == 95)`, so a dropped row is a
  compile error instead of a silent glyph shift.

## References

- **Guard:** `FigureExportTest.RasterFontGlyphAlignment` (dual-engine,
  enabled) in `src/graphics/tests/figure_export_test.cpp` — pins glyph
  identity for 't'/'I'/'8'/'A' and asserts `'t'` is NOT the 'w' bitmap.
