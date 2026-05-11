// Diagnostic + assertion: histogram_equalization-style 2x2 subplot with
// imshow on top row + imhist on bottom row. The bug was that the
// preview card's bars compressed to a 1-pixel strip on the left because
// SubplotGrid's per-cell viewport state captured the inherited xRange
// from cell 2 ([0.5, 64.5]) before imhist pushed bar data and updated
// cell 3's xRange to [-0.04, 1.04]. Fix: SubplotGrid re-inits the
// viewport for any cell whose previous default no longer matches the
// current default and the user hasn't pan/zoomed away from the default.

import { test, expect } from '../../helpers/shared.js';

test.describe('histogram_equalization preview', () => {
  test('preview shows bar histogram, not 1-px strip', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + '[X, Y] = meshgrid(linspace(0, 1, 64));\n'
      + 'low = 0.3 + 0.25 * (0.5 * X + 0.5 * Y);\n'
      + 'eq = histeq(low);\n'
      + 'figure;\n'
      + 'subplot(2,2,1); imshow(low); title("Low-contrast input");\n'
      + 'subplot(2,2,2); imshow(eq);  title("After histeq");\n'
      + 'subplot(2,2,3); imhist(low); title("Input histogram (narrow)");\n'
      + 'subplot(2,2,4); imhist(eq);  title("Output histogram (spread)");\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await page.waitForTimeout(400);

    // Inspect the preview card. Each cell becomes its own SVG inside
    // .fp-card. Cells 3 and 4 are the bar histograms.
    const cellSvgs = page.locator('.fp-card svg');
    const total = await cellSvgs.count();
    expect(total).toBeGreaterThanOrEqual(4);

    // Bar layer renders as <rect> elements. With the bug, the bars
    // compress to a single ~1-px column at x=0 — visually one strip.
    // After the fix, 64 distinct bars with monotonically increasing x.
    const xCoords = await page.locator('.fp-card svg rect').evaluateAll((els) => {
      return els
        .filter((el) => {
          // Skip the cell background rect (full-width, full-height).
          const w = parseFloat(el.getAttribute('width') || '0');
          const h = parseFloat(el.getAttribute('height') || '0');
          return w > 0 && w < 50 && h > 0;
        })
        .map((el) => parseFloat(el.getAttribute('x') || '0'))
        .filter(Number.isFinite);
    });
    // We expect bars to spread across the cell horizontally — span > 30px
    // is the simplest way to detect "not collapsed to a strip" without
    // depending on the cell's exact pixel width.
    const span = xCoords.length > 0
      ? Math.max(...xCoords) - Math.min(...xCoords)
      : 0;
    expect(xCoords.length, 'no bar rects in preview card').toBeGreaterThan(10);
    expect(span, `bars span only ${span}px — collapsed to a strip`).toBeGreaterThan(30);
  });
});
