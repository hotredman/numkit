// webgl-3d-wrappers.spec.js — Etap 5: quiver3 / contour3 / surfc /
// meshc 3-D wrapper builtins routed through the WebGL renderer.

import { test, expect } from '../helpers/shared.js';

test.describe('WebGL 3-D — quiver3 / contour3 / surfc / meshc', () => {
  test('quiver3(x, y, z, u, v, w) — arrows in 3-D', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'x = [0 1 2]; y = [0 1 2]; z = [0 1 2];\n'
      + 'u = [0.5 0.5 0.5]; v = [0 0 0]; w = [0.5 0.5 0.5];\n'
      + 'quiver3(x, y, z, u, v, w);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await expect(ide.figureCards.first().locator('canvas[data-numkit-3d]'))
      .toBeVisible({ timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('quiver3 with explicit scale', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'x = [0 1]; y = [0 1]; z = [0 0];\n'
      + 'u = [1 -1]; v = [0 0]; w = [1 1];\n'
      + 'quiver3(x, y, z, u, v, w, 0.5);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('contour3(Z) — contour lines on surface height', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'Z = [0 1 2 1 0; 1 2 3 2 1; 2 3 4 3 2; 1 2 3 2 1; 0 1 2 1 0];\n'
      + 'contour3(Z);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await expect(ide.figureCards.first().locator('canvas[data-numkit-3d]'))
      .toBeVisible({ timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('contour3(Z, n) — n levels', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'Z = [1 2 3; 2 4 6; 3 6 9];\n'
      + 'contour3(Z, 5);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('contour3(Z, levels) — explicit level vector', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'Z = [1 2 3; 2 4 6; 3 6 9];\n'
      + 'contour3(Z, [2 4 6 8]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('surfc(Z) — surface + contour combined', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'Z = [1 2 3 4; 2 3 4 5; 3 4 5 6];\n'
      + 'surfc(Z);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await expect(ide.figureCards.first().locator('canvas[data-numkit-3d]'))
      .toBeVisible({ timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('meshc(Z) — mesh + contour combined', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'Z = [1 2 3; 2 3 4; 3 4 5];\n'
      + 'meshc(Z);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('quiver3 modal — opens cleanly', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'quiver3([0 1], [0 1], [0 0], [1 -1], [0 0], [1 1]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    await expect(ide.figureWindow.locator('canvas[data-numkit-3d]'))
      .toBeVisible({ timeout: 10_000 });
  });
});
