// coneplot-streamtube.spec.js — 3-D vector-field visualisation:
// cone-headed arrows + streamlines wrapped in tubes.

import { test, expect } from '@playwright/test';
import { launchIde, closeIde } from '../helpers/launch.js';
import { IdePage } from '../helpers/ide.js';

test.describe('coneplot — cone-headed arrows', () => {
  let app, page, ide;

  test.beforeEach(async () => {
    app = await launchIde();
    page = await app.firstWindow();
    ide = new IdePage(page);
    await ide.waitForReady();
  });

  test.afterEach(async () => { await closeIde(app); });

  test('coneplot(U, V, W) — implicit grid, no errors', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'U = ones(3, 3, 3);\n'
      + 'V = zeros(3, 3, 3);\n'
      + 'W = zeros(3, 3, 3);\n'
      + 'coneplot(U, V, W);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('coneplot(X,Y,Z,U,V,W,Cx,Cy,Cz) — cones at user positions', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'x = linspace(-1, 1, 3);\n'
      + 'y = linspace(-1, 1, 3);\n'
      + 'z = linspace(-1, 1, 3);\n'
      + 'U = ones(3, 3, 3);\n'
      + 'V = zeros(3, 3, 3);\n'
      + 'W = zeros(3, 3, 3);\n'
      + 'coneplot(x, y, z, U, V, W, [0 0.5], [0 0.5], [0 0.5]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('coneplot mounts a 3-D WebGL canvas', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'U = ones(2, 2, 2);\n'
      + 'V = ones(2, 2, 2);\n'
      + 'W = zeros(2, 2, 2);\n'
      + 'coneplot(U, V, W);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    await expect(ide.figureWindow.locator('canvas[data-numkit-3d]'))
      .toBeVisible({ timeout: 5_000 });
  });
});

test.describe('streamtube — streamlines wrapped in tubes', () => {
  let app, page, ide;

  test.beforeEach(async () => {
    app = await launchIde();
    page = await app.firstWindow();
    ide = new IdePage(page);
    await ide.waitForReady();
  });

  test.afterEach(async () => { await closeIde(app); });

  test('streamtube(X,Y,Z,U,V,W, sx,sy,sz) — uniform-flow seed gives a tube', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'x = linspace(0, 5, 6);\n'
      + 'y = linspace(0, 5, 6);\n'
      + 'z = linspace(0, 5, 6);\n'
      + 'U = ones(6, 6, 6);\n'
      + 'V = zeros(6, 6, 6);\n'
      + 'W = zeros(6, 6, 6);\n'
      + 'streamtube(x, y, z, U, V, W, [0.5], [2.5], [2.5]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('streamtube(U, V, W, sx, sy, sz) — implicit grid', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'U = zeros(5, 5, 5);\n'
      + 'V = ones(5, 5, 5);\n'
      + 'W = zeros(5, 5, 5);\n'
      + 'streamtube(U, V, W, [3], [1], [3]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('streamtube — zero-magnitude seed exits gracefully', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'U = zeros(4, 4, 4);\n'
      + 'V = zeros(4, 4, 4);\n'
      + 'W = zeros(4, 4, 4);\n'
      + 'streamtube(U, V, W, [2], [2], [2]);\n'
      + 'fprintf(\'survived\\n\');\n'
    );
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('streamtube mounts a 3-D WebGL canvas', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'U = ones(5, 5, 5);\n'
      + 'V = zeros(5, 5, 5);\n'
      + 'W = zeros(5, 5, 5);\n'
      + 'streamtube(U, V, W, [1], [3], [3]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    await expect(ide.figureWindow.locator('canvas[data-numkit-3d]'))
      .toBeVisible({ timeout: 5_000 });
  });
});
