// display-per-axis-grid.spec.js — toolbar display ▾ now exposes
// per-axis grid toggles (X grid / Y grid) matching MATLAB HG2's
// XGrid / YGrid properties. Combined "grid" row stays as quick
// all-axes flip.

import { test, expect } from '../../helpers/shared.js';

async function openDisplay(page) {
  await page.locator('.fw-toolbar .ve-btn', { hasText: /display/i }).click();
  await expect(page.locator('.fw-pop').first()).toBeVisible({ timeout: 2_000 });
}

test('display ▾ has X grid + Y grid toggle rows', async ({ ide, page }) => {
  await ide.runScript('import compat.*;\nplot(1:10);\n');
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await openDisplay(page);

  await expect(page.locator('.fw-pop-toggle',
    { has: page.locator('span', { hasText: 'X grid' }) })).toBeVisible();
  await expect(page.locator('.fw-pop-toggle',
    { has: page.locator('span', { hasText: 'Y grid' }) })).toBeVisible();
});

test('X grid toggle flips ✓ independently of Y grid', async ({ ide, page }) => {
  await ide.runScript('import compat.*;\nplot(1:10);\n');
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await openDisplay(page);

  const xRow = page.locator('.fw-pop-toggle',
    { has: page.locator('span', { hasText: 'X grid' }) });
  const yRow = page.locator('.fw-pop-toggle',
    { has: page.locator('span', { hasText: 'Y grid' }) });

  // Click X grid only.
  await xRow.click();
  await page.waitForTimeout(80);
  expect((await xRow.locator('.fw-pop-check').textContent()).trim()).toBe('✓');
  expect((await yRow.locator('.fw-pop-check').textContent()).trim()).toBe('');
});

test('combined grid toggle drives both X grid AND Y grid', async ({ ide, page }) => {
  await ide.runScript('import compat.*;\nplot(1:10);\n');
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await openDisplay(page);

  const gridRow = page.locator('.fw-pop-toggle',
    { has: page.locator('span', { hasText: /^grid$/ }) });
  const xRow = page.locator('.fw-pop-toggle',
    { has: page.locator('span', { hasText: 'X grid' }) });
  const yRow = page.locator('.fw-pop-toggle',
    { has: page.locator('span', { hasText: 'Y grid' }) });

  await gridRow.click();
  await page.waitForTimeout(80);
  expect((await gridRow.locator('.fw-pop-check').textContent()).trim()).toBe('✓');
  expect((await xRow.locator('.fw-pop-check').textContent()).trim()).toBe('✓');
  expect((await yRow.locator('.fw-pop-check').textContent()).trim()).toBe('✓');
});
