// colorbar-and-colormap.spec.js — verifies the colorbar bug fix +
// new toggle/menu entries:
//   1. imagesc(I) WITHOUT a colorbar() call → no colorbar drawn
//      (MATLAB parity, was a bug — IDE used to auto-show for heatmap).
//   2. imagesc(I); colorbar; → colorbar drawn.
//   3. display ▾ toggle 'colorbar' flips visibility.
//   4. ПКМ → Display submenu has 'colorbar' toggle.
//   5. ПКМ → Colormap submenu lists palettes.

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
  await page.locator('.fw-toolbar .ve-btn', { hasText: /decoration/i }).click();
  await expect(page.locator('.fw-pop').first()).toBeVisible({ timeout: 2_000 });
}

// Colorbar = a <rect> filled with the cbar gradient. The gradient
// itself sits in <defs> regardless of bar visibility, so we count the
// actual rect that uses it instead.
async function colorbarBarCount(page) {
  return await page.locator('.fw-window svg rect[fill^="url(#cbar-"]').count();
}

test('imagesc without colorbar() call — no colorbar drawn (MATLAB parity)', async ({ ide, page }) => {
  await ide.runScript(
    'import compat.*;\n'
    + 'imagesc(rand(8, 8));\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(150);

  expect(await colorbarBarCount(page),
    'colorbar should NOT auto-show until colorbar() is called').toBe(0);
});

test('imagesc + colorbar — colorbar IS drawn', async ({ ide, page }) => {
  await ide.runScript(
    'import compat.*;\n'
    + 'imagesc(rand(8, 8));\n'
    + 'colorbar;\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(150);

  expect(await colorbarBarCount(page)).toBeGreaterThan(0);
});

test('display ▾ has colorbar toggle and it flips visibility', async ({ ide, page }) => {
  await ide.runScript(
    'import compat.*;\n'
    + 'imagesc(rand(8, 8));\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(150);

  // Pre: no colorbar.
  expect(await colorbarBarCount(page)).toBe(0);

  await openDisplayMenu(page);
  await page.locator('.fw-pop-toggle', {
    has: page.locator('span', { hasText: 'colorbar' })
  }).click();
  await page.waitForTimeout(120);

  expect(await colorbarBarCount(page)).toBeGreaterThan(0);
});

test('ПКМ Display submenu includes colorbar toggle', async ({ ide, page }) => {
  await ide.runScript(
    'import compat.*;\n'
    + 'imagesc(rand(8, 8));\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(150);

  await rightClickPlot(page);
  await page.locator('.ctx-sub-trigger', { hasText: /Display/ }).hover();
  await page.waitForTimeout(60);

  await expect(page.locator('.ctx-submenu button',
    { hasText: /^(✓ )?colorbar$/ })).toBeVisible();
});

test('ПКМ has Colormap submenu with palette list', async ({ ide, page }) => {
  await ide.runScript(
    'import compat.*;\n'
    + 'imagesc(rand(8, 8));\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(150);

  await rightClickPlot(page);
  await page.locator('.ctx-sub-trigger', { hasText: /Colormap/ }).hover();
  await page.waitForTimeout(60);

  // Several known palettes.
  await expect(page.locator('.ctx-submenu button', { hasText: /^(✓ )?parula$/ })).toBeVisible();
  await expect(page.locator('.ctx-submenu button', { hasText: /^(✓ )?jet$/ })).toBeVisible();
  await expect(page.locator('.ctx-submenu button', { hasText: /^(✓ )?viridis$/ })).toBeVisible();
});

test('ПКМ Colormap on non-heatmap figure is absent', async ({ ide, page }) => {
  await ide.runScript('import compat.*;\nplot(1:10);\n');
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(120);

  await rightClickPlot(page);
  // No Colormap submenu trigger for plain plot.
  await expect(page.locator('.ctx-sub-trigger', { hasText: /Colormap/ })).toHaveCount(0);
});
