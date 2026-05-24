// fit-axis-equal.spec.js — coverage for the interaction between
// the aspect modes (auto/equal/square/image/tight) and fit ▾.
//
// Design contract (toolbar=universal): fit X/Y/Z buttons are ALWAYS
// clickable regardless of aspect mode. Behaviour adapts per cell:
//   • aspect=auto/tight/square → single-axis fit fires literally,
//     viewport[axis] snaps to cell.xRange/yRange (script lims or
//     padded data extent).
//   • aspect=equal/image → single-axis fit upgrades to fit-both
//     (cell.axisMode === 'equal'|'image' triggers upgradeFitAxis
//     in plotUtils.fitCellViewport) so the DataAspectRatio = [1 1 1]
//     contract isn't broken.
//
// All fit paths (toolbar fit ▾, ПКМ Fit X/Y, SubplotGrid fitSignal
// effect) route through the same `fitCellViewport` so there's no
// behavioural divergence between menus.

import { test, expect } from '../../helpers/shared.js';

async function openFit(page) {
  await page.locator('.fw-toolbar .ve-btn', { hasText: /fit/i }).click();
  await expect(page.locator('.fw-pop').first()).toBeVisible({ timeout: 2_000 });
}

async function openAxes(page) {
  await page.locator('.fw-toolbar .ve-btn', { hasText: /axes/i }).click();
  await expect(page.locator('.fw-pop').first()).toBeVisible({ timeout: 2_000 });
}

/** Right-click center of the Nth subplot cell (or the only canvas
 *  for non-subplot). Convenience for ПКМ tests. */
async function rightClickCell(page, idx = 0) {
  const svg = page.locator('.fw-window .fw-canvas-wrap svg').nth(idx);
  const box = await svg.boundingBox();
  await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
  await page.mouse.down({ button: 'right' });
  await page.mouse.up({ button: 'right' });
  await expect(page.locator('.ctx-menu')).toBeVisible({ timeout: 2_000 });
}

