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

async function openDecorationMenu(page) {
  await page.locator('.fw-toolbar .ve-btn', { hasText: /decoration/i }).click();
  await expect(page.locator('.fw-pop').first()).toBeVisible({ timeout: 2_000 });
}

async function openAxesMenu(page) {
  await page.locator('.fw-toolbar .ve-btn', { hasText: /axes/i }).click();
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

  // After the axes/decoration split: Y log lives in axes ▾ under
  // `log scale:` section, title lives in decoration ▾ (labels).
  // Section-scoped locator for the log row — `Y` also appears under
  // axes ▾ `reverse:`.
  const yLog = page.locator('.fw-pop-section', {
    has: page.locator('.fw-pop-head', { hasText: /^log scale$/ }),
  }).locator('.fw-pop-toggle', {
    has: page.locator('span', { hasText: /^Y$/ }),
  });

  await openAxesMenu(page);
  await yLog.click();
  await page.locator('.fw-toolbar .ve-btn', { hasText: /axes/i }).click();
  await page.waitForTimeout(50);

  await openDecorationMenu(page);
  await page.locator('.fw-pop-toggle', { has: page.locator('span', { hasText: 'title' }) }).click();
  await page.locator('.fw-toolbar .ve-btn', { hasText: /decoration/i }).click();
  await page.waitForTimeout(80);

  // Title is hidden now.
  await expect(page.locator('.fw-window svg text', { hasText: 'hello' })).toHaveCount(0);

  // Click Reset.
  await page.locator('.fw-toolbar [data-fw-reset="all"]').click();
  await page.waitForTimeout(120);

  // Title visible again.
  await expect(page.locator('.fw-window svg text', { hasText: 'hello' })).toBeVisible();
  // Y log inactive: open axes ▾, the row should not show ✓.
  await openAxesMenu(page);
  const yLogCheck = await yLog.locator('.fw-pop-check').textContent();
  expect(yLogCheck.trim()).toBe('');
});

