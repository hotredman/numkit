// b3-streamline.spec.js — RK4 streamlines through a 2-D vector field.
//
// MATLAB:
//   streamline(X, Y, U, V, sx, sy)  — explicit seed points
//   streamslice(X, Y, U, V)         — auto-pick seeds across the grid
//
// numkit's streamslice picks a 5×5 grid centred on each cell of the
// 5×5 super-grid covering (xMin..xMax, yMin..yMax). Integration uses
// classic 4-th-order Runge–Kutta with h = 0.4 * min(dx, dy); it stops
// on out-of-bounds, NaN cell, or |F| < 1e-9 (stall).
//
// Tests assert:
//   1. Figure renders without console errors
//   2. The card has SVG paths (the trace polyline)
//   3. Both forms (streamline + streamslice) work

import { test, expect } from '../helpers/shared.js';

test.describe('B3 — streamline / streamslice', () => {
  test('streamslice — uniform field renders, no errors', async ({ ide, page }) => {
    // Constant field U=1, V=0 → horizontal streamlines from every seed.
    await ide.runScript(
      'import compat.*;\n'
      + 'x = linspace(0, 1, 6);\n'
      + 'y = linspace(0, 1, 6);\n'
      + 'U = ones(6, 6);\n'
      + 'V = zeros(6, 6);\n'
      + 'streamslice(x, y, U, V);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('streamline with explicit seeds — rotation field renders', async ({ ide, page }) => {
    // Rigid rotation: U(x,y) = -y, V(x,y) = x → circular flow.
    await ide.runScript(
      'import compat.*;\n'
      + 'x = linspace(-1, 1, 11);\n'
      + 'y = linspace(-1, 1, 11);\n'
      + 'U = zeros(11, 11);\n'
      + 'V = zeros(11, 11);\n'
      + 'for i = 1:11\n'
      + '  for j = 1:11\n'
      + '    U(i, j) = -y(i);\n'
      + '    V(i, j) =  x(j);\n'
      + '  end\n'
      + 'end\n'
      + 'sx = [0.5 -0.5 0.5 -0.5];\n'
      + 'sy = [0.5  0.5 -0.5 -0.5];\n'
      + 'streamline(x, y, U, V, sx, sy);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
    const paths = await ide.figureCards.first().locator('svg path').count();
    expect(paths).toBeGreaterThanOrEqual(1);
  });

  test('streamslice with sink field — paths converge, no crash', async ({ ide, page }) => {
    // Inward flow: U = -x, V = -y.
    await ide.runScript(
      'import compat.*;\n'
      + 'x = linspace(-1, 1, 7);\n'
      + 'y = linspace(-1, 1, 7);\n'
      + 'U = zeros(7, 7);\n'
      + 'V = zeros(7, 7);\n'
      + 'for i = 1:7\n'
      + '  for j = 1:7\n'
      + '    U(i, j) = -x(j);\n'
      + '    V(i, j) = -y(i);\n'
      + '  end\n'
      + 'end\n'
      + 'streamslice(x, y, U, V);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('streamline modal — expands cleanly', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'x = linspace(0, 2, 5);\n'
      + 'y = linspace(0, 2, 5);\n'
      + 'U = ones(5, 5);\n'
      + 'V = ones(5, 5);\n'
      + 'streamline(x, y, U, V, [0.5 1.0], [0.5 0.5]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('streamslice with zero field — no integration, no crash', async ({ ide, page }) => {
    // |F| = 0 everywhere → stall on first step. Engine must remain clean.
    await ide.runScript(
      'import compat.*;\n'
      + 'x = linspace(0, 1, 4);\n'
      + 'y = linspace(0, 1, 4);\n'
      + 'U = zeros(4, 4);\n'
      + 'V = zeros(4, 4);\n'
      + 'streamslice(x, y, U, V);\n'
      + 'fprintf(\'survived\\n\');\n'
    );
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
    await ide.runScript('import compat.*;\nplot([1 2 3], [1 2 3]);\n');
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  });
});
