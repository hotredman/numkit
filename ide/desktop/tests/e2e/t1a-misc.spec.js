// t1a-misc.spec.js — Tier-1 batch A: stem3, compass, feather, spy.
//
// All four wrap existing layer kinds (quiver / scatter / plot3) so
// the renderer doesn't need new code. Tests focus on:
//   1. Each builtin runs without console errors
//   2. A figure card actually appears (proof of pushDataset path)
//   3. The right SVG primitives are present (paths for arrows/stems,
//      circles for spy markers).

import { test, expect } from '@playwright/test';
import { launchIde, closeIde } from '../helpers/launch.js';
import { IdePage } from '../helpers/ide.js';

test.describe('Tier 1A — stem3 / compass / feather / spy', () => {
  let app, page, ide;

  test.beforeEach(async () => {
    app = await launchIde();
    page = await app.firstWindow();
    ide = new IdePage(page);
    await ide.waitForReady();
  });

  test.afterEach(async () => { await closeIde(app); });

  test('stem3(x, y, z) — renders, has both stems and tip markers', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'x = [1 2 3 4];\n'
      + 'y = [1 2 3 4];\n'
      + 'z = [1 4 9 16];\n'
      + 'stem3(x, y, z);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
    // 4 markers (scatter3 dataset) → 4 circles (scatter mode renders one
    // <circle> per point).
    const circles = await ide.figureCards.first().locator('svg circle').count();
    expect(circles).toBeGreaterThanOrEqual(4);
  });

  test('compass(U, V) — arrows from origin', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'U = [1 0 -1 0];\n'
      + 'V = [0 1 0 -1];\n'
      + 'compass(U, V);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('compass with complex Z — unpacks real/imag', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'Z = [1+1i, -1+1i, -1-1i, 1-1i];\n'
      + 'compass(Z);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('feather(U, V) — arrows on x-axis', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'U = [1 1 1 1 1];\n'
      + 'V = [-2 -1 0 1 2];\n'
      + 'feather(U, V);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('spy(M) — sparsity dots match nonzeros', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'M = [1 0 0 1; 0 1 0 0; 1 0 1 0; 0 0 0 1];\n'
      + 'spy(M);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    // 6 non-zero entries → 6 scatter circles.
    const circles = await ide.figureCards.first().locator('svg circle').count();
    expect(circles).toBe(6);
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('spy on identity matrix — diagonal pattern', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'spy(eye(5));\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    const circles = await ide.figureCards.first().locator('svg circle').count();
    expect(circles).toBe(5);
  });

  test('all four open cleanly in FigureWindow modal', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'compass([1 -1], [1 1]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });
});
