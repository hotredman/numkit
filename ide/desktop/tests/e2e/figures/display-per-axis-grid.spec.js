// display-per-axis-grid.spec.js — toolbar grid ▾ exposes per-axis
// grid toggles (X / Y) matching MATLAB HG2's XGrid / YGrid properties.
// After the matrix-layout refactor each axis row carries TWO state
// buttons (major / minor) instead of separate rows; this spec drives
// the major button only — minor coverage lives in display-menu.spec.js.

import { test, expect } from '../../helpers/shared.js';

async function openGrid(page) {
  // Click the `grid ▾` toolbar button. Regex without anchors because
  // the button textContent is `\n grid ▾\n` (whitespace wraps the
  // inline SVG icon).
  await page.locator('.fw-toolbar .ve-btn', { hasText: /grid/i }).click();
  await expect(page.locator('.fw-pop').first()).toBeVisible({ timeout: 2_000 });
}

/** Grid ▾ matrix-row locator. Each axis row holds the label + two
 *  state buttons: [0] major, [1] minor. */
function gridRow(page, axis) {
  return page.locator('.fw-pop-matrix .fw-pop-mrow', {
    has: page.locator('.fw-pop-mrow-label', { hasText: new RegExp(`^${axis}$`) }),
  });
}
const majorBtn = (page, axis) => gridRow(page, axis).locator('.fw-pop-mbtn').nth(0);
const minorBtn = (page, axis) => gridRow(page, axis).locator('.fw-pop-mbtn').nth(1);
async function isBtnActive(btn) {
  const cls = (await btn.getAttribute('class')) || '';
  return /\bis-active\b/.test(cls);
}

test('grid ▾ has X + Y matrix rows with major/minor buttons', async ({ ide, page }) => {
  await ide.runScript('import compat.*;\nplot(1:10);\n');
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await openGrid(page);

  await expect(gridRow(page, 'X')).toBeVisible();
  await expect(gridRow(page, 'Y')).toBeVisible();
  // Each row has exactly 2 state buttons (major / minor).
  await expect(gridRow(page, 'X').locator('.fw-pop-mbtn')).toHaveCount(2);
  await expect(gridRow(page, 'Y').locator('.fw-pop-mbtn')).toHaveCount(2);
});

test('X grid major flips is-active independently of Y grid major', async ({ ide, page }) => {
  await ide.runScript('import compat.*;\nplot(1:10);\n');
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await openGrid(page);

  await majorBtn(page, 'X').click();
  await page.waitForTimeout(80);
  expect(await isBtnActive(majorBtn(page, 'X')), 'X major should be active').toBe(true);
  expect(await isBtnActive(majorBtn(page, 'Y')), 'Y major should stay inactive').toBe(false);
});

test('X grid major draws ONLY vertical lines (no horizontal)', async ({ ide, page }) => {
  // Renderer split: XGrid → vertical lines (x1==x2), YGrid → horizontal
  // (y1==y2). Toggle X major and assert vertical lines appear, no
  // horizontal grid lines.
  await ide.runScript('import compat.*;\nplot(1:10);\n');
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await openGrid(page);
  await majorBtn(page, 'X').click();
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

test('combined `all` major drives both X grid AND Y grid', async ({ ide, page }) => {
  await ide.runScript('import compat.*;\nplot(1:10);\n');
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await openGrid(page);

  // Master combined row in the matrix layout: label `all`, button [0]
  // is the master major toggle, button [1] is the master minor toggle.
  // Lives in the same dedicated `grid ▾` popover (split from axes ▾).
  await majorBtn(page, 'all').click();
  await page.waitForTimeout(80);
  expect(await isBtnActive(majorBtn(page, 'all')), '`all` major should be active').toBe(true);
  expect(await isBtnActive(majorBtn(page, 'X')),   'X major fanned on').toBe(true);
  expect(await isBtnActive(majorBtn(page, 'Y')),   'Y major fanned on').toBe(true);
});

test('X minor button toggles independently of X major', async ({ ide, page }) => {
  // Matrix layout means X has its own minor button; flipping it
  // shouldn't touch X's major bit.
  await ide.runScript('import compat.*;\nplot(1:10);\n');
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await openGrid(page);

  await minorBtn(page, 'X').click();
  await page.waitForTimeout(80);
  expect(await isBtnActive(minorBtn(page, 'X')), 'X minor should be active').toBe(true);
  expect(await isBtnActive(majorBtn(page, 'X')), 'X major must stay inactive').toBe(false);
});
