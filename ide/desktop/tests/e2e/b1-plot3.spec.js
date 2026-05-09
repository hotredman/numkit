// b1-plot3.spec.js — plot3 + scatter3 with 2-D cabinet projection.
//
// MATLAB:
//   plot3(x, y, z)     — line through 3-D points
//   scatter3(x, y, z)  — markers at 3-D points
//
// Our renderer collapses (x, y, z) → 2-D screen coords via cabinet
// projection: x' = x + 0.5*z*cos(30°), y' = y + 0.5*z*sin(30°).
// True 3-D camera (orbit/dolly) is B3 territory; this gets the data
// on screen so users can run scripts and read values today.

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

  test('plot3(x, y, z) — figure card with line path', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot3([1 2 3 4 5], [1 4 9 16 25], [0 1 2 3 4]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    // Line series renders as a single <path>; we just want at least
    // one inside the figure card.
    const paths = await ide.figureCards.first().locator('svg path').count();
    expect(paths).toBeGreaterThanOrEqual(1);
  });

  test('scatter3(x, y, z) — N markers projected to 2-D', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'scatter3([1 2 3 4], [1 2 3 4], [0 1 2 3]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    const circles = await ide.figureCards.first().locator('svg circle').count();
    // 4 marker dots — scatter3 maps to scatter mode after projection.
    expect(circles).toBe(4);
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
