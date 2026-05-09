// t2a-patch.spec.js — Tier 2 polygon layer kind + patch / fill / fill3.
// Polygon mode breaks on `null` to support multi-polygon datasets and
// closes each sub-path automatically.

import { test, expect } from '@playwright/test';
import { launchIde, closeIde } from '../helpers/launch.js';
import { IdePage } from '../helpers/ide.js';

test.describe('Tier 2 — patch / fill / fill3', () => {
  let app, page, ide;

  test.beforeEach(async () => {
    app = await launchIde();
    page = await app.firstWindow();
    ide = new IdePage(page);
    await ide.waitForReady();
  });

  test.afterEach(async () => { await closeIde(app); });

  test('patch(X, Y) — single triangle renders', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'patch([0 1 0.5], [0 0 1]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('patch(X, Y, char-color) — applies named color', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'patch([0 1 1 0], [0 0 1 1], \'r\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('patch(X, Y, [r g b]) — RGB triplet color', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'patch([0 1 0.5], [0 0 1], [0.2 0.7 0.3]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('patch with column matrix — multiple polygons', async () => {
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

  test('fill(X, Y) — alias of patch', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'fill([0 1 1 0], [0 0 1 1], \'b\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('fill3(X, Y, Z) — 3D polygon via cabinet projection', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'fill3([0 1 0.5], [0 0 1], [0 0 1]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('patch produces an SVG <path> with fill', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'patch([0 1 0.5], [0 0 1], \'g\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    const paths = await ide.figureCards.first().locator('svg path').count();
    expect(paths).toBeGreaterThanOrEqual(1);
  });
});
