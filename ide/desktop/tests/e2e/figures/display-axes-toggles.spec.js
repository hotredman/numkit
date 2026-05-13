// display-axes-toggles.spec.js — display ▾ "axes" section: axis,
// box, X reverse, Y reverse. Map to MATLAB Visible / Box / XDir /
// YDir.

import { test, expect } from '../../helpers/shared.js';

async function openDisplay(page) {
  await page.locator('.fw-toolbar .ve-btn', { hasText: /display/i }).click();
  await expect(page.locator('.fw-pop').first()).toBeVisible({ timeout: 2_000 });
}

test('display ▾ "axes" section has axis / box / X reverse / Y reverse', async ({ ide, page }) => {
  await ide.runScript('import compat.*;\nplot(1:10);\n');
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await openDisplay(page);

  for (const lbl of ['axis', 'box', 'X reverse', 'Y reverse']) {
    await expect(page.locator('.fw-pop-toggle',
      { has: page.locator('span', { hasText: lbl }) })).toBeVisible();
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
  await page.locator('.fw-pop-toggle',
    { has: page.locator('span', { hasText: 'Y reverse' }) }).click();
  await page.waitForTimeout(150);

  const after = await ytickPositions();
  const topValAfter = parseFloat(after[0]?.txt || '0');
  const botValAfter = parseFloat(after[after.length - 1]?.txt || '0');
  expect(topValAfter).toBeLessThan(botValAfter);
});
