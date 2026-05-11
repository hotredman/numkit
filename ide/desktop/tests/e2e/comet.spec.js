// comet.spec.js — animated trail (v1: routes to plot / plot3 with
// full data). Real step-by-step animation is BACKLOG.

import { test, expect } from '../helpers/shared.js';

test.describe('comet / comet3', () => {
  test('comet(x, y) — final-state line plot', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 't = linspace(0, 4*pi, 50);\n'
      + 'comet(t, sin(t));\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('comet3(x, y, z) — 3-D trail', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 't = linspace(0, 4*pi, 30);\n'
      + 'comet3(sin(t), cos(t), t);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    await expect(ide.figureWindow.locator('canvas[data-numkit-3d]'))
      .toBeVisible({ timeout: 5_000 });
  });
});
