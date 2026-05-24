// display-axes-toggles.spec.js — axes ▾ button: `visible: axis/box`
// and a `reverse · log scale` matrix (rows = X/Y/Z, columns =
// reverse / log). Maps to MATLAB Visible / Box / XDir·YDir·ZDir /
// XScale·YScale·ZScale.
//
// Matrix layout collapses what used to be two separate per-axis
// sections (`reverse:` and `log scale:`, 3 rows each) into one
// 4-row block (header + X/Y/Z). Same checkbox-style cells as the
// grid ▾ matrix.

import { test, expect } from '../../helpers/shared.js';

async function openDisplay(page) {
  await page.locator('.fw-toolbar .ve-btn', { hasText: /axes/i }).click();
  await expect(page.locator('.fw-pop').first()).toBeVisible({ timeout: 2_000 });
}

/** Locate one cell in the axes-matrix by (axis, col index).
 *  col: 0 = reverse, 1 = log. */
function axisBtn(page, axis, col) {
  return page.locator('.fw-pop-matrix .fw-pop-mrow', {
    has: page.locator('.fw-pop-mrow-label', { hasText: new RegExp(`^${axis}$`) }),
  }).locator('.fw-pop-mbtn').nth(col);
}
async function isActive(btn) {
  const cls = (await btn.getAttribute('class')) || '';
  return /\bis-active\b/.test(cls);
}

test('axes ▾ has visible/box rows + reverse/log matrix with X/Y/Z', async ({ ide, page }) => {
  await ide.runScript('import compat.*;\nplot(1:10);\n');
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await openDisplay(page);

  // visible section — old DisplayToggle rows.
  await expect(page.locator('.fw-pop-toggle', {
    has: page.locator('span', { hasText: /^axis$/ }),
  })).toBeVisible();
  await expect(page.locator('.fw-pop-toggle', {
    has: page.locator('span', { hasText: /^box$/ }),
  })).toBeVisible();

  // Matrix rows X/Y/Z, each with 2 buttons.
  for (const ax of ['X', 'Y', 'Z']) {
    await expect(axisBtn(page, ax, 0)).toBeVisible();   // reverse
    await expect(axisBtn(page, ax, 1)).toBeVisible();   // log
  }
});

test('toggle "axis" hides x/y tick lines + box', async ({ ide, page }) => {
  await ide.runScript('import compat.*;\nplot(1:10);\n');
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });

  const ticksBefore = await page.locator('.fw-window svg text[text-anchor="end"]').count();
  expect(ticksBefore).toBeGreaterThan(0);

  await openDisplay(page);
  await page.locator('.fw-pop-toggle',
    { has: page.locator('span', { hasText: /^axis$/ }) }).click();
  await page.waitForTimeout(120);

  const ticksAfter = await page.locator('.fw-window svg text[text-anchor="end"]').count();
  expect(ticksAfter, `axis-off should hide tick labels, got ${ticksAfter}`).toBe(0);
});

test('toggle "Y reverse" flips axis direction', async ({ ide, page }) => {
  // Y direction reversal: smallest data Y maps to TOP of the panel
  // (instead of bottom). Grab y-tick label positions before/after and
  // assert the order flips.
  await ide.runScript('import compat.*;\nplot([1 2 3], [10 20 30]);\n');
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(120);

  async function ytickPositions() {
    return await page.locator('.fw-window svg text[text-anchor="end"]').evaluateAll((els) =>
      els
        .filter((el) => /^-?\d/.test(el.textContent || ''))
        .map((el) => ({ y: parseFloat(el.getAttribute('y')), txt: el.textContent }))
        .sort((a, b) => a.y - b.y)
    );
  }
  const before = await ytickPositions();
  const topValBefore = parseFloat(before[0]?.txt || '0');
  const botValBefore = parseFloat(before[before.length - 1]?.txt || '0');
  expect(topValBefore).toBeGreaterThan(botValBefore);

  // Click the Y reverse cell (axis=Y, col=0).
  await openDisplay(page);
  await axisBtn(page, 'Y', 0).click();
  await page.waitForTimeout(150);
  expect(await isActive(axisBtn(page, 'Y', 0)),
    'Y reverse should be active after click').toBe(true);

  const after = await ytickPositions();
  const topValAfter = parseFloat(after[0]?.txt || '0');
  const botValAfter = parseFloat(after[after.length - 1]?.txt || '0');
  expect(topValAfter).toBeLessThan(botValAfter);
});
