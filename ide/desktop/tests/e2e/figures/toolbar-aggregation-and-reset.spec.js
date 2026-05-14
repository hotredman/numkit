// toolbar-aggregation-and-reset.spec.js
// 1. Toolbar display ▾ ✓ shows only when EVERY cell has the option set
//    (after applying per-cell ПКМ overrides). Toolbar colormap ▾ same.
// 2. Reset row is at the TOP of every popover.
// 3. ПКМ Display submenu and Colormap submenu both have a top reset row;
//    in subplot mode it clears only this cell's override.

import { test, expect } from '../../helpers/shared.js';

async function rightClickCell(page, idx) {
  const svg = page.locator('.fw-window .fw-canvas-wrap svg').nth(idx);
  const box = await svg.boundingBox();
  await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
  await page.mouse.down({ button: 'right' });
  await page.mouse.up({ button: 'right' });
  await expect(page.locator('.ctx-menu')).toBeVisible({ timeout: 2_000 });
}

async function openToolbarMenu(page, label) {
  await page.locator('.fw-toolbar .ve-btn', { hasText: new RegExp(label, 'i') }).click();
  await expect(page.locator('.fw-pop').first()).toBeVisible({ timeout: 2_000 });
}

test('toolbar reset rows are at TOP of popovers (axes ▾, decoration ▾, colormap ▾)', async ({ ide, page }) => {
  await ide.runScript('import compat.*;\nimagesc(rand(8,8));\n');
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(100);

  // axes ▾ first child = "reset" button.
  await openToolbarMenu(page, 'axes');
  const firstAxes = page.locator('.fw-pop > .fw-pop-section').first()
    .locator('button').first();
  await expect(firstAxes).toContainText(/^reset$/);
  await page.locator('.fw-toolbar .ve-btn', { hasText: /axes/i }).click(); // close

  // decoration ▾ first child = "reset" button.
  await openToolbarMenu(page, 'decoration');
  const firstDecoration = page.locator('.fw-pop > .fw-pop-section').first()
    .locator('button').first();
  await expect(firstDecoration).toContainText(/^reset$/);
  await page.locator('.fw-toolbar .ve-btn', { hasText: /decoration/i }).click(); // close

  // colormap ▾ first child = "reset" button.
  await openToolbarMenu(page, 'colormap');
  const firstCmap = page.locator('.fw-pop > .fw-pop-section').first()
    .locator('button').first();
  await expect(firstCmap).toContainText(/^reset$/);
});

