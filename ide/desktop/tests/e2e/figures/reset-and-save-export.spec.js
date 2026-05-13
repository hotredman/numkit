// reset-and-save-export.spec.js — covers the new toolbar Reset
// button + Save/Export submenu in ПКМ + display ▾ legend toggle &
// reset.

import { test, expect } from '../../helpers/shared.js';

async function rightClickPlot(page) {
  const canvas = page.locator('.fw-window .fw-canvas-wrap svg').first();
  const box = await canvas.boundingBox();
  await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
  await page.mouse.down({ button: 'right' });
  await page.mouse.up({ button: 'right' });
  await expect(page.locator('.ctx-menu')).toBeVisible({ timeout: 2_000 });
}

async function openDisplayMenu(page) {
  await page.locator('.fw-toolbar .ve-btn', { hasText: /display/i }).click();
  await expect(page.locator('.fw-pop').first()).toBeVisible({ timeout: 2_000 });
}

test('toolbar has standalone Reset button', async ({ ide, page }) => {
  await ide.runScript('import compat.*;\nplot(1:10);\n');
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });

  // The data-fw-reset="all" attribute marks the standalone Reset button.
  const resetBtn = page.locator('.fw-toolbar [data-fw-reset="all"]');
  await expect(resetBtn).toBeVisible();
  await expect(resetBtn).toContainText(/reset/i);
});

test('toolbar Reset restores both viewport AND display state', async ({ ide, page }) => {
  await ide.runScript(
    'import compat.*;\n'
    + 'plot(1:1000);\n'
    + 'title("hello");\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(150);

  // Toggle ylog ON + title OFF in one menu session, then close menu
  // by clicking the display ▾ trigger again (Escape closes the whole
  // modal, not the popover).
  await openDisplayMenu(page);
  await page.locator('.fw-pop-toggle', { has: page.locator('span', { hasText: 'ylog' }) }).click();
  await page.locator('.fw-pop-toggle', { has: page.locator('span', { hasText: 'title' }) }).click();
  await page.locator('.fw-toolbar .ve-btn', { hasText: /display/i }).click();
  await page.waitForTimeout(80);

  // Title is hidden now.
  await expect(page.locator('.fw-window svg text', { hasText: 'hello' })).toHaveCount(0);

  // Click Reset.
  await page.locator('.fw-toolbar [data-fw-reset="all"]').click();
  await page.waitForTimeout(120);

  // Title visible again.
  await expect(page.locator('.fw-window svg text', { hasText: 'hello' })).toBeVisible();
  // ylog inactive: open menu, the ylog toggle should not show ✓.
  await openDisplayMenu(page);
  const ylogActive = await page.locator('.fw-pop-toggle', {
    has: page.locator('span', { hasText: 'ylog' })
  }).locator('.fw-pop-check').textContent();
  expect(ylogActive.trim()).toBe('');
});

test('display ▾ has legend toggle + reset button', async ({ ide, page }) => {
  await ide.runScript(
    'import compat.*;\n'
    + 'plot(1:10);\n'
    + 'legend(\'a\');\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });

  await openDisplayMenu(page);
  // legend toggle present in the labels section.
  await expect(page.locator('.fw-pop-toggle', {
    has: page.locator('span', { hasText: 'legend' })
  })).toBeVisible();
  // reset button present at the bottom of display ▾.
  await expect(page.locator('.fw-pop button', { hasText: /^reset$/ })).toBeVisible();
});

test('display ▾ reset re-syncs to script defaults', async ({ ide, page }) => {
  await ide.runScript(
    'import compat.*;\n'
    + 'plot(1:1000);\n'
    + 'title("h");\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(120);

  // Toggle title off + click reset in the same menu session.
  await openDisplayMenu(page);
  await page.locator('.fw-pop-toggle', { has: page.locator('span', { hasText: 'title' }) }).click();
  await page.waitForTimeout(80);
  await expect(page.locator('.fw-window svg text', { hasText: 'h' })).toHaveCount(0);

  await page.locator('.fw-pop button', { hasText: /^reset$/ }).click();
  await page.waitForTimeout(120);

  // Title back.
  await expect(page.locator('.fw-window svg text', { hasText: 'h' })).toBeVisible();
});

test('ПКМ has 🏠 Reset + Save/Export submenu', async ({ ide, page }) => {
  await ide.runScript('import compat.*;\nplot(1:10);\n');
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(120);

  await rightClickPlot(page);
  // Reset row.
  await expect(page.locator('.ctx-menu .ctx-item', { hasText: /Reset/ })).toBeVisible();
  // Save/Export submenu trigger.
  await expect(page.locator('.ctx-sub-trigger', { hasText: /Save \/ Export/ })).toBeVisible();
});
