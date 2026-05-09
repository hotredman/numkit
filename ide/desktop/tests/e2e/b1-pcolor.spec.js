// b1-pcolor.spec.js — pcolor: like imagesc but (x, y) are cell
// vertices, not cell centres. Rendering reuses the heatmap layer
// pipeline; only the autoscaled xRange/yRange differs (no half-cell
// padding for pcolor).
//
// Tests assert that pcolor produces a heatmap-style image element
// the same way imagesc does, and that the figure renders without
// console errors.

import { test, expect } from '@playwright/test';
import { launchIde, closeIde } from '../helpers/launch.js';
import { IdePage } from '../helpers/ide.js';

test.describe('B1 — pcolor', () => {
  let app, page, ide;

  test.beforeEach(async () => {
    app = await launchIde();
    page = await app.firstWindow();
    ide = new IdePage(page);
    await ide.waitForReady();
  });

  test.afterEach(async () => { await closeIde(app); });

  test('pcolor(C) — basic 3×3 grid → heatmap image', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'pcolor([1 2 3; 4 5 6; 7 8 9]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    const images = await ide.figureCards.first().locator('svg image').count();
    expect(images, `pcolor rendered ${images} <image> elements`).toBeGreaterThanOrEqual(1);
  });

  test('pcolor(X, Y, C) — vertex-aligned coordinates', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'pcolor([0 1 2 3], [0 1 2 3], [1 2 3 4; 5 6 7 8; 9 10 11 12; 13 14 15 16]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('pcolor + quiver overlay (composite)', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'pcolor([1 2 3], [1 2 3], [1 2 3; 4 5 6; 7 8 9]);\n'
      + 'hold on;\n'
      + 'quiver([1 2 3], [1 2 3], [0.3 0.3 0.3], [0.3 0.3 0.3]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    const card = ide.figureCards.first();
    expect(await card.locator('svg image').count()).toBeGreaterThanOrEqual(1);
    expect(await card.locator('svg line').count()).toBeGreaterThanOrEqual(9);
  });
});
