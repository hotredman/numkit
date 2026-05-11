// bubble-swarm.spec.js — bubblechart / bubblechart3 / swarmchart /
// swarmchart3 — all delegate to scatter / scatter3 with sizes
// passed through.

import { test, expect } from '../../helpers/shared.js';

test.describe('bubblechart / swarmchart family', () => {
  test('bubblechart(x, y, sizes) — markers visible', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'bubblechart([1 2 3 4], [4 1 3 2], [50 80 30 100]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    const circles = await ide.figureCards.first().locator('svg circle').count();
    expect(circles).toBeGreaterThanOrEqual(4);
  });

  test('bubblechart3 — 3-D variant mounts WebGL canvas', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'bubblechart3([1 2 3], [3 2 1], [1 4 9], [50 80 30]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    await expect(ide.figureWindow.locator('canvas[data-numkit-3d]'))
      .toBeVisible({ timeout: 5_000 });
  });

  test('swarmchart(x, y) — routes through scatter, no errors', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'swarmchart([1 1 1 2 2 2 3 3 3], [1 2 3 1 2 3 1 2 3]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });
});
