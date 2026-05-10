// area-stacked.spec.js — area(Y) where Y is a matrix → stacked series.
//
// Each column becomes one band. We emit datasets in reverse column
// order so the topmost band paints first and lower bands overdraw
// the bottom of higher bands, producing the classic stacked-area
// visual without changing the area-render path.

import { test, expect } from '@playwright/test';
import { launchIde, closeIde } from '../helpers/launch.js';
import { IdePage } from '../helpers/ide.js';

test.describe('area — stacked multi-series', () => {
  let app, page, ide;

  test.beforeEach(async () => {
    app = await launchIde();
    page = await app.firstWindow();
    ide = new IdePage(page);
    await ide.waitForReady();
  });

  test.afterEach(async () => { await closeIde(app); });

  test('area(Y) with vector Y — single series (back-compat)', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'area([1 2 3 2 1]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('area(Y) with matrix Y — multiple area paths drawn', async () => {
    await ide.runScript(
      'import compat.*;\n'
      // 5 rows × 3 cols — 3 stacked series.
      + 'Y = [1 2 3; 2 3 4; 3 4 5; 2 3 4; 1 2 3];\n'
      + 'area(Y);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    // Each stacked column is its own area dataset → at least 3 filled
    // SVG paths under the data-layer group.
    const card = ide.figureCards.first();
    const filled = await card.locator('svg path[fill]:not([fill="none"])').count();
    expect(filled).toBeGreaterThanOrEqual(3);
  });

  test('area(x, Y) with explicit x vector + matrix Y', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'x = linspace(0, 1, 5);\n'
      + 'Y = [1 2; 2 3; 3 4; 2 3; 1 2];\n'
      + 'area(x, Y);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('area(Y) — single column matrix is treated as vector', async () => {
    await ide.runScript(
      'import compat.*;\n'
      // 5×1 — looks like a column vector; renderer should fall through
      // to single-series path (Yc=1 means "stacked" branch is skipped).
      + 'Y = [1; 2; 3; 4; 5];\n'
      + 'area(Y);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('area stacked opens cleanly in modal', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'Y = [1 1 1; 2 2 2; 3 3 3];\n'
      + 'area(Y);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });
});
