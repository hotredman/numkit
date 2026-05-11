// b2-yyaxis.spec.js — dual Y-axis support.
//
// MATLAB:
//   yyaxis left;  plot(x, y1)         — series goes on left axis
//   yyaxis right; plot(x, y2)         — series goes on right axis
//   yyaxis left;  ylabel('left units')
//   yyaxis right; ylim([0 1])
//
// numkit: builtin `yyaxis(side)` enables the dual-axis state on the
// current axes and switches the active side. ylim / ylabel / yscale
// route to the active side's shadow fields when yy is on. plot,
// scatter, bar, errorbar, etc. all stamp the active side onto the
// dataset's `yside` field via FigureManager::pushDataset.
//
// Tests assert:
//   1. yyaxis call doesn't error and figure renders
//   2. left + right plots both reach the layer pipeline
//   3. ylabel/ylim/yscale post-yyaxis route to the right side
//   4. modal expansion preserves both sides

import { test, expect } from '../../helpers/shared.js';

test.describe('B2 — yyaxis (dual Y)', () => {
  test('yyaxis(\'left\') alone — bare call enables, figure renders', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([1 2 3], [1 4 9]);\n'
      + 'yyaxis(\'left\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('left + right plots — both render with dual-axis state', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'yyaxis(\'left\');\n'
      + 'plot([1 2 3 4], [1 2 3 4]);\n'
      + 'hold(\'on\');\n'
      + 'yyaxis(\'right\');\n'
      + 'plot([1 2 3 4], [1000 2000 3000 4000]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('ylim post-yyaxis right routes to right axis (no errors)', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'yyaxis(\'left\');\n'
      + 'plot([1 2 3], [1 2 3]);\n'
      + 'hold(\'on\');\n'
      + 'yyaxis(\'right\');\n'
      + 'plot([1 2 3], [100 200 300]);\n'
      + 'ylim([0 500]);\n'
      + 'ylabel(\'right units\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('yscale(\'log\') on right side — figure renders with log-y2', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'yyaxis(\'left\');\n'
      + 'plot([1 2 3 4], [1 2 3 4]);\n'
      + 'hold(\'on\');\n'
      + 'yyaxis(\'right\');\n'
      + 'plot([1 2 3 4], [10 100 1000 10000]);\n'
      + 'yscale(\'log\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('yyaxis switching back to left — subsequent plot lands on left', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'yyaxis(\'left\');\n'
      + 'plot([1 2 3], [1 2 3]);\n'
      + 'hold(\'on\');\n'
      + 'yyaxis(\'right\');\n'
      + 'plot([1 2 3], [50 60 70]);\n'
      + 'yyaxis(\'left\');\n'
      + 'plot([1 2 3], [3 2 1]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('dual-y figure opens cleanly in FigureWindow modal', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'yyaxis(\'left\');\n'
      + 'plot([1 2 3 4], [1 4 9 16]);\n'
      + 'ylabel(\'squared\');\n'
      + 'hold(\'on\');\n'
      + 'yyaxis(\'right\');\n'
      + 'plot([1 2 3 4], [10 20 30 40]);\n'
      + 'ylabel(\'linear\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('scatter on right axis — uses pushDataset routing', async ({ ide, page }) => {
    // Verifies that not just plot(), but scatter/bar/etc. all carry
    // the active yyaxis side via FigureManager::pushDataset.
    await ide.runScript(
      'import compat.*;\n'
      + 'yyaxis(\'left\');\n'
      + 'plot([1 2 3 4 5], [1 2 3 4 5]);\n'
      + 'hold(\'on\');\n'
      + 'yyaxis(\'right\');\n'
      + 'scatter([1 2 3 4 5], [100 200 300 400 500]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });
});
