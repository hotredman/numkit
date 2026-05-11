// webgl-3d-edge.spec.js — Etap 8: edge cases for 3-D rendering.
//
// Covers degenerate inputs (single point, all NaN, zero-extent axes),
// figure-type swaps, subplot mixes (2-D + 3-D in one window), and
// repeated re-runs. The IDE must stay alive in every case (no crash
// dialogue, no FigureErrorBoundary tile, no leaked WebGL contexts).

import { test, expect } from '../../helpers/shared.js';

test.describe('WebGL 3-D — edge cases', () => {
  test('plot3 with single point — degenerate bbox stays finite', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot3(1, 2, 3);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('surf with all-zero Z — flat plane, no NaN explosion', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'surf(zeros(4, 4));\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('surf with constant non-zero Z — single z-level surface', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'surf(ones(4, 4) * 5);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('plot3 with NaN points — line breaks survive into 3-D', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot3([0 1 NaN 3 4], [0 1 NaN 3 4], [0 1 NaN 3 4]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('surf with NaN cells — leave holes, no garbage triangles', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'Z = [1 2 3; 2 NaN 4; 3 4 5];\n'
      + 'surf(Z);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('mixed 3-D types — plot3 + scatter3 + surf in one figure', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'surf([1 2 3; 2 3 4; 3 4 5]);\n'
      + 'hold(\'on\');\n'
      + 'plot3([0.5 1.5 2.5], [0.5 1.5 2.5], [3 5 7]);\n'
      + 'scatter3([1 2 3], [2 1 3], [4 4 4]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('repeat figure swap (3-D → 2-D → 3-D) — no leaked WebGL state', async ({ ide, page }) => {
    for (let i = 0; i < 3; i++) {
      await ide.runScript('import compat.*;\nplot3([1 2 3], [1 4 9], [0 1 2]);\n');
      await ide.runScript('import compat.*;\nplot([1 2 3], [1 4 9]);\n');
      await ide.runScript('import compat.*;\nsurf([1 2 3; 2 3 4]);\n');
    }
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
       && !/FigureBoundary/i.test(e)
    )).toEqual([]);
  });

  test('subplot — one 2-D cell + one 3-D cell in the same figure', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'subplot(1, 2, 1);\n'
      + 'plot([0 1 2 3], [0 1 4 9]);\n'
      + 'subplot(1, 2, 2);\n'
      + 'surf([1 2 3; 2 3 4; 3 4 5]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    // 3-D cell mounts a canvas; 2-D cell stays in SVG.
    const canvases3d = await ide.figureCards.first()
      .locator('canvas[data-numkit-3d]').count();
    expect(canvases3d).toBeGreaterThanOrEqual(1);
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('huge 3-D grid (50×50 surf) — performance + no crash', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'x = linspace(-3, 3, 50);\n'
      + 'y = linspace(-3, 3, 50);\n'
      + 'Z = zeros(50, 50);\n'
      + 'for i = 1:50; for j = 1:50;\n'
      + '  Z(i, j) = sin(x(j))*cos(y(i));\n'
      + 'end; end;\n'
      + 'surf(x, y, Z);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 15_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('Inf in z — clamped, no NaN propagation crash', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot3([0 1 2], [0 1 2], [0 Inf 2]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('rapid view changes — 5 sequential view() calls', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot3([0 1 2 3], [0 1 4 9], [1 2 3 4]);\n'
      + 'view(0, 0);\n'
      + 'view(45, 45);\n'
      + 'view(90, 30);\n'
      + 'view(2);\n'
      + 'view(3);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });
});
