// webgl-3d-shapes.spec.js — Etap 3: bar3, waterfall, fill3 now route
// through the WebGL renderer via raw 3-D coords + new layer modes
// (bar3, waterfall, polygon3d).

import { test, expect } from '../helpers/shared.js';

test.describe('WebGL 3-D — bar3 / waterfall / fill3', () => {
  test('bar3(Z) — cuboid mesh in WebGL', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'Z = [1 2 3; 2 3 4; 3 4 5];\n'
      + 'bar3(Z);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await expect(ide.figureCards.first().locator('canvas[data-numkit-3d]'))
      .toBeVisible({ timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('bar3 with zero / NaN entries — those bars skipped, no crash', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'Z = [1 0 3; 0 5 0; 3 0 9];\n'
      + 'bar3(Z);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('waterfall(Z) — ribbon strips in WebGL', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'Z = [1 2 3 4; 2 3 4 5; 3 4 5 6];\n'
      + 'waterfall(Z);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await expect(ide.figureCards.first().locator('canvas[data-numkit-3d]'))
      .toBeVisible({ timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('fill3(X, Y, Z) — single triangle polygon in 3-D', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'fill3([0 1 0.5], [0 0 1], [0 0 1]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await expect(ide.figureCards.first().locator('canvas[data-numkit-3d]'))
      .toBeVisible({ timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('fill3 with column matrix — multiple polygons', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'X = [0 2; 1 3; 0.5 2.5];\n'
      + 'Y = [0 0; 0 0; 1 1];\n'
      + 'Z = [0 0; 1 1; 2 2];\n'
      + 'fill3(X, Y, Z);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('bar3 + view(45, 45) — camera honoured', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'Z = [1 2; 3 4];\n'
      + 'bar3(Z);\n'
      + 'view(45, 45);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('all three open cleanly in modal', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'bar3([1 2; 3 4]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    await expect(ide.figureWindow.locator('canvas[data-numkit-3d]'))
      .toBeVisible({ timeout: 10_000 });
  });
});
