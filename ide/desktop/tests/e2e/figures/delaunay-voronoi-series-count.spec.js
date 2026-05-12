// delaunay-voronoi-series-count.spec.js — verify the canonical
// MATLAB-parity series count.
//
//   delaunay(x, y) — returns Mx3 indices, no plotting
//   triplot(tri, x, y, ...) — plots ALL triangle edges as ONE series
//                             (null-separated polyline). One legend entry.
//   voronoi(x, y) — plots edges as ONE line series + the points as ONE
//                   scatter series. Two series total.
//
// Bug we're guarding against: the old delaunay_voronoi.m example used a
// for-loop over `plot()` per triangle, producing M independent series
// (one per triangle) — bloated the figure JSON, broke the legend, and
// made restyling impossible.

import { test, expect } from '../../helpers/shared.js';

async function seriesCount(page) {
  // Walk React fiber on the figure card to read figure.cells[0].layers
  // (or for a non-subplot figure, figure.layers / figure.series).
  return await page.evaluate(() => {
    const card = document.querySelector('.fp-card');
    if (!card) return -1;
    const fiberKey = Object.keys(card).find((k) => k.startsWith('__reactFiber'));
    let fib = card[fiberKey];
    while (fib && !(fib.memoizedProps && fib.memoizedProps.figure)) fib = fib.return;
    if (!fib) return -2;
    const fig = fib.memoizedProps.figure;
    const layers = Array.isArray(fig.layers) ? fig.layers
                : Array.isArray(fig.cells) && fig.cells[0]?.layers ? fig.cells[0].layers
                : [];
    return layers.filter((l) => l.kind === 'series').length;
  });
}

test.describe('delaunay + voronoi series count (MATLAB parity)', () => {
  test('triplot(tri, x, y) → ONE series, not M', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'rng(7);\n'
      + 'x = rand(1, 15) * 4;\n'
      + 'y = rand(1, 15) * 4;\n'
      + 'tri = delaunay(x, y);\n'
      + 'figure;\n'
      + 'triplot(tri, x, y, \'b-\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await page.waitForTimeout(300);
    const n = await seriesCount(page);
    expect(n, `triplot drew ${n} series — expected 1 (single null-separated polyline)`)
      .toBe(1);
  });

  test('triplot + scatter overlay → TWO series', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'rng(7);\n'
      + 'x = rand(1, 15) * 4;\n'
      + 'y = rand(1, 15) * 4;\n'
      + 'tri = delaunay(x, y);\n'
      + 'figure;\n'
      + 'triplot(tri, x, y, \'b-\');\n'
      + 'hold on;\n'
      + 'plot(x, y, \'r.\', \'MarkerSize\', 8);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await page.waitForTimeout(300);
    const n = await seriesCount(page);
    expect(n, `triplot + plot drew ${n} series — expected 2`).toBe(2);
  });

  test('voronoi(x, y) → TWO series (edges + points), not many', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'rng(7);\n'
      + 'x = rand(1, 15) * 4;\n'
      + 'y = rand(1, 15) * 4;\n'
      + 'voronoi(x, y);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await page.waitForTimeout(300);
    const n = await seriesCount(page);
    expect(n, `voronoi drew ${n} series — expected 2 (edges + points)`).toBe(2);
  });
});