test('ПКМ Display submenu has reset row at TOP', async ({ ide, page }) => {
  await ide.runScript(
    'import compat.*;\n'
    + 'figure;\n'
    + 'subplot(1,2,1); plot(1:10);\n'
    + 'subplot(1,2,2); plot(1:10);\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(100);

  await rightClickCell(page, 0);
  await page.locator('.ctx-sub-trigger', { hasText: /Display/ }).hover();
  await page.waitForTimeout(60);
  // First button in the submenu should be "reset".
  const firstBtn = page.locator('.ctx-submenu > .ctx-item').first();
  await expect(firstBtn).toContainText(/^reset$/);
});

test('ПКМ Colormap submenu has reset row at TOP', async ({ ide, page }) => {
  await ide.runScript(
    'import compat.*;\n'
    + 'figure;\n'
    + 'subplot(1,2,1); imagesc(rand(8,8));\n'
    + 'subplot(1,2,2); imagesc(rand(8,8));\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(100);

  await rightClickCell(page, 0);
  await page.locator('.ctx-sub-trigger', { hasText: /Colormap/ }).hover();
  await page.waitForTimeout(60);
  const firstBtn = page.locator('.ctx-submenu > .ctx-item').first();
  await expect(firstBtn).toContainText(/^reset$/);
});

test('toolbar decoration ▾ ✓ aggregates: only set when ALL cells have it', async ({ ide, page }) => {
  await ide.runScript(
    'import compat.*;\n'
    + 'figure;\n'
    + 'subplot(1,2,1); plot(1:10);\n'
    + 'subplot(1,2,2); plot(1:10);\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(120);

  // Pre: nothing on. ✓ on grid is empty.
  await openToolbarMenu(page, 'decoration');
  const gridToggle = page.locator('.fw-pop-toggle',
    { has: page.locator('span', { hasText: /^grid$/ }) });
  expect((await gridToggle.locator('.fw-pop-check').textContent()).trim()).toBe('');
  await page.locator('.fw-toolbar .ve-btn', { hasText: /decoration/i }).click();  // close
  await page.waitForTimeout(50);

  // Toggle grid on cell A only via ПКМ.
  await rightClickCell(page, 0);
  await page.locator('.ctx-sub-trigger', { hasText: /Display/ }).hover();
  await page.waitForTimeout(60);
  await page.locator('.ctx-submenu button', { hasText: /^(✓ )?grid$/ }).click();
  await page.waitForTimeout(120);

  // Toolbar decoration ▾ → grid still has NO ✓ (only one cell on, not all).
  await openToolbarMenu(page, 'decoration');
  const partial = await gridToggle.locator('.fw-pop-check').textContent();
  expect(partial.trim(), `partial-state ✓ should be empty: '${partial}'`).toBe('');
  await page.locator('.fw-toolbar .ve-btn', { hasText: /decoration/i }).click();
  await page.waitForTimeout(50);

  // Now toggle grid on cell B too.
  await rightClickCell(page, 1);
  await page.locator('.ctx-sub-trigger', { hasText: /Display/ }).hover();
  await page.waitForTimeout(60);
  await page.locator('.ctx-submenu button', { hasText: /^(✓ )?grid$/ }).click();
  await page.waitForTimeout(120);

  // Now toolbar shows ✓.
  await openToolbarMenu(page, 'decoration');
  const allOn = await gridToggle.locator('.fw-pop-check').textContent();
  expect(allOn.trim(), `all-on ✓ should be set: '${allOn}'`).toBe('✓');
});

test('toolbar colormap ▾ ✓ aggregates: only set when ALL cells use that palette', async ({ ide, page }) => {
  await ide.runScript(
    'import compat.*;\n'
    + 'figure;\n'
    + 'subplot(1,2,1); imagesc(rand(8,8));\n'
    + 'subplot(1,2,2); imagesc(rand(8,8));\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(120);

  // Default: both parula → ✓ on parula, no ✓ on jet.
  await openToolbarMenu(page, 'colormap');
  const parulaCheck = await page.locator('.fw-pop button',
    { has: page.locator('span', { hasText: /^parula$/ }) }).first()
    .locator('.fw-pop-check').textContent();
  expect(parulaCheck.trim()).toBe('✓');
  await page.locator('.fw-toolbar .ve-btn', { hasText: /^colormap/i }).click();  // close
  await page.waitForTimeout(50);

  // Per-cell A → jet via ПКМ.
  await rightClickCell(page, 0);
  await page.locator('.ctx-sub-trigger', { hasText: /Colormap/ }).hover();
  await page.waitForTimeout(60);
  await page.locator('.ctx-submenu button', { hasText: /^(✓ )?jet$/ }).click();
  await page.waitForTimeout(120);

  // Now NEITHER parula nor jet has ✓ in toolbar (mixed).
  await openToolbarMenu(page, 'colormap');
  const parulaMix = await page.locator('.fw-pop button',
    { has: page.locator('span', { hasText: /^parula$/ }) }).first()
    .locator('.fw-pop-check').textContent();
  const jetMix = await page.locator('.fw-pop button',
    { has: page.locator('span', { hasText: /^jet$/ }) }).first()
    .locator('.fw-pop-check').textContent();
  expect(parulaMix.trim(), `parula in mixed: ${parulaMix}`).toBe('');
  expect(jetMix.trim(), `jet in mixed: ${jetMix}`).toBe('');
});
