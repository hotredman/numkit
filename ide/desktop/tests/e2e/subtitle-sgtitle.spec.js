// subtitle-sgtitle.spec.js — title / subtitle / sgtitle text rendering.

import { test, expect } from '@playwright/test';
import { launchIde, closeIde } from '../helpers/launch.js';
import { IdePage } from '../helpers/ide.js';

test.describe('subtitle / sgtitle', () => {
  let app, page, ide;

  test.beforeEach(async () => {
    app = await launchIde();
    page = await app.firstWindow();
    ide = new IdePage(page);
    await ide.waitForReady();
  });

  test.afterEach(async () => { await closeIde(app); });

  test('title + subtitle both render', async () => {
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

  test('sgtitle — figure-level super title', async () => {
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
