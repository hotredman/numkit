// b1-errorbar.spec.js — first B1 overlay relative.
//
// errorbar(x, y, e)              — symmetric error bars
// errorbar(x, y, neg, pos)       — asymmetric
//
// We don't pixel-diff; we assert structural correctness:
//   1. A figure card appears (engine emitted FIGURE_DATA)
//   2. The SVG has the right number of bar segments (3 lines per
//      point: vertical + 2 caps) and the centre dot
//   3. No renderer console errors

import { test, expect } from '@playwright/test';
import { launchIde, closeIde } from '../helpers/launch.js';
import { IdePage } from '../helpers/ide.js';

test.describe('B1 — errorbar', () => {
  let app, page, ide;

  test.beforeEach(async () => {
    app = await launchIde();
    page = await app.firstWindow();
    ide = new IdePage(page);
    await ide.waitForReady();
  });

  test.afterEach(async () => { await closeIde(app); });

  test('errorbar(x, y, e) — symmetric → figure card with bars + caps', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'errorbar([1 2 3 4 5], [1 2 3 4 5], [0.2 0.3 0.2 0.4 0.3]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });

    const card = ide.figureCards.first();
    // 5 data points × (1 vertical bar + 2 caps) = 15 line elements
    // (axes themselves are <line> too; we just check ≥ 15 from the bars).
    const lines = await card.locator('svg line').count();
    expect(lines, `errorbar drew ${lines} <line>s; expected ≥ 15 (5 pts × 3 segments)`)
      .toBeGreaterThanOrEqual(15);

    // 5 centre dots (circles). Heatmap legend may add more if present;
    // for a pure errorbar plot it's exactly 5.
    const circles = await card.locator('svg circle').count();
    expect(circles).toBe(5);
  });

  test('errorbar(x, y, neg, pos) — asymmetric draws different upper/lower', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'errorbar([1 2 3], [10 20 30], [0.5 1 1.5], [2 3 4]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });

    const card = ide.figureCards.first();
    // 3 points × 3 segments = 9 lines from errorbar; assert ≥ 9.
    expect(await card.locator('svg line').count()).toBeGreaterThanOrEqual(9);
    expect(await card.locator('svg circle').count()).toBe(3);
  });

  test('errorbar in a composite (line + errorbar overlay)', async () => {
    // hold on path: errorbar layered over a plot is the typical
    // MATLAB usage pattern.
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([1 2 3 4 5], [2 3 5 4 6]);\n'
      + 'hold on;\n'
      + 'errorbar([1 2 3 4 5], [2 3 5 4 6], [0.2 0.3 0.2 0.3 0.2]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });

    // No renderer errors.
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });
});
