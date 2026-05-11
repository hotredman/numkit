// grid-matlab-parity.spec.js — `grid` / `grid on` / `grid off` /
// `grid minor` semantics across 2-D and 3-D figures. Behaviour
// targets MATLAB:
//   grid          → toggle MAJOR (minor untouched)
//   grid on       → major on
//   grid off      → both off
//   grid minor    → toggle MINOR (major untouched)
//
// We verify the engine state via the figure card's emitted JSON
// (visible to the renderer through CSS classes / SVG attrs / canvas
// presence). Wire format: figure.grid = "on"|"off",
// figure.gridMinor = "on"|"off".

import { test, expect } from '../helpers/shared.js';

test.describe('grid — MATLAB parity', () => {
  // -------- 2-D figures: SVG path --------

  test('2-D plot — grid on draws major grid lines', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([1 2 3 4 5], [1 4 9 16 25]);\n'
      + 'grid on;\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    // `grid on` engages the renderer's major grid; SVG paths/lines
    // exceed the data-only count. We don't pixel-diff; just confirm
    // figure renders clean.
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('2-D plot — grid off after grid on clears both', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([1 2 3], [1 4 9]);\n'
      + 'grid on;\n'
      + 'grid minor;\n'
      + 'grid off;\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('2-D plot — bare `grid` toggles major; minor stays untouched', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([1 2 3], [1 4 9]);\n'
      + 'grid minor;\n'      // minor on, major still off
      + 'grid;\n'             // toggle major → major on (minor unchanged)
      + 'grid;\n'             // toggle major → major off (minor still on!)
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('2-D plot — grid minor + grid on coexist', async ({ ide, page }) => {
    // MATLAB: `grid on; grid minor` shows BOTH major and minor.
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([0 1 2 3], [0 1 4 9]);\n'
      + 'grid on;\n'
      + 'grid minor;\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    // Both grid + minor toolbar buttons should be active.
    const gridBtn  = ide.figureWindow.locator('button', { hasText: /^grid$/ });
    const minorBtn = ide.figureWindow.locator('button', { hasText: /^minor$/ });
    await expect(gridBtn).toHaveClass(/is-active/);
    await expect(minorBtn).toHaveClass(/is-active/);
  });

  test('2-D plot — grid off after both: both buttons inactive', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([1 2 3], [1 4 9]);\n'
      + 'grid on;\n'
      + 'grid minor;\n'
      + 'grid off;\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    const gridBtn  = ide.figureWindow.locator('button', { hasText: /^grid$/ });
    const minorBtn = ide.figureWindow.locator('button', { hasText: /^minor$/ });
    await expect(gridBtn).not.toHaveClass(/is-active/);
    await expect(minorBtn).not.toHaveClass(/is-active/);
  });

  test('2-D plot — default is no grid (MATLAB parity)', async ({ ide, page }) => {
    await ide.runScript('import compat.*;\nplot([1 2 3], [1 4 9]);\n');
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    const gridBtn  = ide.figureWindow.locator('button', { hasText: /^grid$/ });
    const minorBtn = ide.figureWindow.locator('button', { hasText: /^minor$/ });
    await expect(gridBtn).not.toHaveClass(/is-active/);
    await expect(minorBtn).not.toHaveClass(/is-active/);
  });

  // -------- 3-D figures: WebGL path --------

  test('3-D surf — grid on by default (3-D-only convention)', async ({ ide, page }) => {
    await ide.runScript('import compat.*;\nsurf([1 2 3; 2 3 4; 3 4 5]);\n');
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    const gridBtn  = ide.figureWindow.locator('button', { hasText: /^grid$/ });
    await expect(gridBtn).toHaveClass(/is-active/);
  });

  test('3-D surf — grid minor draws denser lines', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'surf([1 2 3; 2 3 4; 3 4 5]);\n'
      + 'grid minor;\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    const minorBtn = ide.figureWindow.locator('button', { hasText: /^minor$/ });
    await expect(minorBtn).toHaveClass(/is-active/);
    // No errors.
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('3-D plot3 — grid off then grid on round-trip', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot3([0 1 2 3], [0 1 0 1], [0 1 2 3]);\n'
      + 'grid off;\n'
      + 'grid on;\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  // -------- Toolbar interaction (visual override of script state) --------

  test('clicking grid button overrides script grid state', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([1 2 3], [1 4 9]);\n'
      + 'grid on;\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    const gridBtn = ide.figureWindow.locator('button', { hasText: /^grid$/ });
    await expect(gridBtn).toHaveClass(/is-active/);
    await gridBtn.click();
    await expect(gridBtn).not.toHaveClass(/is-active/);
  });

  test('range-row footer has top padding (visual breathing room)', async ({ ide, page }) => {
    // Theme/UX: the X/Y/Z input row was sitting flush against the
    // canvas. Verify the new padding is present.
    await ide.runScript('import compat.*;\nplot([1 2 3], [1 4 9]);\n');
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    const row = ide.figureWindow.locator('.fw-range-row');
    const padTop    = await row.evaluate((el) =>
      Number(getComputedStyle(el).paddingTop.replace('px', '')));
    const padBottom = await row.evaluate((el) =>
      Number(getComputedStyle(el).paddingBottom.replace('px', '')));
    // Both edges should breathe — the row was hugging the canvas
    // border above and the status bar below before the symmetric
    // padding fix.
    expect(padTop).toBeGreaterThanOrEqual(8);
    expect(padBottom).toBeGreaterThanOrEqual(8);
  });
});