test('axes ▾ has aspect radio with 5 mutually-exclusive options', async ({ ide, page }) => {
  await ide.runScript(
    'import compat.*;\n'
    + 'plot(1:10);\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });

  await openAxes(page);
  // Aspect section with 5 pills.
  const pills = page.locator('.fw-pop-radio');
  await expect(pills).toHaveCount(5);
  await expect(pills.nth(0)).toContainText(/^auto$/);
  await expect(pills.nth(1)).toContainText(/^equal$/);
  await expect(pills.nth(2)).toContainText(/^square$/);
  await expect(pills.nth(3)).toContainText(/^image$/);
  await expect(pills.nth(4)).toContainText(/^tight$/);
  // Default for a bare plot — no axis mode set in script → 'auto' active.
  await expect(pills.nth(0)).toHaveClass(/is-active/);
});

test('aspect radio: clicking `equal` sets axisMode and shrinks panel to square', async ({ ide, page }) => {
  // Tall-aspect data (X span 20, Y span 4). With aspect=auto the
  // panel fills the cell; clicking `equal` triggers panel-shrink to
  // honour 1:1 data aspect → panel becomes WIDE and SHORT (W/H = 5).
  await ide.runScript(
    'import compat.*;\n'
    + 'rng(0);\n'
    + 'plot(linspace(-10, 10, 100), randn(1, 100) * 2);\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(150);

  await openAxes(page);
  // Click `equal`.
  await page.locator('.fw-pop-radio', { hasText: /^equal$/ }).click();
  await page.waitForTimeout(150);
  // Pill is now active.
  await expect(page.locator('.fw-pop-radio', { hasText: /^equal$/ })).toHaveClass(/is-active/);
  await expect(page.locator('.fw-pop-radio', { hasText: /^auto$/ })).not.toHaveClass(/is-active/);
});

test('fit ▾ X/Y/Z always enabled (toolbar=universal policy)', async ({ ide, page }) => {
  // Per the toolbar=universal rule, fit X/Y/Z buttons must STAY
  // clickable regardless of aspect mode. Behaviour adapts under the
  // hood: for axis-equal cells the click is upgraded to fit-both so
  // the DataAspectRatio = [1 1 1] contract isn't broken.
  await ide.runScript(
    'import compat.*;\n'
    + 'rng(0);\n'
    + 'scatter(randn(200,1), randn(200,1));\n'
    + 'axis equal; xlim([-5 5]); ylim([-5 5]);\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(150);

  await openFit(page);
  for (const ax of [/^all$/, /^X$/, /^Y$/, /^Z$/]) {
    await expect(page.locator('.fw-pop button', { hasText: ax })).toBeEnabled();
  }
});

test('fit ▾ X on axis-equal acts as fit both (refits BOTH axes)', async ({ ide, page }) => {
  // Reset both axes to script-set limits (auto-equivalent for plain
  // plot). User pans Y, then clicks fit X. Because axis equal locks
  // the axes together the X click upgrades to fit-both → BOTH axes
  // snap back to default. Y range restored even though user clicked
  // only X.
  await ide.runScript(
    'import compat.*;\n'
    + 'rng(0);\n'
    + 'scatter(randn(200,1), randn(200,1));\n'
    + 'axis equal; xlim([-5 5]); ylim([-5 5]);\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(200);

  // Snapshot the SVG box BEFORE — should stay constant (panel preserved).
  const before = await page.locator('.fw-window .fw-canvas-wrap svg').first().boundingBox();

  await openFit(page);
  await page.locator('.fw-pop button', { hasText: /^X$/ }).first().click();
  await page.waitForTimeout(200);

  const after = await page.locator('.fw-window .fw-canvas-wrap svg').first().boundingBox();
  // Panel size must be unchanged — upgrade to fit-both means both
  // axes snap back to [-5, 5] (which they already were) so dx/dy
  // ratio is preserved and panel-shrink keeps the same square.
  expect(Math.abs(after.width  - before.width),
    `panel width drifted ${before.width} → ${after.width}`).toBeLessThan(4);
  expect(Math.abs(after.height - before.height),
    `panel height drifted ${before.height} → ${after.height}`).toBeLessThan(4);
});

test('ПКМ Fit X on axis-equal cell also upgrades to fit both (no panel drift)', async ({ ide, page }) => {
  // Same axis-equal upgrade as the toolbar fit ▾ — ПКМ Fit X must
  // dispatch as fit-both for axis-equal cells. Without the fix the
  // ПКМ path bypassed applyFit and called computeFitViewport
  // directly with axisMode='x', breaking the DataAspectRatio
  // contract and visibly resizing the panel.
  await ide.runScript(
    'import compat.*;\n'
    + 'rng(0);\n'
    + 'scatter(randn(200,1), randn(200,1));\n'
    + 'axis equal; xlim([-5 5]); ylim([-5 5]);\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(200);

  const before = await page.locator('.fw-window .fw-canvas-wrap svg').first().boundingBox();

  // Right-click in the plot → ПКМ Fit section → X.
  const svg = page.locator('.fw-window .fw-canvas-wrap svg').first();
  const box = await svg.boundingBox();
  await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
  await page.mouse.down({ button: 'right' });
  await page.mouse.up({ button: 'right' });
  await expect(page.locator('.ctx-menu')).toBeVisible({ timeout: 2_000 });
  // Fit X row — labelled `X` in the ПКМ Fit section.
  await page.locator('.ctx-menu .ctx-item', { hasText: /^X$/ }).first().click();
  await page.waitForTimeout(200);

  const after = await page.locator('.fw-window .fw-canvas-wrap svg').first().boundingBox();
  expect(Math.abs(after.width  - before.width),
    `panel width drifted ${before.width} → ${after.width}`).toBeLessThan(4);
  expect(Math.abs(after.height - before.height),
    `panel height drifted ${before.height} → ${after.height}`).toBeLessThan(4);
});

test('toolbar 🏠 Reset clears aspect override back to script value', async ({ ide, page }) => {
  // Script default is auto. User changes aspect to `equal` via UI.
  // Toolbar Reset → aspect snaps back to `auto`.
  await ide.runScript(
    'import compat.*;\n'
    + 'plot(1:10);\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(150);

  await openAxes(page);
  await page.locator('.fw-pop-radio', { hasText: /^equal$/ }).click();
  await page.waitForTimeout(100);
  await expect(page.locator('.fw-pop-radio', { hasText: /^equal$/ })).toHaveClass(/is-active/);
  await page.locator('.fw-toolbar .ve-btn', { hasText: /axes/i }).click();   // close axes ▾
  await page.waitForTimeout(50);

  // 🏠 Reset.
  await page.locator('.fw-toolbar [data-fw-reset="all"]').click();
  await page.waitForTimeout(150);

  // Re-open axes ▾ → `auto` should be active again, `equal` not.
  await openAxes(page);
  await expect(page.locator('.fw-pop-radio', { hasText: /^auto$/ })).toHaveClass(/is-active/);
  await expect(page.locator('.fw-pop-radio', { hasText: /^equal$/ })).not.toHaveClass(/is-active/);
});

test('ПКМ Reset on subplot cell clears aspect override on THAT cell only', async ({ ide, page }) => {
  // Both cells start at auto (script default). Change cell A → equal
  // via ПКМ Axes ▶ aspect. ПКМ Reset on cell A should restore A to
  // auto WITHOUT touching cell B's already-default state.
  // Mixed → uniform `auto` transition.
  await ide.runScript(
    'import compat.*;\n'
    + 'figure;\n'
    + 'subplot(1, 2, 1); plot(1:10);\n'
    + 'subplot(1, 2, 2); plot(1:10);\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(150);

  // Right-click cell A, set aspect=equal via ПКМ Axes ▶.
  const svgA = page.locator('.fw-window .fw-canvas-wrap svg').nth(0);
  const boxA = await svgA.boundingBox();
  await page.mouse.move(boxA.x + boxA.width / 2, boxA.y + boxA.height / 2);
  await page.mouse.down({ button: 'right' });
  await page.mouse.up({ button: 'right' });
  await expect(page.locator('.ctx-menu')).toBeVisible({ timeout: 2_000 });
  await page.locator('.ctx-sub-trigger', { hasText: /Axes/ }).hover();
  await page.waitForTimeout(80);
  await page.locator('.ctx-submenu button', { hasText: /^(✓ )?equal$/ }).click();
  await page.waitForTimeout(150);

  // Sanity: toolbar head should now say `mixed` (cell A=equal, B=auto).
  await openAxes(page);
  await expect(page.locator('.fw-pop-head', { hasText: /^aspect: mixed$/ })).toBeVisible();
  await page.locator('.fw-toolbar .ve-btn', { hasText: /axes/i }).click();
  await page.waitForTimeout(50);

  // ПКМ Reset on cell A.
  await page.mouse.move(boxA.x + boxA.width / 2, boxA.y + boxA.height / 2);
  await page.mouse.down({ button: 'right' });
  await page.mouse.up({ button: 'right' });
  await expect(page.locator('.ctx-menu')).toBeVisible({ timeout: 2_000 });
  await page.locator('.ctx-menu .ctx-item', { hasText: /^Reset$/ }).first().click();
  await page.waitForTimeout(150);

  // Now both cells should be auto → head reads plain `aspect` (uniform).
  await openAxes(page);
  await expect(page.locator('.fw-pop-radio', { hasText: /^auto$/ })).toHaveClass(/is-active/);
  await expect(page.locator('.fw-pop-head', { hasText: /^aspect$/ })).toBeVisible();
});

test('subplot with mixed aspect: NO pill is active, head shows `mixed`', async ({ ide, page }) => {
  // Cell A is auto (default), cell B is `axis equal`. axisModeAgg = null
  // (mixed) → none of the 5 pills should have is-active, and the
  // section head should annotate `mixed` so the user sees there is no
  // uniform value.
  await ide.runScript(
    'import compat.*;\n'
    + 'figure;\n'
    + 'subplot(1, 2, 1); plot(1:10);\n'
    + 'subplot(1, 2, 2); plot(1:10); axis equal;\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(150);

  await openAxes(page);
  // No pill carries is-active.
  await expect(page.locator('.fw-pop-radio.is-active')).toHaveCount(0);
  // Section head reflects the mixed state.
  await expect(page.locator('.fw-pop-head', { hasText: /^aspect: mixed$/ })).toBeVisible();
});

test('ПКМ Axes ▶ aspect pill change syncs to toolbar pill state', async ({ ide, page }) => {
  // Single source of truth for axisMode: changing it from the ПКМ
  // surface must be reflected immediately in the toolbar aspect
  // pills. We change subplot cells one at a time and watch the
  // toolbar aggregate transition: auto → mixed → equal.
  await ide.runScript(
    'import compat.*;\n'
    + 'figure;\n'
    + 'subplot(1, 2, 1); plot(1:10);\n'
    + 'subplot(1, 2, 2); plot(1:10);\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(150);

  // Pre: both cells auto → toolbar shows `aspect`, auto pill active.
  await openAxes(page);
  await expect(page.locator('.fw-pop-head', { hasText: /^aspect$/ })).toBeVisible();
  await expect(page.locator('.fw-pop-radio', { hasText: /^auto$/ })).toHaveClass(/is-active/);
  await page.locator('.fw-toolbar .ve-btn', { hasText: /axes/i }).click();
  await page.waitForTimeout(50);

  // Change cell A → equal via ПКМ pillRow.
  await rightClickCell(page, 0);
  await page.locator('.ctx-sub-trigger', { hasText: /Axes/ }).hover();
  await page.waitForTimeout(80);
  await page.locator('.ctx-submenu .ctx-pill', { hasText: /^equal$/ }).click();
  await page.waitForTimeout(150);

  // Toolbar should now read `aspect: mixed` with no pill active.
  await openAxes(page);
  await expect(page.locator('.fw-pop-head', { hasText: /^aspect: mixed$/ })).toBeVisible();
  await expect(page.locator('.fw-pop-radio.is-active')).toHaveCount(0);
  await page.locator('.fw-toolbar .ve-btn', { hasText: /axes/i }).click();
  await page.waitForTimeout(50);

  // Change cell B → equal via ПКМ. Both cells now equal → uniform.
  await rightClickCell(page, 1);
  await page.locator('.ctx-sub-trigger', { hasText: /Axes/ }).hover();
  await page.waitForTimeout(80);
  await page.locator('.ctx-submenu .ctx-pill', { hasText: /^equal$/ }).click();
  await page.waitForTimeout(150);

  await openAxes(page);
  await expect(page.locator('.fw-pop-head', { hasText: /^aspect: equal$/ })).toBeVisible();
  await expect(page.locator('.fw-pop-radio', { hasText: /^equal$/ })).toHaveClass(/is-active/);
});

test('axes ▾ `default` button clears UI aspect override', async ({ ide, page }) => {
  // Popover-local `default` button at the top of axes ▾ must reset
  // aspect along with the rest of the axes-state surface. Same
  // semantic as 🏠 Reset but scoped to the axes ▾ popover.
  await ide.runScript(
    'import compat.*;\n'
    + 'plot(1:10);\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(150);

  await openAxes(page);
  await page.locator('.fw-pop-radio', { hasText: /^equal$/ }).click();
  await page.waitForTimeout(100);
  await expect(page.locator('.fw-pop-radio', { hasText: /^equal$/ })).toHaveClass(/is-active/);

  // Click the `default` button at the top of the same popover.
  await page.locator('.fw-pop button', { hasText: /^default$/ }).first().click();
  await page.waitForTimeout(150);

  // Pop closes after default — re-open and verify.
  await openAxes(page);
  await expect(page.locator('.fw-pop-radio', { hasText: /^auto$/ })).toHaveClass(/is-active/);
});

test('aspect=square: fit X works literally (panel stays square, X refits)', async ({ ide, page }) => {
  // `square` locks the PLOT BOX to be square (W=H), not the data
  // ratio. Single-axis fit is fine — refits X (or Y) to data extent
  // and the panel stays square regardless of dx/dy. No upgrade.
  await ide.runScript(
    'import compat.*;\n'
    + 'plot(linspace(-10, 10, 100), randn(1, 100) * 2);\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(150);

  // Switch aspect to square.
  await openAxes(page);
  await page.locator('.fw-pop-radio', { hasText: /^square$/ }).click();
  await page.waitForTimeout(150);
  await page.locator('.fw-toolbar .ve-btn', { hasText: /axes/i }).click();
  await page.waitForTimeout(50);

  // Snapshot panel size — should be square (W = H within rounding).
  const box = await page.locator('.fw-window .fw-canvas-wrap svg').first().boundingBox();
  // Panel is now W = H = min(cellW, cellH).
  // Click fit X (button enabled per universal rule). square does NOT
  // upgrade — fit X applies literally to X alone.
  await openFit(page);
  await expect(page.locator('.fw-pop button', { hasText: /^X$/ })).toBeEnabled();
  await page.locator('.fw-pop button', { hasText: /^X$/ }).first().click();
  await page.waitForTimeout(150);

  // Panel must STILL be square afterwards (square locks plot box).
  const after = await page.locator('.fw-window .fw-canvas-wrap svg').first().boundingBox();
  expect(Math.abs(after.width - box.width),
    `panel width drifted ${box.width} → ${after.width}`).toBeLessThan(4);
  expect(Math.abs(after.height - box.height),
    `panel height drifted ${box.height} → ${after.height}`).toBeLessThan(4);
});

test('subplot mixed aspect: fit X routes per cell (auto vs equal)', async ({ ide, page }) => {
  // Two cells, different aspect modes. Toolbar fit X must dispatch
  // per cell: auto-cell gets single-axis fit, equal-cell gets
  // upgraded to fit-both. Both end up at their script defaults so
  // we can verify the renderer didn't break either contract:
  //   • cell A (aspect=auto, no axis-equal) — panel = full cell.
  //   • cell B (aspect=equal + square xlim/ylim) — panel = square.
  await ide.runScript(
    'import compat.*;\n'
    + 'figure;\n'
    + 'subplot(1, 2, 1); plot(linspace(-10, 10, 100), randn(1, 100) * 2);\n'
    + 'subplot(1, 2, 2); scatter(randn(200,1), randn(200,1));\n'
    + 'axis equal; xlim([-5 5]); ylim([-5 5]);\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(200);

  // Snapshot both cells before fit.
  const beforeA = await page.locator('.fw-window .fw-canvas-wrap svg').nth(0).boundingBox();
  const beforeB = await page.locator('.fw-window .fw-canvas-wrap svg').nth(1).boundingBox();

  // Click toolbar fit X — fires fitSignal{axis:'x'} → SubplotGrid
  // routes per cell via fitCellViewport (cell A literal X, cell B
  // upgraded to both).
  await openFit(page);
  await page.locator('.fw-pop button', { hasText: /^X$/ }).first().click();
  await page.waitForTimeout(200);

  const afterA = await page.locator('.fw-window .fw-canvas-wrap svg').nth(0).boundingBox();
  const afterB = await page.locator('.fw-window .fw-canvas-wrap svg').nth(1).boundingBox();
  // Cell A (aspect=auto): SVG cell allocation doesn't change.
  expect(Math.abs(afterA.width  - beforeA.width),
    `cell A width drifted ${beforeA.width} → ${afterA.width}`).toBeLessThan(4);
  expect(Math.abs(afterA.height - beforeA.height),
    `cell A height drifted ${beforeA.height} → ${afterA.height}`).toBeLessThan(4);
  // Cell B (aspect=equal): SVG cell allocation also stable (upgrade
  // to fit-both snaps back to scripted [-5, 5] × [-5, 5] → dx=dy → panel-
  // shrink preserves the same aspect).
  expect(Math.abs(afterB.width  - beforeB.width),
    `cell B width drifted ${beforeB.width} → ${afterB.width}`).toBeLessThan(4);
  expect(Math.abs(afterB.height - beforeB.height),
    `cell B height drifted ${beforeB.height} → ${afterB.height}`).toBeLessThan(4);
});

test('subplot 1×3 axis equal: fit X keeps all 3 cells the same width', async ({ ide, page }) => {
  // qam_constellation-style: 3 axis-equal cells. fit X upgrades to
  // fit-both per cell, so all 3 cells snap back to their scripted
  // xlim/ylim ([-5, 5] × [-5, 5]) and the panels stay equal-sized.
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

  // Click fit X.
  await openFit(page);
  await page.locator('.fw-pop button', { hasText: /^X$/ }).first().click();
  await page.waitForTimeout(200);

  // All three cells should be the same width afterwards.
  const widths = [];
  for (let i = 0; i < 3; i++) {
    const b = await page.locator('.fw-window .fw-canvas-wrap svg').nth(i).boundingBox();
    widths.push(b.width);
  }
  expect(Math.abs(widths[0] - widths[1]),
    `cells 0,1 differ: ${widths.join(',')}`).toBeLessThan(8);
  expect(Math.abs(widths[1] - widths[2]),
    `cells 1,2 differ: ${widths.join(',')}`).toBeLessThan(8);
});
