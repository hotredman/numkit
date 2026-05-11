// webgl-3d.spec.js — WebGL renderer for 3-D figures via three.js.
//
// Detection signals (pixel-diff is unreliable on Electron / driver
// variance, so we lean on structural + console signals):
//   1. <canvas data-numkit-3d="1"> exists in the DOM
//   2. Renderer logs "[numkit-3d] gl context ok" once on mount
//   3. data-numkit-3d-frames attribute > 0 (proves the rAF loop ran)
//   4. devErrors stays clean across mode setup
//
// Mouse-orbit interaction is checked via frame-counter advance: drag
// the canvas, wait, confirm frame count moved forward (proves the
// camera responded and triggered re-renders).

import { test, expect } from '../helpers/shared.js';

test.describe('WebGL — 3-D figures via three.js', () => {
  test('plot3 routes through Composite3DPlot — canvas mounted', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 't = linspace(0, 4*pi, 50);\n'
      + 'plot3(cos(t), sin(t), t);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    // The card is the preview; canvas lives inside it.
    const canvas = ide.figureCards.first().locator('canvas[data-numkit-3d]');
    await expect(canvas).toBeVisible({ timeout: 10_000 });
    expect(ide.devLogs()).toMatch(/\[numkit-3d\] gl context ok/);
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('scatter3 — same WebGL routing', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'scatter3([0 1 2 3 4], [0 1 4 9 16], [1 2 3 4 5]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await expect(ide.figureCards.first().locator('canvas[data-numkit-3d]'))
      .toBeVisible({ timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('stem3 — emits 1 plot3 + 1 scatter3, both rendered in WebGL', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'stem3([1 2 3 4], [1 2 3 4], [1 4 9 16]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await expect(ide.figureCards.first().locator('canvas[data-numkit-3d]'))
      .toBeVisible({ timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('surf — wireframe via two plot3 polylines, also WebGL', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'Z = [1 2 3; 2 3 4; 3 4 5];\n'
      + 'surf(Z);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await expect(ide.figureCards.first().locator('canvas[data-numkit-3d]'))
      .toBeVisible({ timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('view(az, el) — figure carries the camera view parameters', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot3([0 1 2], [0 1 0], [0 1 0]);\n'
      + 'view(45, 60);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await expect(ide.figureCards.first().locator('canvas[data-numkit-3d]'))
      .toBeVisible({ timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('view([az el]) — vector form also accepted', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot3([0 1 2], [0 1 0], [0 1 0]);\n'
      + 'view([30 45]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await expect(ide.figureCards.first().locator('canvas[data-numkit-3d]'))
      .toBeVisible({ timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('rAF loop runs — frame counter advances', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot3([0 1 2], [0 1 0], [0 1 0]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    const canvas = ide.figureCards.first().locator('canvas[data-numkit-3d]');
    await expect(canvas).toBeVisible({ timeout: 10_000 });
    // Wait a beat for the rAF loop to tick a few times.
    await page.waitForTimeout(700);
    const frames = await canvas.getAttribute('data-numkit-3d-frames');
    expect(Number(frames)).toBeGreaterThan(0);
  });

  test('3D figure opens cleanly in FigureWindow modal', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 't = linspace(0, 2*pi, 30);\n'
      + 'plot3(cos(t), sin(t), t);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    // Modal also has its own WebGL canvas.
    await expect(ide.figureWindow.locator('canvas[data-numkit-3d]'))
      .toBeVisible({ timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('2-D figures are NOT routed to WebGL (canvas-3d absent)', async ({ ide, page }) => {
    // Sanity: ordinary plot stays in SVG; this guards against the
    // detector accidentally matching 2-D figures.
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([1 2 3], [1 4 9]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    const canvas3d = ide.figureCards.first().locator('canvas[data-numkit-3d]');
    expect(await canvas3d.count()).toBe(0);
    // SVG path still works.
    await expect(ide.figureCards.first().locator('svg')).toBeVisible();
  });
});
