// t2b-shapes.spec.js — Tier-2 filled-shape builtins:
// pie / pie3, boxplot / boxchart, violinplot, bar3, waterfall.
//
// All five build on the polygon layer kind from t2a. We rely on the
// smoke spec to guarantee WASM mode (a duplicate-compat at any of
// these new builtin names would brick repl_init).

import { test, expect } from '../helpers/shared.js';

test.describe('Tier 2 — pie / boxplot / violinplot / bar3 / waterfall', () => {
  test('pie(X) — wedges per slice', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'pie([3 1 4 1 5 9]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    const paths = await ide.figureCards.first().locator('svg path').count();
    expect(paths).toBeGreaterThanOrEqual(6);
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('pie(X, explode) — explodes selected wedges', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'pie([2 3 4 5], [0 1 0 0]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('pie3(X) — flattened pie via Y-tilt', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'pie3([10 20 30 40]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('boxplot(x) — single box renders', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'boxplot([1 2 3 3 4 4 5 5 6 7 12]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('boxplot on matrix — one box per column', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'X = randn(20, 3);\n'
      + 'boxplot(X);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('boxchart — alias of boxplot', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'boxchart([10 12 14 14 15 16 18 19 22]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('violinplot(x) — KDE shape + box + median', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'violinplot(randn(1, 60));\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('bar3(Z) — 3D bars rendered in WebGL canvas', async ({ ide, page }) => {
    // History: this used to assert ≥ 30 SVG <path> elements when bar3
    // pre-projected through cabinet on the C++ side. After Etap 3 of
    // the WebGL roll-out bar3 emits raw 3-D coords and the renderer
    // builds an indexed cuboid mesh in WebGL. Geometry now lives in
    // a <canvas> — assert that instead. (webgl-3d.spec covers
    // detailed canvas signals.)
    await ide.runScript(
      'import compat.*;\n'
      + 'Z = [1 2 3; 4 5 6; 7 8 9];\n'
      + 'bar3(Z);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await expect(ide.figureCards.first().locator('canvas[data-numkit-3d]'))
      .toBeVisible({ timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('waterfall(Z) — per-row ribbon stack', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'Z = [1 2 3 4; 2 3 4 5; 4 3 2 1];\n'
      + 'waterfall(Z);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('all five open cleanly in modal', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'pie([1 2 3]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });
});
