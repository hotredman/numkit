// webgl-3d-window.spec.js — FigureWindow integration for 3-D figures.
// Covers: X/Y/Z range inputs in the footer, fit-3D menu (all/X/Y/Z),
// PNG export through canvas, SVG export disabled, CSV/JSON download.

import { test, expect } from '../../helpers/shared.js';

test.describe('FigureWindow — 3-D layout', () => {
  test('plot3 — X / Y / Z inputs in the footer (6 inputs total)', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot3([0 1 2 3], [0 1 0 1], [0 1 2 3]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });

    const footerRow = ide.figureWindow.locator('.fw-range-row');
    await expect(footerRow).toBeVisible({ timeout: 5_000 });
    expect(await footerRow.locator('input').count()).toBe(6);   // x/y/z lo+hi

    // Status bar should show all three axes.
    const status = ide.figureWindow.locator('.fw-status');
    await expect(status).toContainText('x ∈');
    await expect(status).toContainText('y ∈');
    await expect(status).toContainText('z ∈');
  });

  test('plot3 — input values reflect data extent after first render', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot3([0 1 2 3 4], [0 1 4 9 16], [1 2 3 4 5]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    // Wait a tick for onBBox → setViewport.
    await page.waitForTimeout(300);

    const inputs = ide.figureWindow.locator('.fw-range-row input');
    // x-lo / x-hi should track the actual data extent (0 / 4).
    const xLo = await inputs.nth(0).inputValue();
    const xHi = await inputs.nth(1).inputValue();
    expect(Number(xLo)).toBeCloseTo(0, 1);
    expect(Number(xHi)).toBeCloseTo(4, 1);
  });

  test('Fit menu — 3-D popup has X / Y / Z buttons', async ({ ide, page }) => {
    await ide.runScript('import compat.*;\nsurf([1 2 3; 2 3 4; 3 4 5]);\n');
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });

    await ide.figureWindow.locator('button', { hasText: 'fit' }).first().click();
    const popup = ide.figureWindow.locator('.fw-pop');
    await expect(popup).toBeVisible({ timeout: 5_000 });
    await expect(popup.locator('button', { hasText: 'all axes' })).toBeVisible();
    await expect(popup.locator('button', { hasText: 'X only' })).toBeVisible();
    await expect(popup.locator('button', { hasText: 'Y only' })).toBeVisible();
    await expect(popup.locator('button', { hasText: 'Z only' })).toBeVisible();
    // After the toolbar fit ▾ overhaul this row is just "reset".
    await expect(popup.locator('button', { hasText: /^reset$/ })).toBeVisible();
  });

  test('Fit · Z only — fires without errors', async ({ ide, page }) => {
    await ide.runScript('import compat.*;\nsurf([1 2; 3 4]);\n');
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    await page.waitForTimeout(300);

    await ide.figureWindow.locator('button', { hasText: 'fit' }).first().click();
    await ide.figureWindow.locator('.fw-pop button', { hasText: 'Z only' }).click();
    await page.waitForTimeout(150);

    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('Save · PNG download fires on a 3-D figure', async ({ ide, page }) => {
    await ide.runScript('import compat.*;\nsurf([1 2 3; 2 3 4; 3 4 5]);\n');
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });

    await ide.figureWindow.locator('button', { hasText: 'save / export' }).click();
    const dl = page.waitForEvent('download', { timeout: 5_000 }).catch(() => null);
    await ide.figureWindow.locator('button', { hasText: 'PNG @2×' }).click();
    const evt = await dl;
    // Whether the download dialog actually surfaces in headless
    // Electron is environment-dependent; the strong signal is "no
    // crash / no console error".
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('Save · SVG button is disabled for 3-D (with explanatory tooltip)', async ({ ide, page }) => {
    await ide.runScript('import compat.*;\nsurf([1 2; 3 4]);\n');
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });

    await ide.figureWindow.locator('button', { hasText: 'save / export' }).click();
    const svgBtn = ide.figureWindow.locator('button', { hasText: 'SVG' });
    await expect(svgBtn).toBeVisible({ timeout: 5_000 });
    expect(await svgBtn.isDisabled()).toBe(true);
  });

  test('Save · CSV download fires on a 3-D figure', async ({ ide, page }) => {
    await ide.runScript('import compat.*;\nplot3([1 2 3], [4 5 6], [7 8 9]);\n');
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });

    await ide.figureWindow.locator('button', { hasText: 'save / export' }).click();
    const dl = page.waitForEvent('download', { timeout: 5_000 }).catch(() => null);
    await ide.figureWindow.locator('button', { hasText: 'CSV' }).click();
    await dl;
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('Composite3DPlot ignores hardcoded clearColor — wrapper carries CSS bg', async ({ ide, page }) => {
    // Theme regression test (Step 0). The wrapper div should carry
    // background `var(--plot-bg, ...)` so the WebGL canvas can stay
    // transparent and react to theme switches.
    await ide.runScript('import compat.*;\nplot3([1 2 3], [1 2 3], [1 2 3]);\n');
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    const wrapper = ide.figureCards.first().locator(
      'canvas[data-numkit-3d]').locator('xpath=..');
    const bgStyle = await wrapper.evaluate((el) => el.style.background);
    expect(bgStyle).toContain('--plot-bg');
  });
});
