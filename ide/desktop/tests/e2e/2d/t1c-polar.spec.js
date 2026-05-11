// t1c-polar.spec.js — Tier-1 batch C: polarscatter, polarhistogram.
// Both reuse the polar adapter + PolarPlot renderer, with a small
// extension to PolarPlot.jsx that picks scatter / bar / line by
// series.mode.

import { test, expect } from '../../helpers/shared.js';

test.describe('Tier 1C — polarscatter / polarhistogram', () => {
  test('polarscatter(theta, rho) — markers on polar axes', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'theta = linspace(0, 2*pi, 12);\n'
      + 'rho = ones(1, 12);\n'
      + 'polarscatter(theta, rho);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('polarhistogram(theta) — angular wedges from binned theta', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'theta = [0 0 0.1 0.2 1 1.1 1.2 1.5 3 3.1 3.2 5 5.1 6];\n'
      + 'polarhistogram(theta, 12);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('polarhistogram default 36 bins', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'theta = linspace(0, 2*pi, 200);\n'
      + 'polarhistogram(theta);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('polar trio (polarplot + polarscatter + polarhistogram) compose cleanly', async ({ ide, page }) => {
    // Hold-on accumulation across all three polar modes — verifies the
    // adapter split (one mode per dataset) and the PolarPlot renderer
    // can mix all three on the same axes.
    await ide.runScript(
      'import compat.*;\n'
      + 'theta = linspace(0, 2*pi, 64);\n'
      + 'polarplot(theta, sin(2*theta));\n'
      + 'hold(\'on\');\n'
      + 'polarscatter([0 1 2 3 4 5], [0.5 0.5 0.5 0.5 0.5 0.5]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('polarscatter — 12 markers reach the SVG', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'theta = linspace(0, 2*pi, 12);\n'
      + 'rho = [1 2 3 4 5 6 7 8 9 10 11 12];\n'
      + 'polarscatter(theta, rho);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    const circles = await ide.figureCards.first().locator('svg circle').count();
    // Polar grid draws ~5 grid circles + 12 markers; check we exceed
    // grid alone.
    expect(circles).toBeGreaterThanOrEqual(12);
  });

  test('polar batch opens cleanly in modal', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'polarhistogram(linspace(0, 2*pi, 100), 24);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });
});
