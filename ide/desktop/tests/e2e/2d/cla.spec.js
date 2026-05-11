// cla.spec.js — clear-current-axes.

import { test, expect } from '../../helpers/shared.js';

test.describe('cla — clear current axes', () => {
  test('cla clears the data layers but keeps the axes', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([1 2 3], [1 4 9]);\n'
      + 'cla;\n'
      + 'plot([4 5 6], [16 25 36]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('cla("reset") also clears title + labels', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([1 2 3], [1 4 9]);\n'
      + 'title(\'old title\');\n'
      + 'cla(\'reset\');\n'
      + 'plot([1 2 3], [9 4 1]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    const labels = await ide.figureWindow.locator('svg text').allTextContents();
    expect(labels.join(' ')).not.toMatch(/old title/);
  });
});
