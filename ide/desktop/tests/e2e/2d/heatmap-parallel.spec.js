// heatmap-parallel.spec.js — heatmap(C) (table-style) + parallelplot.
// Both delegate to existing renderers — heatmap → imagesc, parallelplot
// → multi-line plot. Real cell-text overlays (heatmap) and dedicated
// multi-axis parallel rendering are BACKLOG.

import { test, expect } from '../../helpers/shared.js';

test.describe('heatmap (table) + parallelplot', () => {
  test('heatmap(C) — image element rendered', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'C = [1 2 3; 2 4 6; 3 6 9];\n'
      + 'heatmap(C);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    const img = ide.figureCards.first().locator('svg image[href^="data:image/png"]');
    expect(await img.count()).toBeGreaterThanOrEqual(1);
  });

  test('heatmap(C, "Colormap", "hot") — colormap honoured', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'heatmap([1 2; 3 4], \'Colormap\', \'hot\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    const opts = await ide.figureWindow.locator('select').elementHandles();
    let found = false;
    for (const sel of opts) {
      if ((await sel.evaluate((el) => el.value)) === 'hot') { found = true; break; }
    }
    expect(found).toBe(true);
  });

  test('parallelplot — emits one line per row of the matrix', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      // 4 rows × 5 cols → 4 lines spanning x = 1..5.
      + 'T = [1 2 3 4 5; 5 4 3 2 1; 2 3 4 3 2; 1 1 5 1 1];\n'
      + 'parallelplot(T);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    // Each row → one path with stroke. Expect at least 4 paths.
    const paths = await ide.figureCards.first().locator('svg path').count();
    expect(paths).toBeGreaterThanOrEqual(4);
  });
});
