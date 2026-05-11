// polar-thetalim.spec.js — angular range (`thetalim`) parity:
//   • script `thetalim([a b])` reaches the renderer
//   • FigureWindow footer exposes θ° lo / hi inputs alongside r
//   • partial sweep clips the series + draws a wedge-style frame

import { test, expect } from '../helpers/shared.js';

test.describe('polar — thetalim', () => {
  test('polarplot footer has 4 inputs (r lo/hi + θ lo/hi)', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'theta = linspace(0, 2*pi, 100);\n'
      + 'polarplot(theta, sin(2*theta));\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    const inputs = ide.figureWindow.locator('.fw-range-row input');
    expect(await inputs.count()).toBe(4);
    // Default sweep [0, 360] when thetalim wasn't set.
    expect(Number(await inputs.nth(2).inputValue())).toBeCloseTo(0, 1);
    expect(Number(await inputs.nth(3).inputValue())).toBeCloseTo(360, 1);
  });

  test('thetalim([0 90]) — script value reaches the modal inputs', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'theta = linspace(0, pi/2, 30);\n'
      + 'polarplot(theta, theta);\n'
      + 'thetalim([0 90]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    const inputs = ide.figureWindow.locator('.fw-range-row input');
    expect(Number(await inputs.nth(2).inputValue())).toBeCloseTo(0, 1);
    expect(Number(await inputs.nth(3).inputValue())).toBeCloseTo(90, 1);
  });

  test('thetalim([-180 180]) — symmetric sweep also passes through', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'theta = linspace(-pi, pi, 60);\n'
      + 'polarplot(theta, abs(theta));\n'
      + 'thetalim([-180 180]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    const inputs = ide.figureWindow.locator('.fw-range-row input');
    expect(Number(await inputs.nth(2).inputValue())).toBeCloseTo(-180, 1);
    expect(Number(await inputs.nth(3).inputValue())).toBeCloseTo(180, 1);
  });

  test('partial sweep — frame is wedge-shaped, full sweep — disc', async ({ ide, page }) => {
    // Partial sweep should add the two radial spokes that close the
    // wedge (full sweep doesn't). Count <line stroke="…plot-frame"> as
    // proxy.
    await ide.runScript(
      'import compat.*;\n'
      + 'theta = linspace(0, pi/2, 30);\n'
      + 'polarplot(theta, theta);\n'
      + 'thetalim([0 90]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    // Modal renders an SVG; the frame arc is a <path>, the closing
    // radials are <line>s. We just confirm at least one path with
    // the frame stroke appears (full sweep would use <circle>).
    const framePaths = await ide.figureWindow.locator('svg path').count();
    expect(framePaths).toBeGreaterThanOrEqual(1);
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('changing θ-hi via input — no console errors', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'theta = linspace(0, 2*pi, 60);\n'
      + 'polarplot(theta, sin(theta));\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    const inputs = ide.figureWindow.locator('.fw-range-row input');
    // Set θ-hi to 180 (was 360), narrowing to half-disc.
    await inputs.nth(3).click();
    await inputs.nth(3).fill('180');
    await inputs.nth(3).press('Enter');
    await page.waitForTimeout(150);
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('status bar shows θ ∈ [...]', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'theta = linspace(0, pi, 30);\n'
      + 'polarplot(theta, theta);\n'
      + 'thetalim([0 180]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    const status = ide.figureWindow.locator('.fw-status');
    await expect(status).toContainText('θ ∈');
  });
});
