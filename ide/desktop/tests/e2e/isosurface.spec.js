// isosurface.spec.js — marching-cubes iso-surface extraction.
//
// Coverage:
//   • isosurface(V, iso) builds a 3-D figure with a fill3-style
//     mesh.
//   • isosurface(X, Y, Z, V, iso) — explicit grid form.
//   • Empty / degenerate volume (all below or all above iso) doesn't
//     crash.
//
// We don't pixel-test the surface — just confirm the canvas mounts
// and the call survives without console errors.

import { test, expect } from '../helpers/shared.js';

test.describe('isosurface — marching cubes', () => {
  test('isosurface(V, iso) — gradient volume → mesh', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'V = zeros(4, 4, 4);\n'
      + 'V(:,:,1) = 1;\n'
      + 'V(:,:,2) = 2;\n'
      + 'V(:,:,3) = 3;\n'
      + 'V(:,:,4) = 4;\n'
      + 'isosurface(V, 2.5);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('isosurface mounts the WebGL canvas', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'V = zeros(4, 4, 4);\n'
      + 'V(:,:,1) = 1;\n'
      + 'V(:,:,2) = 5;\n'
      + 'V(:,:,3) = 5;\n'
      + 'V(:,:,4) = 1;\n'
      + 'isosurface(V, 3);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    await expect(ide.figureWindow.locator('canvas[data-numkit-3d]'))
      .toBeVisible({ timeout: 5_000 });
  });

  test('isosurface with constant volume (no surface) — no crash', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'V = ones(4, 4, 4);\n'
      + 'isosurface(V, 5);\n'   // iso above max → empty surface
      + 'fprintf(\'survived\\n\');\n'
    );
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('isosurface(X, Y, Z, V, iso) — explicit grid coords', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'x = linspace(-2, 2, 4);\n'
      + 'y = linspace(-2, 2, 4);\n'
      + 'z = linspace(-2, 2, 4);\n'
      + 'V = zeros(4, 4, 4);\n'
      + 'V(:,:,1) = 0;\n'
      + 'V(:,:,2) = 1;\n'
      + 'V(:,:,3) = 2;\n'
      + 'V(:,:,4) = 3;\n'
      + 'isosurface(x, y, z, V, 1.5);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });
});
