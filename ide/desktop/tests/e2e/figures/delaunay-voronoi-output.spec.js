// delaunay-voronoi-output.spec.js — visual smoke for the
// Computational_Geometry/delaunay_voronoi.m example.
//
// User-reported issues:
//   1. Delaunay figure shows extra red lines connecting all points
//      (the `plot(x, y, 'r.', 'MarkerSize', 8)` call should produce
//      red DOTS only — but if linespec marker-only suppression
//      regressed, points come out connected as a polyline).
//   2. Voronoi cells extend way beyond the input data extent, with
//      lines reaching x=16, y=-8 etc. — the figure's auto-axis fits
//      the wild far-away circumcenters of nearly-degenerate triangles.

import { test, expect } from '../../helpers/shared.js';

const NON_FATAL = (e) =>
  !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e);

async function snapshot(page, name) {
  await page.screenshot({
    path: `test-results/_diag/${name}.png`,
    fullPage: false,
  });
}

test.describe('delaunay + voronoi output', () => {
  test('plot(x, y, \'r.\') — markers only, no connecting line', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'rng(7);\n'
      + 'x = rand(1, 15) * 4;\n'
      + 'y = rand(1, 15) * 4;\n'
      + 'plot(x, y, \'r.\', \'MarkerSize\', 8);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    await page.waitForTimeout(300);
    await snapshot(page, 'plot-r-dot');

    // Path elements with non-trivial `d` attribute = the would-be polyline.
    const longPaths = await page.locator('.fw-window .fw-canvas-wrap path').evaluateAll(
      (els) => els.filter((el) => {
        const d = el.getAttribute('d') || '';
        // Only count line-paths longer than a marker glyph (~50 chars).
        return d.length > 100 && el.getAttribute('fill') === 'none';
      }).length
    );
    expect(longPaths,
      `'r.' linespec drew ${longPaths} long polyline path(s) — should be 0 (markers only)`
    ).toBe(0);
    expect(ide.devErrors().filter(NON_FATAL)).toEqual([]);
  });

  test('voronoi(x, y) — viewport bounded near data extent', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'rng(7);\n'
      + 'x = rand(1, 15) * 4;\n'
      + 'y = rand(1, 15) * 4;\n'
      + 'voronoi(x, y);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    await page.waitForTimeout(300);
    await snapshot(page, 'voronoi');

    // Read the viewport y range. With x, y in [0, 4], a sane Voronoi
    // axis stays within [-1, 5] or so. If far-away circumcenters are
    // passed through unfiltered, the auto-axis blows up to e.g. y=-8.
    const inputs = page.locator('.fw-window .fw-range-row input');
    if (await inputs.count() === 4) {
      const yMin = parseFloat(await inputs.nth(2).inputValue());
      const yMax = parseFloat(await inputs.nth(3).inputValue());
      expect(yMin, `Voronoi yMin=${yMin}, yMax=${yMax} — axis blew up`).toBeGreaterThan(-2);
      expect(yMax, `Voronoi yMax=${yMax} — axis blew up`).toBeLessThan(6);
    }
    expect(ide.devErrors().filter(NON_FATAL)).toEqual([]);
  });
});
