// fit-axis-equal.spec.js — regression for the bug where clicking
// fit ▾ → X (or Y) on a figure with `axis equal` / `axis image`
// extended the viewport on one axis only, breaking the
// DataAspectRatio = [1 1 1] contract and visibly changing the
// rendered panel size. Fix: refit upgrades to 'both' for these cells
// so the original xlim/ylim relationship is preserved.

import { test, expect } from '../../helpers/shared.js';

async function openFit(page) {
  await page.locator('.fw-toolbar .ve-btn', { hasText: /fit/i }).click();
  await expect(page.locator('.fw-pop').first()).toBeVisible({ timeout: 2_000 });
}

async function svgBox(page, nth = 0) {
  return await page.locator('.fw-window .fw-canvas-wrap svg').nth(nth).boundingBox();
}

test('subplot 1×3 with axis equal: fit ▾ X keeps all three panels equal-sized', async ({ ide, page }) => {
  // Reproduces the qam_constellation pattern: three axis-equal panels
  // side by side with the same xlim/ylim. Before the fix, fit X
  // independently expanded each panel's X viewport to data extent,
  // and the axis-equal panel-shrink path in CompositePlot then
  // re-aspected each panel to match its new dx/dy ratio — leaving
  // three rectangles of different widths instead of three squares.
  await ide.runScript(
    'import compat.*;\n'
    + 'figure;\n'
    + 'rng(0);\n'
    + 'for k = 1:3\n'
    + '  subplot(1, 3, k);\n'
    + '  scatter(randn(200,1)*k, randn(200,1)*k);\n'
    + '  axis equal; xlim([-5 5]); ylim([-5 5]);\n'
    + 'end\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(200);

  // Snapshot each cell's rendered SVG size BEFORE fit X.
  const before = [];
  for (let i = 0; i < 3; i++) {
    const b = await svgBox(page, i);
    before.push({ w: b.width, h: b.height });
  }
  // Sanity: all three panels should be near-equal in width to start.
  expect(Math.abs(before[0].w - before[1].w),
    `pre-fit cells 0,1 differ: ${before.map((b) => b.w).join(',')}`).toBeLessThan(8);
  expect(Math.abs(before[1].w - before[2].w),
    `pre-fit cells 1,2 differ: ${before.map((b) => b.w).join(',')}`).toBeLessThan(8);

  // Click fit ▾ → X.
  await openFit(page);
  await page.locator('.fw-pop button', { hasText: /^X$/ }).first().click();
  await page.waitForTimeout(200);

  // After fit X with the fix: panels stay near-equal (because the
  // single-axis fit was upgraded to fit-both for axis-equal cells →
  // each cell snaps back to its scripted [-5, 5] × [-5, 5] viewport
  // and the panel-shrink keeps them square).
  const after = [];
  for (let i = 0; i < 3; i++) {
    const b = await svgBox(page, i);
    after.push({ w: b.width, h: b.height });
  }
  expect(Math.abs(after[0].w - after[1].w),
    `post-fit cells 0,1 differ: ${after.map((b) => b.w).join(',')}`).toBeLessThan(8);
  expect(Math.abs(after[1].w - after[2].w),
    `post-fit cells 1,2 differ: ${after.map((b) => b.w).join(',')}`).toBeLessThan(8);
});

// Non-subplot path (FigureWindow.applyFit) uses the same upgrade
// rule — covered by inspection (single condition: figure.axisMode ===
// 'equal' || 'image'). Subplot test above exercises the path that
// originally regressed (qam_constellation is 1×3 subplot).
