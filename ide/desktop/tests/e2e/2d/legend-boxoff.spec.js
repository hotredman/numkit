// legend-boxoff.spec.js — legend('boxoff') / legend('boxon').

import { test, expect } from '../../helpers/shared.js';

test.describe('legend boxoff / boxon', () => {
  test('legend default — frame rect present', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([1 2 3], [1 4 9]);\n'
      + 'legend(\'curve A\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    // Look for a legend bg <rect> (fill=var(--plot-bg) with rx=3).
    const html = await ide.figureWindow.locator('svg').last().innerHTML();
    expect(html).toMatch(/fill="var\(--plot-bg\)"[^>]*rx="3"/);
  });

  test('legend("boxoff") — frame hidden', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([1 2 3], [1 4 9]);\n'
      + 'legend(\'curve A\');\n'
      + 'legend boxoff;\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    const html = await ide.figureWindow.locator('svg').last().innerHTML();
    expect(html).not.toMatch(/fill="var\(--plot-bg\)"[^>]*rx="3"/);
  });

  test('legend("boxoff") then legend("boxon") restores frame', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([1 2 3], [1 4 9]);\n'
      + 'legend(\'curve A\');\n'
      + 'legend boxoff;\n'
      + 'legend boxon;\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    const html = await ide.figureWindow.locator('svg').last().innerHTML();
    expect(html).toMatch(/fill="var\(--plot-bg\)"[^>]*rx="3"/);
  });
});
