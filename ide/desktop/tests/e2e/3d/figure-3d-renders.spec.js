// figure-3d-renders.spec.js — guard against the "process is not defined"
// regression in the 3-D renderer.
//
// Pre-fix: Composite3DPlot.jsx had `preserveDrawingBuffer:
// process.env.NUMKIT_E2E === '1'`. Vite doesn't define `process` in the
// browser bundle (no `define` block in vite.config), so any 3-D figure
// (surf / mesh / plot3 / scatter3 / waterfall / etc.) crashed with
// "process is not defined" and the figure window showed
// "figure render error" instead of the plot.
//
// Fix replaced the reference with `import.meta.env.VITE_NUMKIT_E2E === '1'`
// — replaced at build time, never undefined at runtime.
//
// We assert: 3-D figure renders without console errors AND without
// the user-visible "figure render error" panel surfacing.

import { test, expect } from '../../helpers/shared.js';

const NON_FATAL = (e) =>
  !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e);

test.describe('3-D figure renders', () => {
  test('surf — opens, renders, no "process" error', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + '[X, Y] = meshgrid(-2:0.4:2, -2:0.4:2);\n'
      + 'Z = X .* exp(-X.^2 - Y.^2);\n'
      + 'surf(X, Y, Z);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    await page.waitForTimeout(300);   // three.js init + first frame

    // No "figure render error" panel surfaced (that was the visible
    // symptom of process.env exploding in WebGLRenderer construction).
    expect(await page.locator('text=figure render error').count()).toBe(0);

    // The 3-D canvas + label layer must be in the DOM (Composite3DPlot
    // mounts both unconditionally).
    await expect(page.locator('.fw-window canvas').first()).toBeVisible();

    const errors = ide.devErrors().filter(NON_FATAL);
    const processErr = errors.find((e) => /process is not defined/.test(e));
    expect(processErr).toBeUndefined();
    expect(errors).toEqual([]);
  });

  test('plot3 — line in 3-D, no render error', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 't = linspace(0, 4*pi, 200);\n'
      + 'plot3(cos(t), sin(t), t);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    await page.waitForTimeout(300);

    expect(await page.locator('text=figure render error').count()).toBe(0);
    await expect(page.locator('.fw-window canvas').first()).toBeVisible();

    expect(ide.devErrors().filter(NON_FATAL)).toEqual([]);
  });
});
