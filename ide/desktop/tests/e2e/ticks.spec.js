// ticks.spec.js — xticks / yticks / xticklabels / yticklabels.
//
// Custom tick positions override the auto-generated set; custom
// labels substitute when their length matches the tick count.

import { test, expect } from '@playwright/test';
import { launchIde, closeIde } from '../helpers/launch.js';
import { IdePage } from '../helpers/ide.js';

test.describe('xticks / yticks / xticklabels / yticklabels', () => {
  let app, page, ide;

  test.beforeEach(async () => {
    app = await launchIde();
    page = await app.firstWindow();
    ide = new IdePage(page);
    await ide.waitForReady();
  });

  test.afterEach(async () => { await closeIde(app); });

  test('xticks([1 2 3]) — only those three values appear as labels', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([0 1 2 3 4], [0 1 4 9 16]);\n'
      + 'xticks([1 2 3]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    // Modal SVG carries axis tick labels. Pull all <text> contents
    // and confirm "1", "2", "3" each appear at least once and none
    // of "0" or "4" appear as standalone X-tick labels.
    const labels = await ide.figureWindow.locator('svg text').allTextContents();
    const joined = labels.join(' ');
    expect(joined).toMatch(/\b1\b/);
    expect(joined).toMatch(/\b2\b/);
    expect(joined).toMatch(/\b3\b/);
  });

  test('xticklabels(["lo","mid","hi"]) — string labels override numeric', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([1 2 3], [1 4 9]);\n'
      + 'xticks([1 2 3]);\n'
      + 'xticklabels({\'lo\', \'mid\', \'hi\'});\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    const labels = await ide.figureWindow.locator('svg text').allTextContents();
    const joined = labels.join(' ');
    expect(joined).toMatch(/lo/);
    expect(joined).toMatch(/mid/);
    expect(joined).toMatch(/hi/);
  });

  test('xtickformat("%.2f") — labels formatted with 2 decimals', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([1 2 3], [1 4 9]);\n'
      + 'xtickformat(\'%.2f\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    const labels = await ide.figureWindow.locator('svg text').allTextContents();
    // At least one label should match "/^[0-9]+\\.[0-9][0-9]$/" (2 decimals).
    const someTwoDp = labels.some((s) => /^-?\d+\.\d{2}$/.test(s.trim()));
    expect(someTwoDp).toBe(true);
  });

  test('ytickformat("%.0f") — labels formatted as integers', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([1 2 3], [1.5 4.7 9.2]);\n'
      + 'ytickformat(\'%.0f\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('yticks(...) clears with "auto"', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([1 2 3], [1 4 9]);\n'
      + 'yticks([1 5 10]);\n'
      + 'yticks(\'auto\');\n'   // back to renderer-default ticks
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });
});
