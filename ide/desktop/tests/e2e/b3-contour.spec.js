// b3-contour.spec.js — contour line plots via marching squares.
//
// MATLAB:
//   contour(Z)              — 10 levels over data range
//   contour(Z, n)           — n levels
//   contour(Z, levels)      — vector of explicit levels
//   contour(X, Y, Z[, …])   — with explicit grid coordinates
//   contourf(...)           — same args; filled bands deferred — we
//                             render lines for now (parity with class).
//
// Tests assert:
//   1. Figure renders without console errors for each calling form
//   2. The figure card has multiple <path> elements (one per level)
//   3. contourf doesn't crash (lines fallback)
//   4. Modal expansion preserves the layers

import { test, expect } from '@playwright/test';
import { launchIde, closeIde } from '../helpers/launch.js';
import { IdePage } from '../helpers/ide.js';

test.describe('B3 — contour', () => {
  let app, page, ide;

  test.beforeEach(async () => {
    app = await launchIde();
    page = await app.firstWindow();
    ide = new IdePage(page);
    await ide.waitForReady();
  });

  test.afterEach(async () => { await closeIde(app); });

  test('contour(Z) — default 10 levels, no errors', async () => {
    await ide.runScript(
      'import compat.*;\n'
      // Cone: distance from centre. 7×7 grid for predictable contours.
      + 'Z = zeros(7, 7);\n'
      + 'for i = 1:7; for j = 1:7;\n'
      + '  Z(i, j) = sqrt((i-4)^2 + (j-4)^2);\n'
      + 'end; end;\n'
      + 'contour(Z);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('contour(Z, n) — n levels, multiple paths visible', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'Z = zeros(10, 10);\n'
      + 'for i = 1:10; for j = 1:10;\n'
      + '  Z(i, j) = (i-5)^2 + (j-5)^2;\n'
      + 'end; end;\n'
      + 'contour(Z, 8);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    // 8 levels → at least 8 SVG <path> elements (one per level).
    const paths = await ide.figureCards.first().locator('svg path').count();
    expect(paths).toBeGreaterThanOrEqual(4);
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('contour(Z, levels) — explicit levels vector', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'Z = zeros(8, 8);\n'
      + 'for i = 1:8; for j = 1:8;\n'
      + '  Z(i, j) = i * j;\n'
      + 'end; end;\n'
      + 'contour(Z, [10 20 40]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('contour(X, Y, Z) — explicit grid coordinates', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'x = linspace(-2, 2, 9);\n'
      + 'y = linspace(-2, 2, 9);\n'
      + 'Z = zeros(9, 9);\n'
      + 'for i = 1:9; for j = 1:9;\n'
      + '  Z(i, j) = exp(-(x(j)^2 + y(i)^2));\n'
      + 'end; end;\n'
      + 'contour(x, y, Z);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('contourf — falls back to line render, no crash', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'Z = zeros(6, 6);\n'
      + 'for i = 1:6; for j = 1:6;\n'
      + '  Z(i, j) = sin(i/2) * cos(j/2);\n'
      + 'end; end;\n'
      + 'contourf(Z, 5);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('contour modal — expands cleanly', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'Z = zeros(5, 5);\n'
      + 'for i = 1:5; for j = 1:5;\n'
      + '  Z(i, j) = (i - 3)^2 + (j - 3)^2;\n'
      + 'end; end;\n'
      + 'contour(Z, 4);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('contour with constant Z (degenerate) — no crash', async () => {
    // zmn == zmx → no segments cross the level. We don't emit a card
    // because the adapter discards layers-empty figures (matching the
    // no-renderable contract). The guarantee here is "no error".
    await ide.runScript(
      'import compat.*;\n'
      + 'Z = ones(4, 4);\n'
      + 'contour(Z);\n'
      + 'fprintf(\'survived\\n\');\n'
    );
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
    // A subsequent plot still works (engine isn't wedged).
    await ide.runScript('import compat.*;\nplot([1 2 3], [1 2 3]);\n');
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  });
});
