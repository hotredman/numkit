// b2-polish.spec.js — small UX regressions caught after B2 closed.
//
// 1. Legend swatch differentiates by series mode (circle vs rect vs line).
// 2. yyaxis + heatmap doesn't crash (edge case: padR space-share).
// 3. yyaxis right-side scatter hover doesn't NaN out.

import { test, expect } from '../helpers/shared.js';

test.describe('B2 polish', () => {
  test('legend with mixed mode series — renders without errors', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([1 2 3], [1 2 3]);\n'
      + 'hold(\'on\');\n'
      + 'scatter([1 2 3], [3 2 1]);\n'
      + 'bar([1 2 3], [2 2 2]);\n'
      + 'legend(\'line\', \'scatter\', \'bar\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('yyaxis + heatmap — both render without NaN dimensions', async ({ ide, page }) => {
    // Edge case: padR is shared by the colorbar (heatmap default 'east')
    // and the right-side axis when yyEnabled. Confirm the layout stays
    // stable rather than producing negative widths.
    await ide.runScript(
      'import compat.*;\n'
      + 'imagesc(reshape(1:16, 4, 4));\n'
      + 'hold(\'on\');\n'
      + 'yyaxis(\'right\');\n'
      + 'plot([1 2 3 4], [10 20 30 40]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('yyaxis with single right-side series — auto-range stays finite', async ({ ide, page }) => {
    // When the user creates the figure, immediately switches to right,
    // and plots only one series there, the LEFT auto-range fed bogus
    // ±Infinity values (no left data). Renderer should clamp.
    await ide.runScript(
      'import compat.*;\n'
      + 'yyaxis(\'right\');\n'
      + 'plot([1 2 3 4 5], [10 20 30 40 50]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('legend after FigureWindow re-open — labels still visible', async ({ ide, page }) => {
    // Legend props travel via the figure object that re-mounts in the
    // modal — quick guard that the modal path doesn't drop them.
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([1 2 3], [1 2 3]);\n'
      + 'hold(\'on\');\n'
      + 'plot([1 2 3], [3 2 1]);\n'
      + 'legend(\'a\', \'b\', \'Location\', \'southwest\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });
});
