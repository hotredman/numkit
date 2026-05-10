// xline-yline.spec.js — reference lines spanning the viewport.
//
// xline(x) draws a vertical line at x=x crossing the full Y range
// of the panel. yline(y) is the horizontal counterpart. Both used
// to be no-ops; now they emit dedicated dataset types and render
// as <line> SVG elements.

import { test, expect } from '@playwright/test';
import { launchIde, closeIde } from '../helpers/launch.js';
import { IdePage } from '../helpers/ide.js';

test.describe('xline / yline reference lines', () => {
  let app, page, ide;

  test.beforeEach(async () => {
    app = await launchIde();
    page = await app.firstWindow();
    ide = new IdePage(page);
    await ide.waitForReady();
  });

  test.afterEach(async () => { await closeIde(app); });

  test('xline(5) — vertical reference line in figure', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([1 2 3 4 5 6 7], [1 4 9 16 25 36 49]);\n'
      + 'xline(5);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('yline(20) — horizontal reference line', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([1 2 3 4 5], [1 4 9 16 25]);\n'
      + 'yline(20);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('xline(vector) — multiple reference lines at once', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([1 2 3 4 5 6 7 8 9 10], 1:10);\n'
      + 'xline([2 5 8]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('xline does NOT contribute to auto-range Y bounds', async () => {
    // If xline's sentinel Y leaked into auto-range, the plot would
    // collapse to ~0 height. The data goes 1..25, so any auto-tick
    // pattern with multiple distinct numeric labels confirms a
    // healthy range scan (collapsed range yields 1-2 labels).
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([1 2 3 4 5], [1 4 9 16 25]);\n'
      + 'xline(3);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    await page.waitForTimeout(150);
    // Collect numeric labels; expect ≥ 4 distinct values when the
    // Y-range scan is healthy (a collapsed range produces ≤ 2).
    const labels = await ide.figureWindow.locator('svg text').allTextContents();
    const nums = new Set();
    for (const s of labels) {
      const n = Number(s);
      if (Number.isFinite(n)) nums.add(n);
    }
    expect(nums.size).toBeGreaterThanOrEqual(4);
  });
});
