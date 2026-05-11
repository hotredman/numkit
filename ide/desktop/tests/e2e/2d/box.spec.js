// box.spec.js — box(on|off) frame toggle.

import { test, expect } from '../../helpers/shared.js';

test.describe('box on / off — axis-frame toggle', () => {
  test('box default — full frame (rect with no fill, stroke set)', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([1 2 3], [1 4 9]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    // The frame rect has fill="none" stroke=var(--plot-frame).
    const frame = ide.figureWindow.locator('svg rect[fill="none"]');
    expect(await frame.count()).toBeGreaterThanOrEqual(1);
  });

  test('box off — only bottom + left axis lines, no full frame', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([1 2 3], [1 4 9]);\n'
      + 'box off;\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    // No <rect fill="none"> for the panel-frame anymore.
    const frame = ide.figureWindow.locator('svg rect[fill="none"]');
    expect(await frame.count()).toBe(0);
  });

  test('box without args toggles', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([1 2 3], [1 4 9]);\n'
      + 'box off;\n'
      + 'box;\n'   // toggle back to ON
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });
});
