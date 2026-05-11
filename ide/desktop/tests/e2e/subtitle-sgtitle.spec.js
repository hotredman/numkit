// subtitle-sgtitle.spec.js — title / subtitle / sgtitle text rendering.

import { test, expect } from '../helpers/shared.js';

test.describe('subtitle / sgtitle', () => {
  test('title + subtitle both render', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([1 2 3], [1 4 9]);\n'
      + 'title(\'Main Title\');\n'
      + 'subtitle(\'Subtitle here\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    const labels = await ide.figureWindow.locator('svg text').allTextContents();
    expect(labels.join(' ')).toMatch(/Main Title/);
    expect(labels.join(' ')).toMatch(/Subtitle here/);
  });

  test('sgtitle — figure-level super title', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'subplot(1, 2, 1); plot([1 2 3], [1 2 3]);\n'
      + 'subplot(1, 2, 2); plot([1 2 3], [3 2 1]);\n'
      + 'sgtitle(\'Super Title\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });
});
