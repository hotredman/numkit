// t2a-patch.spec.js — Tier 2 polygon layer kind + patch / fill / fill3.
// Polygon mode breaks on `null` to support multi-polygon datasets and
// closes each sub-path automatically.

import { test, expect } from '../helpers/shared.js';

test.describe('Tier 2 — patch / fill / fill3', () => {
  test('patch(X, Y) — single triangle renders', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'patch([0 1 0.5], [0 0 1]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('patch(X, Y, char-color) — applies named color', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'patch([0 1 1 0], [0 0 1 1], \'r\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('patch(X, Y, [r g b]) — RGB triplet color', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'patch([0 1 0.5], [0 0 1], [0.2 0.7 0.3]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('patch with column matrix — multiple polygons', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'X = [0 2 4; 1 3 5; 0.5 2.5 4.5];\n'
      + 'Y = [0 0 0; 0 0 0; 1 1 1];\n'
      + 'patch(X, Y);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('fill(X, Y) — alias of patch', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'fill([0 1 1 0], [0 0 1 1], \'b\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('fill3(X, Y, Z) — 3D polygon via cabinet projection', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'fill3([0 1 0.5], [0 0 1], [0 0 1]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('patch produces an SVG <path> with fill', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'patch([0 1 0.5], [0 0 1], \'g\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    const paths = await ide.figureCards.first().locator('svg path').count();
    expect(paths).toBeGreaterThanOrEqual(1);
  });
});
