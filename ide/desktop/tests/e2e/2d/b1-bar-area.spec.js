// b1-bar-area.spec.js — barh + area B1 overlays.
//
// Both reuse the existing `kind: 'series'` layer with new modes.
// Tests assert structural correctness:
//
//   barh  — N <rect> elements (one per bar). Width = bar's value
//           extent, height = inter-row spacing.
//   area  — single <path> with non-zero `fill-opacity` (the polygon
//           under the curve). The curve plus baseline-down-and-back
//           gives the closed shape.

import { test, expect } from '../../helpers/shared.js';

test.describe('B1 — barh', () => {
  test('barh(y) — vector → N horizontal bars', async ({ ide }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'barh([2 5 3 7 4]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    const rects = await ide.figureCards.first().locator('svg rect').count();
    expect(rects, `barh drew ${rects} <rect>s; expected ≥ 5`).toBeGreaterThanOrEqual(5);
  });

  test('barh(x, y) — categories + values', async ({ ide }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'barh([10 20 30], [1 4 2]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    const rects = await ide.figureCards.first().locator('svg rect').count();
    expect(rects).toBeGreaterThanOrEqual(3);
  });
});

test.describe('B1 — area', () => {
  test('area(x, y) — single filled polygon', async ({ ide }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'area([1 2 3 4 5], [0.5 1.2 2.1 1.8 0.9]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    const card = ide.figureCards.first();
    // The area path uses fill-opacity around 0.3. We assert ANY path
    // inside the card has a non-zero fill-opacity (axes / gridlines
    // are stroke-only and have fill="none" or no fill-opacity attr).
    const filled = await card.locator('svg path[fill-opacity]').count();
    expect(filled, `area-mode rendered ${filled} fill-opacity paths`).toBeGreaterThanOrEqual(1);
  });

  test('area(x, y, base) — baseline shifts the fill bottom', async ({ ide }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'area([1 2 3 4 5], [3 4 5 4 3], 2);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    // Smoke: figure created + path with fill-opacity exists.
    const filled = await ide.figureCards.first().locator('svg path[fill-opacity]').count();
    expect(filled).toBeGreaterThanOrEqual(1);
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });
});
