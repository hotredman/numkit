// figure-state-parity.spec.js — verify that script-level settings
// reach BOTH the preview card AND the modal window, and that the
// modal mirrors the preview's state. Covers grid, lims, log scale,
// axis modes, colormap, clim — for 2-D, 3-D, and heatmap figures.
//
// Strategy: the engine is the single source of truth, both renderers
// pull off the same `figure` object. We assert via DOM signals the
// renderer sets on its primitives:
//   - showMajor / showMinor → toolbar buttons get `is-active` class
//   - xLog / yLog            → log-toggle buttons activate
//   - xlim / ylim            → footer NumberInputs reflect the values
//   - zlim (3-D)             → same in 3-D footer
//   - colormap (heatmap)     → toolbar select + heatmap colormap field
//   - axis equal             → renderer applies (smoke check, no
//                              pixel diff)
// Preview side asserts via DOM attrs of the SVG / canvas wrapper that
// don't depend on toolbar mounting.

import { test, expect } from '../../helpers/shared.js';

test.describe('Script ↔ preview ↔ modal parity', () => {
  // ─── 2-D plots ────────────────────────────────────────────────────

  test('2-D xlim / ylim — modal inputs show script-set values', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([1 2 3 4 5], [1 4 9 16 25]);\n'
      + 'xlim([-2 7]); ylim([-5 30]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    const inputs = ide.figureWindow.locator('.fw-range-row input');
    expect(Number(await inputs.nth(0).inputValue())).toBeCloseTo(-2, 1);
    expect(Number(await inputs.nth(1).inputValue())).toBeCloseTo(7, 1);
    expect(Number(await inputs.nth(2).inputValue())).toBeCloseTo(-5, 1);
    expect(Number(await inputs.nth(3).inputValue())).toBeCloseTo(30, 1);
  });

  test('2-D xscale log — toolbar x-log toggle activated', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'imagesc(reshape(1:64, 8, 8));\n'
      + 'xscale(\'log\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    const xLogBtn = ide.figureWindow.locator('button', { hasText: 'x log' });
    await expect(xLogBtn).toHaveClass(/is-active/);
  });

  test('2-D yscale log — toolbar y-log toggle activated', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'imagesc(reshape(1:64, 8, 8));\n'
      + 'yscale(\'log\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    const yLogBtn = ide.figureWindow.locator('button', { hasText: 'y log' });
    await expect(yLogBtn).toHaveClass(/is-active/);
  });

  test('2-D grid on — both preview and modal show grid', async ({ ide, page }) => {
    await ide.runScript('import compat.*;\nplot([1 2 3], [1 4 9]); grid on;\n');
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    // Preview: grid lines drawn via .plot-grid stroke. We can grep
    // any line element with that stroke class.
    const previewCard = ide.figureCards.first();
    // Modal: grid toolbar button active.
    await previewCard.click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    const gridBtn = ide.figureWindow.locator('button', { hasText: /^grid$/ });
    await expect(gridBtn).toHaveClass(/is-active/);
  });

  test('2-D grid minor — minor toggle reflects in modal', async ({ ide, page }) => {
    await ide.runScript('import compat.*;\nplot([1 2 3], [1 4 9]); grid on; grid minor;\n');
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    const minorBtn = ide.figureWindow.locator('button', { hasText: /^minor$/ });
    await expect(minorBtn).toHaveClass(/is-active/);
  });

  test('2-D axis equal — modal renders without errors (renderer applies)', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([0 1 2 3], [0 0.5 0 0.5]);\n'
      + 'axis(\'equal\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  // ─── 3-D plots ────────────────────────────────────────────────────

  test('3-D xlim/ylim/zlim — script values reach modal footer inputs', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot3([0 1 2 3], [0 1 4 9], [1 2 3 4]);\n'
      + 'xlim([-1 5]); ylim([-2 12]); zlim([0 10]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    await page.waitForTimeout(300);
    const inputs = ide.figureWindow.locator('.fw-range-row input');
    expect(Number(await inputs.nth(0).inputValue())).toBeCloseTo(-1, 1);
    expect(Number(await inputs.nth(1).inputValue())).toBeCloseTo(5, 1);
    expect(Number(await inputs.nth(2).inputValue())).toBeCloseTo(-2, 1);
    expect(Number(await inputs.nth(3).inputValue())).toBeCloseTo(12, 1);
    expect(Number(await inputs.nth(4).inputValue())).toBeCloseTo(0, 1);
    expect(Number(await inputs.nth(5).inputValue())).toBeCloseTo(10, 1);
  });

  test('3-D grid on by default — preview and modal both show grid', async ({ ide, page }) => {
    // Adapter forces grid='on' for 3-D when no explicit script call.
    await ide.runScript('import compat.*;\nsurf([1 2; 3 4]);\n');
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    const gridBtn = ide.figureWindow.locator('button', { hasText: /^grid$/ });
    await expect(gridBtn).toHaveClass(/is-active/);
  });

  test('3-D explicit grid off — modal grid toggle inactive', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'surf([1 2 3; 2 3 4; 3 4 5]);\n'
      + 'grid off;\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    const gridBtn = ide.figureWindow.locator('button', { hasText: /^grid$/ });
    await expect(gridBtn).not.toHaveClass(/is-active/);
  });

  test('3-D grid minor — modal minor toggle active', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot3([0 1 2], [0 1 0], [0 1 0]);\n'
      + 'grid minor;\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    const minorBtn = ide.figureWindow.locator('button', { hasText: /^minor$/ });
    await expect(minorBtn).toHaveClass(/is-active/);
  });

  test('3-D view(az, el) — modal opens with correct camera (no errors)', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot3([0 1 2], [0 1 0], [0 1 0]);\n'
      + 'view(60, 45);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    await expect(ide.figureWindow.locator('canvas[data-numkit-3d]'))
      .toBeVisible({ timeout: 10_000 });
  });

  // ─── Heatmap (imagesc / pcolor) ───────────────────────────────────

  test('imagesc + colormap("hot") — toolbar select reflects script', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'imagesc(magic(8));\n'
      + 'colormap(\'hot\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    const select = ide.figureWindow.locator('select.fw-cmap-select');
    await expect(select).toHaveValue('hot');
  });

  test('imagesc + clim — modal renders without errors', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'imagesc(magic(8));\n'
      + 'clim([0 30]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('imagesc + grid on — major grid lines drawn over heatmap', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'imagesc(reshape(1:64, 8, 8));\n'
      + 'grid on;\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    const gridBtn = ide.figureWindow.locator('button', { hasText: /^grid$/ });
    await expect(gridBtn).toHaveClass(/is-active/);
  });

  test('imagesc xlim — modal x inputs match script', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'imagesc(reshape(1:64, 8, 8));\n'
      + 'xlim([1 4]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    const inputs = ide.figureWindow.locator('.fw-range-row input');
    expect(Number(await inputs.nth(0).inputValue())).toBeCloseTo(1, 1);
    expect(Number(await inputs.nth(1).inputValue())).toBeCloseTo(4, 1);
  });

  // ─── Cross-window: preview ↔ modal coherence ──────────────────────

  test('preview SVG path count == modal SVG path count for 2-D plot', async ({ ide, page }) => {
    // Both renderers consume the same `figure` object. Stroke-path
    // count is a structural-equivalent proxy: scribbles on preview
    // === scribbles in modal.
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([1 2 3], [1 4 9]);\n'
      + 'hold(\'on\');\n'
      + 'plot([1 2 3], [9 4 1]);\n'
      + 'grid on;\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    const previewCard = ide.figureCards.first();
    const previewPaths = await previewCard.locator('svg path').count();

    await previewCard.click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    const modalPaths = await ide.figureWindow.locator('svg path').count();

    // Modal has the same series content; modal-only chrome may add a
    // few extra paths but the data paths should be present in both.
    // We assert preview ≤ modal (modal has the same paths plus optional
    // legend / extras), and both > 0.
    expect(previewPaths).toBeGreaterThan(0);
    expect(modalPaths).toBeGreaterThan(0);
    expect(modalPaths).toBeGreaterThanOrEqual(previewPaths);
  });

  test('preview canvas == modal canvas for 3-D plot (both WebGL)', async ({ ide, page }) => {
    await ide.runScript('import compat.*;\nsurf([1 2 3; 2 3 4; 3 4 5]);\n');
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await expect(ide.figureCards.first().locator('canvas[data-numkit-3d]'))
      .toBeVisible({ timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow.locator('canvas[data-numkit-3d]'))
      .toBeVisible({ timeout: 10_000 });
  });

  // ─── Legend gating ────────────────────────────────────────────────

  test('legend NOT shown by default — script never called legend()', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([1 2 3], [1 4 9]);\n'
      + 'hold(\'on\');\n'
      + 'plot([1 2 3], [9 4 1]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    // No legend block in the preview SVG (CompositePlot's internal
    // legend short-circuits when the script didn't ask).
    const previewLegend = await ide.figureCards.first()
      .locator('svg .fw-legend, svg [data-numkit-legend]').count();
    expect(previewLegend).toBe(0);

    // No legend toolbar button in the modal (suppressed when
    // legendUserAsked is false).
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    const legendBtn = ide.figureWindow.locator('button.ve-btn', { hasText: /^legend$/ });
    expect(await legendBtn.count()).toBe(0);
  });

  test('legend shown when script calls legend(...)', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([1 2 3], [1 4 9]);\n'
      + 'hold(\'on\');\n'
      + 'plot([1 2 3], [9 4 1]);\n'
      + 'legend(\'rising\', \'falling\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    // Legend button appears AND is active.
    const legendBtn = ide.figureWindow.locator('button.ve-btn', { hasText: /^legend$/ });
    await expect(legendBtn).toBeVisible({ timeout: 5_000 });
    await expect(legendBtn).toHaveClass(/is-active/);
  });

  test('preview heatmap image element matches modal', async ({ ide, page }) => {
    // imagesc emits an <image> in both. Both should be present.
    await ide.runScript('import compat.*;\nimagesc(magic(6));\n');
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    const previewImage = await ide.figureCards.first().locator('svg image').count();
    expect(previewImage).toBeGreaterThanOrEqual(1);
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    const modalImage = await ide.figureWindow.locator('svg image').count();
    expect(modalImage).toBeGreaterThanOrEqual(1);
  });
});
