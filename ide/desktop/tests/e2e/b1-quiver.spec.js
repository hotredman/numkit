// b1-quiver.spec.js — vector field arrows.
//
// quiver(x, y, u, v) → N arrows. Each arrow renders as 3 SVG <line>s
// (shaft + 2 head fins). Zero-length vectors are skipped — so a
// well-defined input of 5 non-zero arrows should produce ≥ 15 lines.

import { test, expect } from '@playwright/test';
import { launchIde, closeIde } from '../helpers/launch.js';
import { IdePage } from '../helpers/ide.js';

test.describe('B1 — quiver', () => {
  let app, page, ide;

  test.beforeEach(async () => {
    app = await launchIde();
    page = await app.firstWindow();
    ide = new IdePage(page);
    await ide.waitForReady();
  });

  test.afterEach(async () => { await closeIde(app); });

  test('quiver(x, y, u, v) — 5 arrows → ≥ 15 lines', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'quiver([1 2 3 4 5], [1 1 1 1 1], [0.5 0.5 0.5 0.5 0.5], [0.3 -0.2 0.4 -0.1 0.2]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    const lines = await ide.figureCards.first().locator('svg line').count();
    expect(lines, `quiver drew ${lines} <line>s; expected ≥ 15 (5 arrows × 3 segs)`)
      .toBeGreaterThanOrEqual(15);
  });

  test('quiver with custom scale renders without errors', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'quiver([1 2 3], [2 2 2], [1 1 1], [0 1 -1], 0.5);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('quiver overlay on imagesc (composite)', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'imagesc([1 2 3], [1 2 3], [1 2 3; 4 5 6; 7 8 9]);\n'
      + 'hold on;\n'
      + 'quiver([1 2 3], [1 2 3], [0.3 0.3 0.3], [0.3 0.3 0.3]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    const card = ide.figureCards.first();
    // Image (heatmap) + arrows both rendered.
    expect(await card.locator('svg image').count()).toBeGreaterThanOrEqual(1);
    expect(await card.locator('svg line').count()).toBeGreaterThanOrEqual(9);
  });
});
