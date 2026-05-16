// fit-polar.spec.js — coverage for polar fit dispatch (toolbar fit ▾
// → R / θ / all + ПКМ Fit R / θ / all). All routes funnel through
// plotUtils.fitCellViewport so the three menus give one result.
//
// defaultPolarViewport rules: r = script rlim if set, else
// [0, nicePolarMax(max(|rho|))]; theta = script thetalim if set,
// else [0, 360].

import { test, expect } from '../../helpers/shared.js';

async function openFit(page) {
  await page.locator('.fw-toolbar .ve-btn', { hasText: /fit/i }).click();
  await expect(page.locator('.fw-pop').first()).toBeVisible({ timeout: 2_000 });
}

async function rightClickCell(page, idx = 0) {
  const svg = page.locator('.fw-window .fw-canvas-wrap svg').nth(idx);
  const box = await svg.boundingBox();
  await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
  await page.mouse.down({ button: 'right' });
  await page.mouse.up({ button: 'right' });
  await expect(page.locator('.ctx-menu')).toBeVisible({ timeout: 2_000 });
}

/** Read the rendered radial-tick labels from the (only / nth) polar
 *  SVG. PolarPlot renders rmax as e.g. "5.00", labels float in the
 *  upper quadrant. */
async function rTicks(page, nth = 0) {
  return await page.locator('.fw-window .fw-canvas-wrap svg').nth(nth)
    .locator('text').evaluateAll((els) => els
      .map((el) => el.textContent || '')
      .filter((t) => /^\d/.test(t))
      .map((t) => parseFloat(t))
      .filter((v) => Number.isFinite(v))
    );
}

