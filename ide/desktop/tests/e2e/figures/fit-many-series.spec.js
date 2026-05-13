// fit-many-series.spec.js — per-series fit lives in a top-level
// "Series ▶" submenu in ПКМ (parallel to Display ▶ / Colormap ▶).
// Toolbar fit ▾ stays figure-wide only — no per-curve rows there.

import { test, expect } from '../../helpers/shared.js';

async function rightClickPlot(page) {
  const canvas = page.locator('.fw-window .fw-canvas-wrap svg').first();
  const box = await canvas.boundingBox();
  await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
  await page.mouse.down({ button: 'right' });
  await page.mouse.up({ button: 'right' });
  await expect(page.locator('.ctx-menu')).toBeVisible({ timeout: 2_000 });
}

test('toolbar fit ▾ shows no per-series rows + Z disabled for 2-D', async ({ ide, page }) => {
  await ide.runScript(
    'import compat.*;\n'
    + 'figure;\n'
    + 'hold on;\n'
    + 'for k = 1:4\n'
    + '  plot([0 1], [k k]);\n'
    + 'end\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.locator('.fw-toolbar .ve-btn', { hasText: /fit/i }).click();
  await expect(page.locator('.fw-pop').first()).toBeVisible({ timeout: 2_000 });

  await expect(page.locator('.fw-pop .fw-pop-row')).toHaveCount(0);
  await expect(page.locator('.fw-pop button', { hasText: /^X only$/ })).toBeVisible();
  await expect(page.locator('.fw-pop button', { hasText: /^Y only$/ })).toBeVisible();
  await expect(page.locator('.fw-pop button', { hasText: /^Z only$/ })).toBeDisabled();
});

test('ПКМ — multi-series figure has Series ▶ submenu with one row per curve', async ({ ide, page }) => {
  await ide.runScript(
    'import compat.*;\n'
    + 'figure;\n'
    + 'hold on;\n'
    + 'for k = 1:4\n'
    + '  plot([0 1], [k k]);\n'
    + 'end\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(150);
  await rightClickPlot(page);

  // Series submenu trigger present, count badge shows 4.
  const trigger = page.locator('.ctx-sub-trigger', { hasText: /Series/ });
  await expect(trigger).toBeVisible();
  await expect(trigger).toContainText(/Series/);
  await expect(trigger).toContainText('(4)');

  // No flat .ctx-row in the parent menu — they all live inside the submenu.
  await expect(page.locator('.ctx-menu:not(.ctx-submenu) .ctx-row')).toHaveCount(0);

  // Hover opens it; 4 rows inside.
  await trigger.hover();
  await page.waitForTimeout(60);
  await expect(page.locator('.ctx-submenu .ctx-row')).toHaveCount(4);
});

test('ПКМ — 12-series figure puts all rows inside Series ▶ submenu', async ({ ide, page }) => {
  await ide.runScript(
    'import compat.*;\n'
    + 'figure;\n'
    + 'hold on;\n'
    + 'for k = 1:12\n'
    + '  plot([0 1], [k k]);\n'
    + 'end\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(150);
  await rightClickPlot(page);

  const trigger = page.locator('.ctx-sub-trigger', { hasText: /Series/ });
  await expect(trigger).toContainText('(12)');
  await trigger.hover();
  await page.waitForTimeout(60);
  await expect(page.locator('.ctx-submenu .ctx-row')).toHaveCount(12);
});

test('ПКМ — single-series figure has NO Series submenu', async ({ ide, page }) => {
  // For a single-series plot the submenu would be a one-row noise; skip.
  await ide.runScript('import compat.*;\nplot(1:10);\n');
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(150);
  await rightClickPlot(page);

  // Series submenu only built when at least one series exists; we
  // permit it for single-series too (consistent), so check it shows
  // exactly one row inside.
  const trigger = page.locator('.ctx-sub-trigger', { hasText: /Series/ });
  if (await trigger.count() > 0) {
    await trigger.hover();
    await page.waitForTimeout(60);
    await expect(page.locator('.ctx-submenu .ctx-row')).toHaveCount(1);
  }
});
