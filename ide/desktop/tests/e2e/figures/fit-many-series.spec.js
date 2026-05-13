// fit-many-series.spec.js — ПКМ context menu series-fit behaviour.
//   ≤5 series → flat list of per-series rows
//   >5 series → folded into a "N curves ▶" submenu
// Toolbar fit ▾ no longer shows per-series rows at all (figure-wide
// only — per-series fit lives exclusively in ПКМ now).

import { test, expect } from '../../helpers/shared.js';

async function rightClickPlot(page) {
  const canvas = page.locator('.fw-window .fw-canvas-wrap svg').first();
  const box = await canvas.boundingBox();
  await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
  await page.mouse.down({ button: 'right' });
  await page.mouse.up({ button: 'right' });
  await expect(page.locator('.ctx-menu')).toBeVisible({ timeout: 2_000 });
}

test('toolbar fit ▾ shows no per-series rows', async ({ ide, page }) => {
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
  // Toolbar fit popover should have NO per-series rows.
  await expect(page.locator('.fw-pop .fw-pop-row')).toHaveCount(0);
  // It SHOULD have X / Y / Z buttons (Z disabled for 2-D).
  await expect(page.locator('.fw-pop button', { hasText: /^X only$/ })).toBeVisible();
  await expect(page.locator('.fw-pop button', { hasText: /^Y only$/ })).toBeVisible();
  await expect(page.locator('.fw-pop button', { hasText: /^Z only$/ })).toBeDisabled();
});

test('ПКМ — 4 series stay flat (no submenu)', async ({ ide, page }) => {
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

  // 4 series → 4 .ctx-row rows directly visible in the parent menu.
  await expect(page.locator('.ctx-menu:not(.ctx-submenu) .ctx-row')).toHaveCount(4);
  // No per-series submenu trigger ("N curves ▶"). The Display submenu
  // ("Display ▶") may still be present — filter it out.
  const subs = page.locator('.ctx-sub-trigger', { hasText: /curves/ });
  await expect(subs).toHaveCount(0);
});

test('ПКМ — 12 series fold into submenu', async ({ ide, page }) => {
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

  // No flat per-series rows in the parent menu.
  await expect(page.locator('.ctx-menu:not(.ctx-submenu) .ctx-row')).toHaveCount(0);
  // "12 curves ▶" submenu trigger present.
  const trigger = page.locator('.ctx-sub-trigger', { hasText: /12 curves/ });
  await expect(trigger).toBeVisible();
  // Hover opens the submenu, every row is in there.
  await trigger.hover();
  await page.waitForTimeout(50);
  await expect(page.locator('.ctx-submenu .ctx-row')).toHaveCount(12);
});

test('ПКМ on 2-D plot has Display submenu but NO Z toggles', async ({ ide, page }) => {
  await ide.runScript(
    'import compat.*;\n'
    + 'plot(1:10);\n'
    + 'title("hello"); xlabel("x");\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(150);
  await rightClickPlot(page);

  // Display submenu present.
  const display = page.locator('.ctx-sub-trigger', { hasText: /Display/ });
  await expect(display).toBeVisible();
  await display.hover();
  await page.waitForTimeout(50);

  // Inside: grid / minor / xlog / ylog / title / xlabel / ylabel.
  // Should NOT contain zlog / zlabel for 2-D figure.
  const sub = page.locator('.ctx-submenu');
  await expect(sub.locator('button', { hasText: /^(✓ )?grid$/ })).toBeVisible();
  await expect(sub.locator('button', { hasText: /^(✓ )?xlog$/ })).toBeVisible();
  await expect(sub.locator('button', { hasText: /^(✓ )?title$/ })).toBeVisible();
  await expect(sub.locator('button', { hasText: /^(✓ )?zlog$/ })).toHaveCount(0);
  await expect(sub.locator('button', { hasText: /^(✓ )?zlabel$/ })).toHaveCount(0);
});
