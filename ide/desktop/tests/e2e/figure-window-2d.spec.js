// figure-window-2d.spec.js — toolbar / footer rework smoke for 2-D
// figures. The X / Y NumberInputs moved from the toolbar to a footer
// row; this spec guards the new layout doesn't regress for plot,
// scatter, polar, and subplot figures.

import { test, expect } from '@playwright/test';
import { launchIde, closeIde } from '../helpers/launch.js';
import { IdePage } from '../helpers/ide.js';

test.describe('FigureWindow — 2-D layout', () => {
  let app, page, ide;

  test.beforeEach(async () => {
    app = await launchIde();
    page = await app.firstWindow();
    ide = new IdePage(page);
    await ide.waitForReady();
  });

  test.afterEach(async () => { await closeIde(app); });

  test('plot — X / Y inputs are in the FOOTER, not the toolbar', async () => {
    await ide.runScript('import compat.*;\nplot([1 2 3], [1 4 9]);\n');
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });

    // The new footer-row sits inside .fw-range-row, the old position
    // was inside .fw-toolbar. Assert the row exists in the footer
    // location and NumberInputs live inside it.
    const footerRow = ide.figureWindow.locator('.fw-range-row');
    await expect(footerRow).toBeVisible({ timeout: 5_000 });
    const inputs = footerRow.locator('input');
    expect(await inputs.count()).toBe(4);   // x-lo, x-hi, y-lo, y-hi

    // Toolbar must NOT contain a range-group anymore.
    const toolbar = ide.figureWindow.locator('.fw-toolbar');
    expect(await toolbar.locator('.fw-range-group').count()).toBe(0);
  });

  test('plot — committing a new x-hi triggers re-render (no error)', async () => {
    await ide.runScript('import compat.*;\nplot([0 1 2 3 4], [0 1 4 9 16]);\n');
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });

    const inputs = ide.figureWindow.locator('.fw-range-row input');
    // Replace x-hi (input #2) with 10.
    await inputs.nth(1).click();
    await inputs.nth(1).fill('10');
    await inputs.nth(1).press('Enter');
    await page.waitForTimeout(150);

    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('polar — single r input pair in footer', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'theta = linspace(0, 2*pi, 60);\n'
      + 'polarplot(theta, sin(2*theta));\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });

    const footerRow = ide.figureWindow.locator('.fw-range-row');
    await expect(footerRow).toBeVisible({ timeout: 5_000 });
    expect(await footerRow.locator('input').count()).toBe(2);   // r-lo, r-hi
  });

  test('subplot — no range-row at all (per-cell pan/zoom)', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'subplot(1, 2, 1); plot([1 2 3], [1 4 9]);\n'
      + 'subplot(1, 2, 2); plot([1 2 3], [9 4 1]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });

    expect(await ide.figureWindow.locator('.fw-range-row').count()).toBe(0);
  });

  test('Fit menu still has X-only / Y-only options for 2-D plots', async () => {
    await ide.runScript('import compat.*;\nplot([1 2 3], [1 4 9]);\n');
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });

    await ide.figureWindow.locator('button', { hasText: 'fit' }).first().click();
    const popup = ide.figureWindow.locator('.fw-pop');
    await expect(popup).toBeVisible({ timeout: 5_000 });
    await expect(popup.locator('button', { hasText: 'X only' })).toBeVisible();
    await expect(popup.locator('button', { hasText: 'Y only' })).toBeVisible();
    // 2-D popup must NOT have a Z option.
    expect(await popup.locator('button', { hasText: 'Z only' }).count()).toBe(0);
  });
});
