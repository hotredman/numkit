// display-axes-toggles.spec.js — axes ▾ button: `visible: axis/box`,
// `reverse: X/Y/Z`, `log scale: X/Y/Z`. Maps to MATLAB Visible / Box /
// XDir·YDir·ZDir / XScale·YScale·ZScale.
//
// Section heads name the ACTIVE state (`reverse`, `log scale`) so
// per-axis rows are single letters; same compact pattern as grid ▾.
// Tests use a section-scoped selector because `X` / `Y` / `Z` appear
// in BOTH `reverse` and `log scale` sections.

import { test, expect } from '../../helpers/shared.js';

async function openDisplay(page) {
  await page.locator('.fw-toolbar .ve-btn', { hasText: /axes/i }).click();
  await expect(page.locator('.fw-pop').first()).toBeVisible({ timeout: 2_000 });
}

/** Locate a `.fw-pop-toggle` row by its (section head, row text) pair.
 *  Both match exactly via anchored regex — necessary because single-
 *  letter rows like `X` appear in multiple sections. */
function row(page, sectionHead, rowText) {
  return page.locator('.fw-pop-section', {
    has: page.locator('.fw-pop-head', { hasText: new RegExp(`^${sectionHead}$`) }),
  }).locator('.fw-pop-toggle', {
    has: page.locator('span', { hasText: new RegExp(`^${rowText}$`) }),
  });
}

test('axes ▾ has visible/box + reverse{X,Y,Z} + log scale{X,Y,Z} rows', async ({ ide, page }) => {
  await ide.runScript('import compat.*;\nplot(1:10);\n');
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await openDisplay(page);

  await expect(row(page, 'visible', 'axis')).toBeVisible();
  await expect(row(page, 'visible', 'box')).toBeVisible();
  for (const ax of ['X', 'Y', 'Z']) {
    await expect(row(page, 'reverse',   ax)).toBeVisible();
    await expect(row(page, 'log scale', ax)).toBeVisible();
  }
});

test('toggle "axis" hides x/y tick lines + box', async ({ ide, page }) => {
  await ide.runScript('import compat.*;\nplot(1:10);\n');
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });

  // Pre: axis ticks present (text labels for ticks).
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
  // Default: top-most y label is the LARGEST (axis xy normal — y up).
  // Smallest y attribute (top of SVG) corresponds to largest data value.
  const topValBefore = parseFloat(before[0]?.txt || '0');
  const botValBefore = parseFloat(before[before.length - 1]?.txt || '0');
  expect(topValBefore).toBeGreaterThan(botValBefore);

  await openDisplay(page);
  await row(page, 'reverse', 'Y').click();
  await page.waitForTimeout(150);

  const after = await ytickPositions();
  const topValAfter = parseFloat(after[0]?.txt || '0');
  const botValAfter = parseFloat(after[after.length - 1]?.txt || '0');
  expect(topValAfter).toBeLessThan(botValAfter);
});
