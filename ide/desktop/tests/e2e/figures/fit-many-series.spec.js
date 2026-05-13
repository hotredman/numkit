// fit-many-series.spec.js — per-series rows in fit ▾.
//   ≤5 series → flat list (every row directly visible)
//   >5 series → folded into a "N curves ▶" submenu that opens to the side
// Same rule applies to ContextMenu (right-click on plot).

import { test, expect } from '../../helpers/shared.js';

async function openFitMenu(page) {
  await page.locator('.fw-toolbar .ve-btn', { hasText: /fit/i }).click();
  await expect(page.locator('.fw-pop').first()).toBeVisible({ timeout: 2_000 });
}

test('fit ▾ — 4 series stay flat (no submenu)', async ({ ide, page }) => {
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

  await openFitMenu(page);
  // 4 series → 4 rows visible inline. No submenu trigger.
  await expect(page.locator('.fw-pop-row')).toHaveCount(4);
  await expect(page.locator('.fw-pop-sub-trigger')).toHaveCount(0);
});

test('fit ▾ — 12 series fold into submenu', async ({ ide, page }) => {
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

  await openFitMenu(page);

  // No flat rows in the parent popover.
  await expect(page.locator('.fw-pop > .fw-pop-section .fw-pop-row')).toHaveCount(0);

  // Submenu trigger present, label includes the count.
  const trigger = page.locator('.fw-pop-sub-trigger');
  await expect(trigger).toBeVisible();
  await expect(trigger).toContainText('12 curves');

  // Hover opens submenu, every row is in there.
  await trigger.hover();
  await page.waitForTimeout(50);
  await expect(page.locator('.fw-pop-sub .fw-pop-row')).toHaveCount(12);
});

test('ПКМ context menu — 12 series fold into submenu', async ({ ide, page }) => {
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

  // Right-click on the plot canvas.
  const canvas = page.locator('.fw-window .fw-canvas-wrap svg').first();
  const box = await canvas.boundingBox();
  await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
  await page.mouse.down({ button: 'right' });
  await page.mouse.up({ button: 'right' });
  await expect(page.locator('.ctx-menu')).toBeVisible({ timeout: 2_000 });

  // Submenu trigger present.
  const trigger = page.locator('.ctx-sub-trigger');
  await expect(trigger).toBeVisible();
  await expect(trigger).toContainText('12 curves');

  // No flat .ctx-row in the parent menu.
  await expect(page.locator('.ctx-menu:not(.ctx-submenu) > .ctx-row')).toHaveCount(0);

  // Open submenu, count rows.
  await trigger.hover();
  await page.waitForTimeout(50);
  await expect(page.locator('.ctx-submenu .ctx-row')).toHaveCount(12);
});

test('ПКМ context menu — 4 series stay flat (no submenu)', async ({ ide, page }) => {
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

  const canvas = page.locator('.fw-window .fw-canvas-wrap svg').first();
  const box = await canvas.boundingBox();
  await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
  await page.mouse.down({ button: 'right' });
  await page.mouse.up({ button: 'right' });
  await expect(page.locator('.ctx-menu')).toBeVisible({ timeout: 2_000 });

  // 4 rows visible inline, no submenu.
  await expect(page.locator('.ctx-row')).toHaveCount(4);
  await expect(page.locator('.ctx-sub-trigger')).toHaveCount(0);
});
