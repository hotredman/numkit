// subplot-empty-slot.spec.js — placeholder UX in skipped subplot slots.
//
// MATLAB renders subplot(R,C,k) for some-but-not-all k as 5 axes + 1
// empty figure-bg slot. We add a faded "not set" placeholder so the
// grid shape is visually intact — easier to read than an unframed gap.

import { test, expect } from '../../helpers/shared.js';

test.describe('Subplot grid empty-slot placeholder', () => {
  test('subplot(2,3,X) for X=1,2,3,5,6 → 5 plots + 1 placeholder', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + '[X, Y] = meshgrid(1:8, 1:8);\n'
      + 'mask = (X - 4).^2 + (Y - 4).^2 <= 3^2;\n'
      + 'figure;\n'
      + 'subplot(2,3,1); imshow(mask); title(\'1\');\n'
      + 'subplot(2,3,2); imshow(mask); title(\'2\');\n'
      + 'subplot(2,3,3); imshow(mask); title(\'3\');\n'
      + 'subplot(2,3,5); imshow(mask); title(\'5\');\n'
      + 'subplot(2,3,6); imshow(mask); title(\'6\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    await page.waitForTimeout(200);

    // 5 plot SVGs.
    expect(await page.locator('.fw-canvas-wrap svg').count()).toBe(5);
    // Exactly 1 placeholder for the skipped slot 4.
    const placeholders = page.locator('.fw-window .sg-empty-slot');
    expect(await placeholders.count()).toBe(1);
    await expect(placeholders.first()).toHaveText('not set');
  });

  test('subplot grid with no skipped slots → no placeholders', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'figure;\n'
      + 'subplot(1,2,1); plot([1 2 3], [1 4 9]);\n'
      + 'subplot(1,2,2); plot([1 2 3], [9 4 1]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    await page.waitForTimeout(200);

    expect(await page.locator('.fw-window .sg-empty-slot').count()).toBe(0);
  });
});
