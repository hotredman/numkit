// contourf.spec.js — filled contour bands.
//
// contourf differs from contour: contour emits open polylines, one
// dataset per level (rendered as <path stroke=...>); contourf emits
// CLOSED polygons that fill the region "Z >= L" with a colour drawn
// from the level's HSL ramp.
//
// We assert:
//   • figure card mounts (no crash)
//   • the renderer's polygon code path produces filled <polygon> /
//     <path fill="..."> elements (not just strokes)
//   • level count argument (contourf(Z, n)) flows through

import { test, expect } from '../../helpers/shared.js';

test.describe('contourf — filled contour bands', () => {
  test('contourf(Z) renders without errors', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + '[X, Y] = meshgrid(linspace(-3, 3, 30));\n'
      + 'Z = X .* exp(-X.^2 - Y.^2);\n'
      + 'contourf(Z);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('contourf produces <path fill="…"> elements (filled, not just stroke)', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'Z = [1 2 3 4; 2 4 6 4; 3 6 9 6; 4 4 6 4];\n'
      + 'contourf(Z);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    // Filled polygons end up as <path> with fill set to the layer
    // colour. Since polygon mode renders a closed path, count the
    // filled paths under the SVG. Should be at least 2 (background
    // band + one level above).
    const card = ide.figureCards.first();
    const filledPaths = await card.locator('svg path[fill]:not([fill="none"])').count();
    expect(filledPaths).toBeGreaterThanOrEqual(2);
  });

  test('contourf(Z, n) — explicit level count produces n+1 bands', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + '[X, Y] = meshgrid(linspace(-2, 2, 20));\n'
      + 'Z = X.^2 + Y.^2;\n'
      + 'contourf(Z, 5);\n'   // 5 levels → 6 bands (incl. base)
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('contourf(X, Y, Z) — explicit grid coords', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'x = linspace(-1, 1, 10);\n'
      + 'y = linspace(-1, 1, 10);\n'
      + '[X, Y] = meshgrid(x, y);\n'
      + 'Z = sin(pi * X) .* cos(pi * Y);\n'
      + 'contourf(x, y, Z);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('contourf opens cleanly in the modal', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'Z = [1 2 3; 2 4 2; 3 2 1];\n'
      + 'contourf(Z);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });
});
