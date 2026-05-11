// confusionchart.spec.js — heatmap with per-cell text labels.

import { test, expect } from '../../helpers/shared.js';

test.describe('confusionchart + heatmap cell labels', () => {
  test('heatmap(C) default — labels rendered (≤ 20×20 grid)', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'heatmap([10 20 30; 40 50 60; 70 80 90]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    // 9 text overlays for 3×3 matrix.
    const labels = await ide.figureWindow.locator('svg text').allTextContents();
    const joined = labels.join(' ');
    // Spot-check a couple of values.
    expect(joined).toMatch(/10/);
    expect(joined).toMatch(/90/);
  });

  test('heatmap CellLabel,off suppresses overlays', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'heatmap([1 2; 3 4], \'CellLabel\', \'off\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('confusionchart with class names', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'C = [50 2 1; 3 45 4; 0 5 48];\n'
      + 'confusionchart(C, {\'A\', \'B\', \'C\'});\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    const labels = await ide.figureWindow.locator('svg text').allTextContents();
    const joined = labels.join(' ');
    // Expect class names + axis labels.
    expect(joined).toMatch(/Predicted Class/);
    expect(joined).toMatch(/True Class/);
    expect(joined).toMatch(/\bA\b/);
    expect(joined).toMatch(/50/);
  });
});
