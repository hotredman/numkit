// display-per-axis-grid.spec.js — toolbar grid ▾ exposes per-axis
// grid toggles (X grid / Y grid) matching MATLAB HG2's XGrid / YGrid
// properties. Combined master `all` row stays as a quick all-axes
// flip. Grid lives in its own toolbar button (split from axes ▾ so
// each menu stays focused).

import { test, expect } from '../../helpers/shared.js';

async function openDisplay(page) {
  // Click the `grid ▾` toolbar button. Regex without anchors because
  // the button textContent is `\n grid ▾\n` (whitespace wraps the
  // inline SVG icon).
  await page.locator('.fw-toolbar .ve-btn', { hasText: /grid/i }).click();
  await expect(page.locator('.fw-pop').first()).toBeVisible({ timeout: 2_000 });
}

test('grid ▾ has X grid + Y grid toggle rows', async ({ ide, page }) => {
  await ide.runScript('import compat.*;\nplot(1:10);\n');
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await openDisplay(page);

  await expect(page.locator('.fw-pop-toggle',
    { has: page.locator('span', { hasText: /^X$/ }) })).toBeVisible();
  await expect(page.locator('.fw-pop-toggle',
    { has: page.locator('span', { hasText: /^Y$/ }) })).toBeVisible();
});

test('X grid toggle flips ✓ independently of Y grid', async ({ ide, page }) => {
  await ide.runScript('import compat.*;\nplot(1:10);\n');
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await openDisplay(page);

  const xRow = page.locator('.fw-pop-toggle',
    { has: page.locator('span', { hasText: /^X$/ }) });
  const yRow = page.locator('.fw-pop-toggle',
    { has: page.locator('span', { hasText: /^Y$/ }) });

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
    { has: page.locator('span', { hasText: /^X$/ }) }).click();
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

  // Master combined-grid row was renamed `grid` → `all` and now
  // lives in the dedicated `grid ▾` popover (split from axes ▾).
  const gridRow = page.locator('.fw-pop-toggle',
    { has: page.locator('span', { hasText: /^all$/ }) });
  const xRow = page.locator('.fw-pop-toggle',
    { has: page.locator('span', { hasText: /^X$/ }) });
  const yRow = page.locator('.fw-pop-toggle',
    { has: page.locator('span', { hasText: /^Y$/ }) });

  await gridRow.click();
  await page.waitForTimeout(80);
  expect((await gridRow.locator('.fw-pop-check').textContent()).trim()).toBe('✓');
  expect((await xRow.locator('.fw-pop-check').textContent()).trim()).toBe('✓');
  expect((await yRow.locator('.fw-pop-check').textContent()).trim()).toBe('✓');
});
