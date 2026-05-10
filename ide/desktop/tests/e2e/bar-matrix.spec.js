// bar-matrix.spec.js — bar(Y) where Y is a matrix → grouped /
// stacked multi-series bar chart.

import { test, expect } from '@playwright/test';
import { launchIde, closeIde } from '../helpers/launch.js';
import { IdePage } from '../helpers/ide.js';

test.describe('bar — matrix Y (grouped + stacked)', () => {
  let app, page, ide;

  test.beforeEach(async () => {
    app = await launchIde();
    page = await app.firstWindow();
    ide = new IdePage(page);
    await ide.waitForReady();
  });

  test.afterEach(async () => { await closeIde(app); });

  test('bar(vector) — single series back-compat', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'bar([1 2 3 4 5]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('bar(matrix) — grouped (default), one dataset per column', async () => {
    await ide.runScript(
      'import compat.*;\n'
      // 3 rows × 3 cols → 3 grouped bar series.
      + 'Y = [1 2 3; 4 5 6; 7 8 9];\n'
      + 'bar(Y);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    // Each grouped series is its own dataset → SVG rect count
    // should be Yr * Yc = 9 rects (or more, accounting for any
    // axis-decoration rects).
    const card = ide.figureCards.first();
    const rects = await card.locator('svg rect').count();
    expect(rects).toBeGreaterThanOrEqual(9);
  });

  test('bar(matrix, "stacked") — cumulative-sum stacked bars', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'Y = [1 2 3; 4 5 6];\n'
      + 'bar(Y, \'stacked\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('bar(x, matrix) — explicit x with matrix Y', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'bar([10 20 30], [1 2; 3 4; 5 6]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });
});
