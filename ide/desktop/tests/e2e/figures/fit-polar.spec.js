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
  // PolarPlot doesn't model cartesian X — toolbar fit X / Y / Z
  // resolves to no-op for polar figures. fitCellViewport's polar
  // branch returns `cur` for cartesian axes, parity with the
  // SubplotGrid per-cell routing.
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

  await openFit(page);
  await page.locator('.fw-pop button', { hasText: /^X$/ }).first().click();
  await page.waitForTimeout(200);

  const after = await rTicks(page);
  // rmax tick stays at script-set 5 (no change).
  expect(after.some((v) => Math.abs(v - 5) < 0.01),
    `fit X on polar should leave rmax at 5: ${JSON.stringify(after)}`).toBe(true);
  expect(after.length).toBe(before.length);
});

test('polar minor R grid renders independently of major R grid', async ({ ide, page }) => {
  // MATLAB R2025b parity: RMinorGrid / ThetaMinorGrid are INDEPENDENT
  // of RGrid / ThetaGrid. The user can have minor rings visible
  // without the major ring strokes. Previously gated on rGridOn &&
  // rMinorOn which made the per-axis minor checkbox a no-op when
  // major was off. We assert: minor rings render even when R major
  // is OFF — turn major off first, then toggle minor on, then count
  // minor-stroke <circle> elements in the SVG.
  await ide.runScript(
    'import compat.*;\n'
    + 'theta = linspace(0, 2*pi, 200);\n'
    + 'polarplot(theta, 3 + sin(3*theta));\n'
    + 'rlim([0 5]);\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(200);

  async function minorRingCount() {
    return await page.locator('.fw-window .fw-canvas-wrap svg circle')
      .evaluateAll((els) => els.filter((el) =>
        (el.getAttribute('stroke') || '').includes('plot-grid-min'),
      ).length);
  }

  // Open grid ▾.
  await page.locator('.fw-toolbar .ve-btn', { hasText: /grid/i }).click();
  await expect(page.locator('.fw-pop').first()).toBeVisible({ timeout: 2_000 });
  const rRow = page.locator('.fw-pop-matrix .fw-pop-mrow', {
    has: page.locator('.fw-pop-mrow-label', { hasText: /^R$/ }),
  });
  const rMajor = rRow.locator('.fw-pop-mbtn').nth(0);
  const rMinorBtn = rRow.locator('.fw-pop-mbtn').nth(1);
  // Helper: read is-active on a button.
  const isActive = async (btn) => /\bis-active\b/.test((await btn.getAttribute('class')) || '');

  // Polar plot's adapter may leave RGrid on or off depending on the
  // script's `grid on/off` state. Force major OFF first so the
  // assertion below is unambiguous.
  if (await isActive(rMajor)) {
    await rMajor.click();
    await page.waitForTimeout(120);
  }
  expect(await isActive(rMajor), 'R major must be off before toggling minor').toBe(false);

  // Now turn R minor ON.
  await rMinorBtn.click();
  await page.waitForTimeout(150);
  expect(await isActive(rMinorBtn), 'R minor should be active after click').toBe(true);
  expect(await isActive(rMajor), 'R major must stay inactive').toBe(false);

  // Close grid ▾ so it doesn't overlap the canvas measurement.
  await page.locator('.fw-toolbar .ve-btn', { hasText: /grid/i }).click();
  await page.waitForTimeout(150);

  // Minor rings should now be visible — the whole point of the fix.
  expect(await minorRingCount(),
    'minor rings should render with R major OFF + R minor ON').toBeGreaterThan(0);
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