test('decoration ▾ has legend toggle + `default` button', async ({ ide, page }) => {
  await ide.runScript(
    'import compat.*;\n'
    + 'plot(1:10);\n'
    + 'legend(\'a\');\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });

  await openDecorationMenu(page);
  // legend toggle present in the labels section.
  await expect(page.locator('.fw-pop-toggle', {
    has: page.locator('span', { hasText: 'legend' })
  })).toBeVisible();
  // `default` button present at the top of decoration ▾.
  await expect(page.locator('.fw-pop button', { hasText: /^default$/ })).toBeVisible();
});

test('decoration ▾ `default` re-syncs to script values', async ({ ide, page }) => {
  await ide.runScript(
    'import compat.*;\n'
    + 'plot(1:1000);\n'
    + 'title("h");\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(120);

  // Toggle title off + click `default` in the same menu session.
  await openDecorationMenu(page);
  await page.locator('.fw-pop-toggle', { has: page.locator('span', { hasText: 'title' }) }).click();
  await page.waitForTimeout(80);
  await expect(page.locator('.fw-window svg text', { hasText: 'h' })).toHaveCount(0);

  await page.locator('.fw-pop button', { hasText: /^default$/ }).click();
  await page.waitForTimeout(120);

  // Title back.
  await expect(page.locator('.fw-window svg text', { hasText: 'h' })).toBeVisible();
});

test('ПКМ order: Reset · Save · Axes · Grid · Decoration · Fit', async ({ ide, page }) => {
  await ide.runScript(
    'import compat.*;\n'
    + 'plot(1:10);\n'
    + 'title("h"); xlabel("x");\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(120);

  await rightClickPlot(page);

  // Read text of every direct child of the parent .ctx-menu in order.
  const labels = await page.locator('.ctx-menu:not(.ctx-submenu)')
    .first()
    .locator(':scope > .ctx-item, :scope > .ctx-sub-wrap > .ctx-sub-trigger, :scope > .ctx-head, :scope > .ctx-sep')
    .evaluateAll((els) => els.map((el) => el.textContent.trim()));

  // Display ▶ split into Axes ▶ + Decoration ▶, then Grid ▶ extracted
  // out of Axes ▶ — mirrors the toolbar's axes ▾ + grid ▾ split.
  const idxReset      = labels.findIndex((s) => /^Reset$/i.test(s));
  const idxSave       = labels.findIndex((s) => /Save \/ Export/.test(s));
  const idxAxes       = labels.findIndex((s) => /Axes/.test(s));
  const idxGrid       = labels.findIndex((s) => /Grid/.test(s));
  const idxDecoration = labels.findIndex((s) => /Decoration/.test(s));
  const idxFit        = labels.findIndex((s) => /^Fit /.test(s));
  expect(idxReset, `labels: ${labels.join(' | ')}`).toBeGreaterThanOrEqual(0);
  expect(idxSave).toBeGreaterThan(idxReset);
  expect(idxAxes).toBeGreaterThan(idxSave);
  expect(idxGrid).toBeGreaterThan(idxAxes);
  expect(idxDecoration).toBeGreaterThan(idxGrid);
  expect(idxFit).toBeGreaterThan(idxDecoration);
});

test('ПКМ Reset row uses SVG house icon, not emoji', async ({ ide, page }) => {
  await ide.runScript('import compat.*;\nplot(1:10);\n');
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(120);

  await rightClickPlot(page);

  const reset = page.locator('.ctx-menu .ctx-item', { hasText: /^Reset$/ }).first();
  await expect(reset).toBeVisible();
  // Inline SVG present (no emoji).
  await expect(reset.locator('svg')).toHaveCount(1);
  // Text content is "Reset" only — no 🏠 codepoint.
  const text = await reset.textContent();
  expect(text).not.toContain('🏠');
});

test('ПКМ Axes ▶ / Grid ▶ / Decoration ▶ each have the right section heads', async ({ ide, page }) => {
  await ide.runScript(
    'import compat.*;\n'
    + 'plot(1:10);\n'
    + 'title("t"); xlabel("x"); ylabel("y");\n'
    + 'legend(\'a\');\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(120);

  await rightClickPlot(page);

  // Axes ▶ — Axes-object props minus grid (grid lives in Grid ▶ now).
  // Heads name the active state of the toggle group; `aspect` got
  // added when the MATLAB `axis equal/square/image/tight/auto`
  // shorthand got its own UI section in axes ▾ / ПКМ Axes ▶.
  await page.locator('.ctx-sub-trigger', { hasText: /Axes/ }).hover();
  await page.waitForTimeout(80);
  const axesHeads = await page.locator('.ctx-submenu .ctx-head').allTextContents();
  expect(axesHeads).toEqual(['visible', 'reverse', 'log scale', 'aspect']);

  // Grid ▶ — master + Cartesian (CompositePlot specialised).
  await page.locator('.ctx-sub-trigger', { hasText: /Grid/ }).hover();
  await page.waitForTimeout(80);
  const gridHeads = await page.locator('.ctx-submenu .ctx-head').allTextContents();
  expect(gridHeads).toEqual(['grid', 'Cartesian']);

  // Decoration ▶ — switching subs via hover (mouseleave on Axes
  // closes its sub; mouseenter on Decoration opens its sub). No need
  // to dismiss the parent ПКМ — Escape would close the whole modal.
  await page.locator('.ctx-sub-trigger', { hasText: /Decoration/ }).hover();
  await page.waitForTimeout(80);
  const decHeads = await page.locator('.ctx-submenu .ctx-head').allTextContents();
  expect(decHeads).toEqual(['labels', 'annotations']);
  // Legend toggle now under annotations section.
  await expect(page.locator('.ctx-submenu button', { hasText: /^(✓ )?legend$/ })).toBeVisible();
});
