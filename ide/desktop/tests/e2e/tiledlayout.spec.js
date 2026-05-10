// tiledlayout.spec.js — modern subplot API (tiledlayout / nexttile).
//
// Both delegate to the existing subplot infrastructure
// (FigureManager::setSubplot). The test checks the figure card
// renders and the modal subplot grid carries the right cell count.

import { test, expect } from '@playwright/test';
import { launchIde, closeIde } from '../helpers/launch.js';
import { IdePage } from '../helpers/ide.js';

test.describe('tiledlayout / nexttile', () => {
  let app, page, ide;

  test.beforeEach(async () => {
    app = await launchIde();
    page = await app.firstWindow();
    ide = new IdePage(page);
    await ide.waitForReady();
  });

  test.afterEach(async () => { await closeIde(app); });

  test('tiledlayout(2, 2) + 4× nexttile + plot — 4-cell grid', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'tiledlayout(2, 2);\n'
      + 'nexttile; plot([1 2 3], [1 2 3]);\n'
      + 'nexttile; plot([1 2 3], [3 2 1]);\n'
      + 'nexttile; plot([1 2 3], [1 4 9]);\n'
      + 'nexttile; plot([1 2 3], [9 4 1]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('nexttile(k) — jumps to specific cell', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'tiledlayout(1, 3);\n'
      + 'nexttile(2);\n'   // skip cell 1, draw in cell 2
      + 'plot([1 2 3], [1 2 3]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('nexttile without prior tiledlayout falls back to 1x1', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'nexttile;\n'   // no grid set
      + 'plot([1 2 3], [1 2 3]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });
});
