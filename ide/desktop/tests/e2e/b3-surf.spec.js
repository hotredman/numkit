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

import { test, expect } from '@playwright/test';
import { launchIde, closeIde } from '../helpers/launch.js';
import { IdePage } from '../helpers/ide.js';

test.describe('B3 — surf / mesh', () => {
  let app, page, ide;

  test.beforeEach(async () => {
    app = await launchIde();
    page = await app.firstWindow();
    ide = new IdePage(page);
    await ide.waitForReady();
  });

  test.afterEach(async () => { await closeIde(app); });

  test('surf(Z) — renders wireframe, no errors', async () => {
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

  test('surf(X, Y, Z) — explicit grid', async () => {
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

  test('mesh(Z) — same wireframe path, no errors', async () => {
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

  test('surf renders multiple SVG paths (horizontal + vertical sweep)', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'Z = [1 2 3; 2 4 6; 3 6 9];\n'
      + 'surf(Z);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    // surf emits 2 datasets (horizontal + vertical). Each renders as
    // one SVG <path>; expect ≥ 2.
    const paths = await ide.figureCards.first().locator('svg path').count();
    expect(paths).toBeGreaterThanOrEqual(2);
  });

  test('surf opens cleanly in FigureWindow modal', async () => {
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

  test('surf with degenerate 1xN — gracefully no-op', async () => {
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