test('toolbar fit ▾ on polar exposes R / θ / all rows', async ({ ide, page }) => {
  await ide.runScript(
    'import compat.*;\n'
    + 'theta = linspace(0, 2*pi, 200);\n'
    + 'polarplot(theta, abs(sin(2*theta)));\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(150);

  await openFit(page);
  // R / θ live in the Polar section of the fit ▾ popover. Same
  // toolbar=universal rule — all rows always clickable.
  await expect(page.locator('.fw-pop button', { hasText: /^R$/ })).toBeEnabled();
  await expect(page.locator('.fw-pop button', { hasText: /^θ$/ })).toBeEnabled();
  await expect(page.locator('.fw-pop button', { hasText: /^all$/ })).toBeEnabled();
});

test('toolbar fit R on polar returns rmax to defaultPolarViewport (rlim if set)', async ({ ide, page }) => {
  // Script sets rlim([0 5]). After pan/zoom (or no perturbation) the
  // fit R must restore viewport.r to [0, 5] — the unified
  // fitCellViewport returns def.r = figure.rlim when rlim is set.
  await ide.runScript(
    'import compat.*;\n'
    + 'theta = linspace(0, 2*pi, 200);\n'
    + 'polarplot(theta, 3 + sin(3*theta));\n'
    + 'rlim([0 5]);\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(150);

  // Initial rmax tick should reach 5 (rlim).
  const before = await rTicks(page);
  expect(before.some((v) => Math.abs(v - 5) < 0.01),
    `pre-fit ticks should include rmax=5: ${JSON.stringify(before)}`).toBe(true);

  // Toolbar fit ▾ → R.
  await openFit(page);
  await page.locator('.fw-pop button', { hasText: /^R$/ }).first().click();
  await page.waitForTimeout(200);

  // After fit R: rmax still at script rlim 5 (no drift).
  const after = await rTicks(page);
  expect(after.some((v) => Math.abs(v - 5) < 0.01),
    `post-fit ticks should still include rmax=5: ${JSON.stringify(after)}`).toBe(true);
});

test('ПКМ Fit R on polar matches toolbar fit R (same fitCellViewport)', async ({ ide, page }) => {
  // Both surfaces route through the unified per-cell fit. Open ПКМ
  // and click Fit → R; assert the rendered rmax tick is the same as
  // the toolbar would give (= script rlim).
  await ide.runScript(
    'import compat.*;\n'
    + 'theta = linspace(0, 2*pi, 200);\n'
    + 'polarplot(theta, 3 + sin(3*theta));\n'
    + 'rlim([0 5]);\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(150);

  await rightClickCell(page);
  // ПКМ Fit section row labelled `R`.
  await page.locator('.ctx-menu .ctx-item', { hasText: /^R$/ }).first().click();
  await page.waitForTimeout(200);

  const after = await rTicks(page);
  expect(after.some((v) => Math.abs(v - 5) < 0.01),
    `post-fit ticks should include rmax=5: ${JSON.stringify(after)}`).toBe(true);
});

test('toolbar fit all on polar resets both R and θ', async ({ ide, page }) => {
  await ide.runScript(
    'import compat.*;\n'
    + 'theta = linspace(0, 2*pi, 200);\n'
    + 'polarplot(theta, 3 + sin(3*theta));\n'
    + 'rlim([0 5]);\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(150);

  await openFit(page);
  await page.locator('.fw-pop button', { hasText: /^all$/ }).first().click();
  await page.waitForTimeout(200);

  const after = await rTicks(page);
  expect(after.some((v) => Math.abs(v - 5) < 0.01),
    `fit-all should leave rmax at script rlim=5: ${JSON.stringify(after)}`).toBe(true);
});

test('toolbar fit X on polar is a no-op (cartesian axis ignored)', async ({ ide, page }) => {
  // PolarPlot doesn't model cartesian X — toolbar fit X / Y / Z should
  // resolve to no-op for polar figures. fitCellViewport's polar
  // branch returns `cur` for unknown axes.
  await ide.runScript(
    'import compat.*;\n'
    + 'theta = linspace(0, 2*pi, 200);\n'
    + 'polarplot(theta, 3 + sin(3*theta));\n'
    + 'rlim([0 5]);\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(150);

  const before = await rTicks(page);

  // Note: for non-subplot polar the toolbar applyFit path remaps
  // x/y/z → 'both' for safety. End result: rmax stays at rlim.
  await openFit(page);
  await page.locator('.fw-pop button', { hasText: /^X$/ }).first().click();
  await page.waitForTimeout(200);

  const after = await rTicks(page);
  // rmax tick stays at script-set 5.
  expect(after.some((v) => Math.abs(v - 5) < 0.01),
    `fit X on polar should leave rmax at 5: ${JSON.stringify(after)}`).toBe(true);
  expect(after.length).toBe(before.length);
});

test('subplot with polar cell: toolbar fit R applies via SubplotGrid', async ({ ide, page }) => {
  // Subplot fit ▾ → R fires fitSignal{axis:'r'} → SubplotGrid effect
  // → fitCellViewport per cell. Polar cells honour 'r', cartesian
  // cells no-op.
  await ide.runScript(
    'import compat.*;\n'
    + 'figure;\n'
    + 'subplot(1, 2, 1); plot(1:10);\n'
    + 'theta = linspace(0, 2*pi, 200);\n'
    + 'subplot(1, 2, 2); polarplot(theta, 3 + sin(3*theta)); rlim([0 5]);\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(200);

  // Polar cell is index 1.
  const polarBefore = await rTicks(page, 1);
  expect(polarBefore.some((v) => Math.abs(v - 5) < 0.01),
    `polar cell pre-fit ticks should include rmax=5: ${JSON.stringify(polarBefore)}`).toBe(true);

  await openFit(page);
  await page.locator('.fw-pop button', { hasText: /^R$/ }).first().click();
  await page.waitForTimeout(200);

  const polarAfter = await rTicks(page, 1);
  expect(polarAfter.some((v) => Math.abs(v - 5) < 0.01),
    `polar cell post-fit ticks should still include rmax=5: ${JSON.stringify(polarAfter)}`).toBe(true);
});
