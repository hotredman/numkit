// b3-surf.spec.js — surf / mesh wireframe via cabinet projection.
//
// MATLAB:
//   surf(Z) / surf(X, Y, Z)
//   mesh(...) — same shape; in MATLAB the difference is face shading
//   on/off. numkit ships a wireframe-only render for both right now —
//   face shading + lighting deferred (would need a polygon layer).
//
// Tests assert:
//   1. Figure renders without console errors
//   2. The card has SVG paths (one per polyline; we emit 2 datasets:
//      horizontal sweep + vertical sweep)
//   3. mesh works the same way

import { test, expect } from '../helpers/shared.js';

test.describe('B3 — surf / mesh', () => {
  test('surf(Z) — renders wireframe, no errors', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'Z = [1 2 3 4; 2 3 4 5; 3 4 5 6; 4 5 6 7];\n'
      + 'surf(Z);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('surf(X, Y, Z) — explicit grid', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'x = [-1 -0.5 0 0.5 1];\n'
      + 'y = [-1 -0.5 0 0.5 1];\n'
      + 'Z = [1 2 3 2 1; 2 4 6 4 2; 3 6 9 6 3; 2 4 6 4 2; 1 2 3 2 1];\n'
      + 'surf(x, y, Z);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('mesh(Z) — same wireframe path, no errors', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'Z = [0 1 0; 1 4 1; 0 1 0];\n'
      + 'mesh(Z);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('surf — proof of render via WebGL canvas presence', async ({ ide, page }) => {
    // Originally asserted ≥ 2 SVG <path> elements (one per polyline
    // sweep), but surf is now routed through the three.js renderer
    // — geometry lives in a <canvas>, not SVG. The 3-D smoke spec
    // covers detailed canvas signals; here we just confirm a
    // canvas mounted, which proves the WebGL routing fired.
    await ide.runScript(
      'import compat.*;\n'
      + 'Z = [1 2 3; 2 4 6; 3 6 9];\n'
      + 'surf(Z);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await expect(ide.figureCards.first().locator('canvas[data-numkit-3d]'))
      .toBeVisible({ timeout: 10_000 });
  });

  test('surf opens cleanly in FigureWindow modal', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'Z = [0 1 2; 1 2 3; 2 3 4];\n'
      + 'surf(Z);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('surf with degenerate 1xN — gracefully no-op', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'Z = [1 2 3];\n'  // 1×3 — R<2, no surface possible
      + 'surf(Z);\n'
      + 'fprintf(\'survived\\n\');\n'
    );
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });
});
