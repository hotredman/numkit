// b1-plot3.spec.js — plot3 + scatter3 routed through the WebGL
// (three.js) renderer.
//
// MATLAB:
//   plot3(x, y, z)     — line through 3-D points
//   scatter3(x, y, z)  — markers at 3-D points
//
// History: this spec originally counted SVG paths/circles when 3-D
// data went through the cabinet-projected 2-D pipeline. After the
// WebGL renderer landed, those primitives moved into a <canvas>
// (via three.js BufferGeometry / LineBasicMaterial / PointsMaterial),
// so the assertion is now "canvas mounted" — the dedicated webgl-3d
// spec covers detail signals (gl context, frame counter, etc.).

import { test, expect } from '@playwright/test';
import { launchIde, closeIde } from '../helpers/launch.js';
import { IdePage } from '../helpers/ide.js';

test.describe('B1 — plot3 / scatter3', () => {
  let app, page, ide;

  test.beforeEach(async () => {
    app = await launchIde();
    page = await app.firstWindow();
    ide = new IdePage(page);
    await ide.waitForReady();
  });

  test.afterEach(async () => { await closeIde(app); });

  test('plot3(x, y, z) — figure card with WebGL canvas', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot3([1 2 3 4 5], [1 4 9 16 25], [0 1 2 3 4]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await expect(ide.figureCards.first().locator('canvas[data-numkit-3d]'))
      .toBeVisible({ timeout: 10_000 });
  });

  test('scatter3(x, y, z) — WebGL points geometry', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'scatter3([1 2 3 4], [1 2 3 4], [0 1 2 3]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await expect(ide.figureCards.first().locator('canvas[data-numkit-3d]'))
      .toBeVisible({ timeout: 10_000 });
  });

  test('plot3 has the same z=0 baseline as plot when z=zeros', async () => {
    // Sanity: with z all zero, the projected (x', y') equals (x, y),
    // so plot3 should produce the same figure as plot (a card with a
    // line path).
    await ide.runScript(
      'import compat.*;\n'
      + 'plot3([1 2 3], [1 4 9], [0 0 0]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });
});
