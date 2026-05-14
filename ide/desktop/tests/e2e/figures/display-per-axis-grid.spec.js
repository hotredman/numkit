// display-per-axis-grid.spec.js — toolbar display ▾ now exposes
// per-axis grid toggles (X grid / Y grid) matching MATLAB HG2's
// XGrid / YGrid properties. Combined "grid" row stays as quick
// all-axes flip.

import { test, expect } from '../../helpers/shared.js';

async function openDisplay(page) {
  await page.locator('.fw-toolbar .ve-btn', { hasText: /decoration/i }).click();
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

test('X grid toggle draws ONLY vertical lines (no horizontal)', async ({ ide, page }) => {
  // Renderer split: XGrid → vertical lines (x1==x2), YGrid → horizontal
  // (y1==y2). Toggle X only and assert vertical lines appear, no
  // horizontal grid lines.
  await ide.runScript('import compat.*;\nplot(1:10);\n');
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await openDisplay(page);
  await page.locator('.fw-pop-toggle',
    { has: page.locator('span', { hasText: 'X grid' }) }).click();
  await page.waitForTimeout(120);

  const lines = await page.locator('.fw-window svg line[stroke*="--plot-grid"]:not([stroke*="--plot-grid-min"])').evaluateAll((els) =>
    els.map((el) => ({
      x1: parseFloat(el.getAttribute('x1')),
      x2: parseFloat(el.getAttribute('x2')),
      y1: parseFloat(el.getAttribute('y1')),
      y2: parseFloat(el.getAttribute('y2')),
    }))
  );
  const vertical   = lines.filter((l) => Math.abs(l.x1 - l.x2) < 0.5);
  const horizontal = lines.filter((l) => Math.abs(l.y1 - l.y2) < 0.5);
  expect(vertical.length, `expected vertical X-grid lines, got ${vertical.length}`).toBeGreaterThan(0);
  expect(horizontal.length, `Y-grid should be off, got ${horizontal.length}`).toBe(0);
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
