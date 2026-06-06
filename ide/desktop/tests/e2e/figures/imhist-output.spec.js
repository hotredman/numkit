// imhist-output.spec.js — verify imhist auto-plots when called
// without LHS (MATLAB convention). Mirrors histogram_equalization.m
// which calls imhist(low) inside subplot cells expecting a histogram
// chart, not silence.

import { test, expect } from '../../helpers/shared.js';

const NON_FATAL = (e) =>
  !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e);

async function snapshot(page, name) {
  await page.screenshot({
    path: `test-results/_diag/${name}.png`,
    fullPage: false,
  });
}

test.describe('imhist auto-plot', () => {
  test('imhist(I) — no LHS draws a histogram bar chart', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + '[X, Y] = meshgrid(linspace(0, 1, 64));\n'
      + 'I = 0.3 + 0.25 * (0.5 * X + 0.5 * Y);\n'
      + 'imhist(I);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    await page.waitForTimeout(300);
    await snapshot(page, 'imhist');
    // Bar chart renders as <rect> elements in the SVG (one per bin).
    const rects = await page.locator('.fw-window .fw-canvas-wrap rect').count();
    expect(rects, 'imhist drew no <rect> bars').toBeGreaterThan(10);
    expect(ide.devErrors().filter(NON_FATAL)).toEqual([]);
  });

  test('subplot grid with imhist in cells 3 and 4', async ({ ide, page }) => {
    // Mirrors examples/Image/histogram_equalization.m shape.
    await ide.runScript(
      'import compat.*;\n'
      + '[X, Y] = meshgrid(linspace(0, 1, 64));\n'
      + 'low = 0.3 + 0.25 * (0.5 * X + 0.5 * Y);\n'
      + 'eq = histeq(low);\n'
      + 'figure;\n'
      + 'subplot(2,2,1); imshow(low);\n'
      + 'subplot(2,2,2); imshow(eq);\n'
      + 'subplot(2,2,3); imhist(low);\n'
      + 'subplot(2,2,4); imhist(eq);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    await page.waitForTimeout(400);

    // 4 SVG cells filled. Empty-slot placeholders should be ZERO.
    expect(await page.locator('.fw-window .fw-canvas-wrap svg').count()).toBe(4);
    expect(await page.locator('.fw-window .sg-empty-slot').count()).toBe(0);
    expect(ide.devErrors().filter(NON_FATAL)).toEqual([]);
  });
});
