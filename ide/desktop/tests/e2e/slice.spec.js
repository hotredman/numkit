// slice.spec.js — axis-aligned cross sections of a 3-D scalar volume.
//
// Coverage:
//   • slice(V, sx, sy, sz) builds a 3-D figure and renders without
//     errors (canvas mounts).
//   • Empty slice arrays — degenerate path picks mid-planes.
//   • Explicit grid form: slice(X, Y, Z, V, sx, sy, sz).
//
// Per-cell colormap on the slice plane is BACKLOG (single
// representative colour per slice for now); we don't pixel-test.

import { test, expect } from '@playwright/test';
import { launchIde, closeIde } from '../helpers/launch.js';
import { IdePage } from '../helpers/ide.js';

test.describe('slice — 3-D volume cross sections', () => {
  let app, page, ide;

  test.beforeEach(async () => {
    app = await launchIde();
    page = await app.firstWindow();
    ide = new IdePage(page);
    await ide.waitForReady();
  });

  test.afterEach(async () => { await closeIde(app); });

  test('slice(V, sx, sy, sz) renders without errors', async () => {
    await ide.runScript(
      'import compat.*;\n'
      // Build a 4×4×4 volume programmatically.
      + 'V = zeros(4, 4, 4);\n'
      + 'V(:,:,1) = eye(4);\n'
      + 'V(:,:,2) = 2*eye(4);\n'
      + 'V(:,:,3) = 3*eye(4);\n'
      + 'V(:,:,4) = 4*eye(4);\n'
      + 'slice(V, [2], [2], [2]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('slice mounts the WebGL canvas', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'V = zeros(3, 3, 3);\n'
      + 'V(:,:,1) = 1; V(:,:,2) = 2; V(:,:,3) = 3;\n'
      + 'slice(V, [1.5], [1.5], [1.5]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    await expect(ide.figureWindow.locator('canvas[data-numkit-3d]'))
      .toBeVisible({ timeout: 5_000 });
  });

  test('slice with empty axis vectors picks mid-planes (no error)', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'V = zeros(3, 3, 3);\n'
      + 'V(:,:,1) = 1; V(:,:,2) = 5; V(:,:,3) = 1;\n'
      + 'slice(V, [], [], []);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('slice(X, Y, Z, V, sx, sy, sz) — explicit grid form', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'x = linspace(-1, 1, 4);\n'
      + 'y = linspace(-1, 1, 4);\n'
      + 'z = linspace(-1, 1, 4);\n'
      + 'V = zeros(4, 4, 4);\n'
      + 'for k = 1:4; V(:,:,k) = k; end\n'
      + 'slice(x, y, z, V, [0], [0], [0]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('slice with multiple sx values — multiple X-planes drawn', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'V = zeros(4, 4, 4);\n'
      + 'for k = 1:4; V(:,:,k) = k; end\n'
      + 'slice(V, [1 3], [], []);\n'   // 2 X-planes
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });
});
