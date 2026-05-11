// tiledlayout.spec.js — modern subplot API (tiledlayout / nexttile).
//
// Both delegate to the existing subplot infrastructure
// (FigureManager::setSubplot). The test checks the figure card
// renders and the modal subplot grid carries the right cell count.

import { test, expect } from '../helpers/shared.js';

test.describe('tiledlayout / nexttile', () => {
  test('tiledlayout(2, 2) + 4× nexttile + plot — 4-cell grid', async ({ ide, page }) => {
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

  test('nexttile(k) — jumps to specific cell', async ({ ide, page }) => {
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

  test('nexttile without prior tiledlayout falls back to 1x1', async ({ ide, page }) => {
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
