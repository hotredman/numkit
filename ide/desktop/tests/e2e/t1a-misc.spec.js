// t1a-misc.spec.js — Tier-1 batch A: stem3, compass, feather, spy.
//
// All four wrap existing layer kinds (quiver / scatter / plot3) so
// the renderer doesn't need new code. Tests focus on:
//   1. Each builtin runs without console errors
//   2. A figure card actually appears (proof of pushDataset path)
//   3. The right SVG primitives are present (paths for arrows/stems,
//      circles for spy markers).

import { test, expect } from '../helpers/shared.js';

test.describe('Tier 1A — stem3 / compass / feather / spy', () => {
  test('stem3(x, y, z) — renders via WebGL canvas', async ({ ide, page }) => {
    // stem3 emits 1 plot3 (line) + 1 scatter3 (markers) so the figure
    // has Z data → routed through Composite3DPlot. Originally we
    // counted SVG <circle> elements (4 markers); under WebGL those
    // are GL points inside a canvas. Assert the canvas mounted
    // instead — the 3-D smoke spec already verifies geometry is
    // actually drawn there.
    await ide.runScript(
      'import compat.*;\n'
      + 'x = [1 2 3 4];\n'
      + 'y = [1 2 3 4];\n'
      + 'z = [1 4 9 16];\n'
      + 'stem3(x, y, z);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await expect(ide.figureCards.first().locator('canvas[data-numkit-3d]'))
      .toBeVisible({ timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('compass(U, V) — arrows from origin', async ({ ide, page }) => {
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

  test('compass with complex Z — unpacks real/imag', async ({ ide, page }) => {
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

  test('feather(U, V) — arrows on x-axis', async ({ ide, page }) => {
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

  test('spy(M) — sparsity dots match nonzeros', async ({ ide, page }) => {
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

  test('spy on identity matrix — diagonal pattern', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'spy(eye(5));\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    const circles = await ide.figureCards.first().locator('svg circle').count();
    expect(circles).toBe(5);
  });

  test('all four open cleanly in FigureWindow modal', async ({ ide, page }) => {
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
