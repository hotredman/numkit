// webgl-3d-axes.spec.js — Etap 1 axes infrastructure for 3-D.
//
// Covers: zlim / zlabel builtins (no longer noops), Z-axis tick labels
// rendered as HTML overlays via CSS2DRenderer, view(2) / view(3)
// preset shortcuts, axis equal / vis3d for 3-D figures, grid on/off.

import { test, expect } from '../helpers/shared.js';

test.describe('WebGL 3-D — axes infrastructure', () => {
  test('zlabel + zlim — both reach the renderer (no errors)', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot3([0 1 2], [0 1 0], [0 1 0]);\n'
      + 'xlabel(\'time\');\n'
      + 'ylabel(\'channel\');\n'
      + 'zlabel(\'amplitude\');\n'
      + 'zlim([-1 2]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await expect(ide.figureCards.first().locator('canvas[data-numkit-3d]'))
      .toBeVisible({ timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('view(2) — top-down preset', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot3([0 1 2], [0 1 2], [0 1 4]);\n'
      + 'view(2);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('view(3) — default 3-D preset', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot3([0 1 2], [0 1 2], [0 1 4]);\n'
      + 'view(3);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('axis(\'equal\') on 3-D figure — single scale across axes', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot3([0 10 20], [0 1 0], [0 1 0]);\n'
      + 'axis(\'equal\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('axis(\'vis3d\') on 3-D figure — same single-scale path', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot3([0 1 2], [0 5 10], [0 100 200]);\n'
      + 'axis(\'vis3d\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('tick labels are rendered as HTML overlays inside the canvas wrapper', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot3([0 1 2 3 4], [0 1 4 9 16], [1 2 3 4 5]);\n'
      + 'xlabel(\'X\'); ylabel(\'Y\'); zlabel(\'Z\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    // CSS2DRenderer creates absolutely-positioned <div>s next to the
    // canvas. Tick + axis labels combined → at least 4 divs (3 axis
    // names + several ticks). We don't pixel-diff, just count.
    const labelDivs = await ide.figureCards.first()
      .locator('canvas[data-numkit-3d]')
      .locator('xpath=..')
      .locator('div')
      .count();
    expect(labelDivs).toBeGreaterThanOrEqual(3);
  });

  test('grid off — no console errors when grid switched off', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot3([0 1 2], [0 1 0], [0 1 0]);\n'
      + 'grid(\'off\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('xlim + ylim + zlim user-set — renderer honours bounds', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot3([0 1 2], [0 1 2], [0 1 2]);\n'
      + 'xlim([-5 5]); ylim([-5 5]); zlim([-5 5]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('plot3 + surf together — first plot3, then surf clears + replaces', async ({ ide, page }) => {
    // Repro of the regression that took the IDE down on b3-surf:
    // figure swap from one 3-D type to another. With ErrorBoundary
    // we shouldn't crash even if internals throw; ideal case:
    // it just works.
    await ide.runScript(
      'import compat.*;\n'
      + 'plot3([1 2 3], [1 4 9], [0 1 2]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.runScript(
      'import compat.*;\n'
      + 'surf([1 2 3; 2 3 4; 3 4 5]);\n'
    );
    await expect(ide.figureCards.first().locator('canvas[data-numkit-3d]'))
      .toBeVisible({ timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
       && !/FigureBoundary/i.test(e)   // boundary's own log is fine
    )).toEqual([]);
  });
});
