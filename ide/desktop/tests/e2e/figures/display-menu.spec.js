// display-menu.spec.js — exercise the "display ▾" toolbar popover that
// consolidates grid / minor / xlog / ylog / zlog / title / xlabel /
// ylabel / zlabel toggles into one menu (replaces the old inline
// grid+minor+log buttons).

import { test, expect } from '../../helpers/shared.js';

async function openDisplayMenu(page) {
  // The display ▾ button sits inside the modal toolbar. After clicking
  // the .fw-pop popover appears as the next sibling.
  await page.locator('.fw-toolbar .ve-btn', { hasText: /display/i }).click();
  await expect(page.locator('.fw-pop').first()).toBeVisible({ timeout: 2_000 });
}

test.describe('display ▾ menu — toggle visibility', () => {
  test('button is always present (incl. for subplot)', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'figure;\n'
      + 'subplot(1,2,1); plot(1:10);\n'
      + 'subplot(1,2,2); plot(1:5);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    const displayBtn = page.locator('.fw-toolbar .ve-btn', { hasText: /display/i });
    await expect(displayBtn).toBeVisible({ timeout: 2_000 });
  });

  test('toggle title hides the figure title text in the SVG', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot(1:10);\n'
      + 'title("Hello visibility");\n'
      + 'xlabel("X axis"); ylabel("Y axis");\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });

    // Title text is in the SVG before toggling.
    const titleLoc = page.locator('.fw-window svg text', { hasText: 'Hello visibility' });
    await expect(titleLoc).toBeVisible({ timeout: 2_000 });

    await openDisplayMenu(page);
    await page.locator('.fw-pop-toggle', { has: page.locator('span', { hasText: 'title' }) }).click();

    // Title gone after toggle.
    await expect(titleLoc).toHaveCount(0, { timeout: 2_000 });

    // Re-toggle restores it.
    await page.locator('.fw-pop-toggle', { has: page.locator('span', { hasText: 'title' }) }).click();
    await expect(titleLoc).toBeVisible({ timeout: 2_000 });
  });

  test('xlabel toggle hides only x-axis label, not y', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot(1:10);\n'
      + 'xlabel("the x"); ylabel("the y");\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });

    await openDisplayMenu(page);
    await page.locator('.fw-pop-toggle', { has: page.locator('span', { hasText: 'xlabel' }) }).click();

    await expect(page.locator('.fw-window svg text', { hasText: 'the x' })).toHaveCount(0);
    await expect(page.locator('.fw-window svg text', { hasText: 'the y' })).toBeVisible();
  });

  test('grid toggle removes major-grid lines', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot(1:10);\n'
      + 'grid on;\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    await page.waitForTimeout(150);

    const beforeCount = await page.locator('.fw-window svg line[stroke*="--plot-grid"]').count();
    expect(beforeCount).toBeGreaterThan(0);

    await openDisplayMenu(page);
    await page.locator('.fw-pop-toggle', { has: page.locator('span', { hasText: /^grid$/ }) }).click();
    await page.waitForTimeout(100);

    const afterCount = await page.locator('.fw-window svg line[stroke*="--plot-grid"]').count();
    expect(afterCount).toBe(0);
  });

  test('zlog and zlabel are disabled for non-3-D figure', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot(1:10);\n'
      + 'title("2D"); xlabel("x"); ylabel("y");\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });

    await openDisplayMenu(page);
    const zlog = page.locator('.fw-pop-toggle', { has: page.locator('span', { hasText: 'zlog' }) });
    const zlabel = page.locator('.fw-pop-toggle', { has: page.locator('span', { hasText: 'zlabel' }) });
    await expect(zlog).toBeDisabled();
    await expect(zlabel).toBeDisabled();
  });

  test('title button disabled when figure has no title set', async ({ ide, page }) => {
    // Probe to surface the actual figure.title so the disabled rule is
    // checked against fresh state. `figure;` forces a new axes; the
    // explicit title("") + xlabel("") are belt-and-braces in case the
    // figure manager carries strings across `close all`.
    await ide.runScript(
      'import compat.*;\n'
      + 'figure;\n'
      + 'plot(1:10);\n'
      + 'title(""); xlabel(""); ylabel("");\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });

    await openDisplayMenu(page);
    const titleBtn = page.locator('.fw-pop-toggle', { has: page.locator('span', { hasText: 'title' }) });
    const xlabelBtn = page.locator('.fw-pop-toggle', { has: page.locator('span', { hasText: 'xlabel' }) });
    await expect(titleBtn).toBeDisabled();
    await expect(xlabelBtn).toBeDisabled();
  });

  test('subplot — title toggle applies to ALL cells', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'figure;\n'
      + 'subplot(1,2,1); plot(1:10); title("Cell A");\n'
      + 'subplot(1,2,2); plot(1:5);  title("Cell B");\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });

    // Both titles visible.
    await expect(page.locator('.fw-window svg text', { hasText: 'Cell A' })).toBeVisible();
    await expect(page.locator('.fw-window svg text', { hasText: 'Cell B' })).toBeVisible();

    await openDisplayMenu(page);
    await page.locator('.fw-pop-toggle', { has: page.locator('span', { hasText: 'title' }) }).click();
    await page.waitForTimeout(100);

    // Both gone.
    await expect(page.locator('.fw-window svg text', { hasText: 'Cell A' })).toHaveCount(0);
    await expect(page.locator('.fw-window svg text', { hasText: 'Cell B' })).toHaveCount(0);
  });
});
