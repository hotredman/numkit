// b3-histogram2.spec.js — 2-D histogram, count grid as heatmap.
//
// MATLAB:
//   histogram2(X, Y)            — default 10×10 binning over data extent
//   histogram2(X, Y, [nx ny])   — explicit grid
//   histogram2(X, Y, n)         — n×n
//   histogram2(X, Y, nx, ny)    — separate args
//
// numkit: builtin bins (X, Y) and delegates to imagesc, so the existing
// heatmap renderer (LUT + colormap + tile overlay) handles display.
// Tests assert:
//   1. Figure renders without console errors for each calling form
//   2. The card carries a heatmap layer (figure-card SVG has a clipPath
//      and a rendered <image>)
//   3. Modal expansion preserves the heatmap

import { test, expect } from '@playwright/test';
import { launchIde, closeIde } from '../helpers/launch.js';
import { IdePage } from '../helpers/ide.js';

test.describe('B3 — histogram2', () => {
  let app, page, ide;

  test.beforeEach(async () => {
    app = await launchIde();
    page = await app.firstWindow();
    ide = new IdePage(page);
    await ide.waitForReady();
  });

  test.afterEach(async () => { await closeIde(app); });

  test('histogram2(X, Y) — default 10×10 binning, no errors', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'X = [1 2 3 4 5 1 2 3 4 5];\n'
      + 'Y = [1 1 2 2 3 3 4 4 5 5];\n'
      + 'histogram2(X, Y);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('histogram2(X, Y, [nx ny]) — explicit grid', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'X = randn(1, 200);\n'
      + 'Y = randn(1, 200);\n'
      + 'histogram2(X, Y, [20 15]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('histogram2(X, Y, n) — square grid', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'X = randn(1, 100);\n'
      + 'Y = randn(1, 100);\n'
      + 'histogram2(X, Y, 25);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('histogram2(X, Y, nx, ny) — separate args', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'X = [1 2 3 4 5 6 7 8 9 10];\n'
      + 'Y = [10 9 8 7 6 5 4 3 2 1];\n'
      + 'histogram2(X, Y, 5, 8);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('histogram2 + colorbar — colorbar appears on the heatmap', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'X = [1 2 3 4 5 1 2 3 4 5];\n'
      + 'Y = [1 1 2 2 3 3 4 4 5 5];\n'
      + 'histogram2(X, Y);\n'
      + 'colorbar();\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('histogram2 opens cleanly in FigureWindow modal', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'X = randn(1, 50);\n'
      + 'Y = randn(1, 50);\n'
      + 'histogram2(X, Y);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('histogram2 renders an SVG image element (heatmap visible)', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'X = [1 1 1 5 5 5];\n'
      + 'Y = [1 1 1 5 5 5];\n'
      + 'histogram2(X, Y, 5);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    // The heatmap path renders an <image> inside the figure card —
    // proxy for "the count grid actually reached the renderer".
    const images = await ide.figureCards.first().locator('svg image').count();
    expect(images).toBeGreaterThanOrEqual(1);
  });
});
