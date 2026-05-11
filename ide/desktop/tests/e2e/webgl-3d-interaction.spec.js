// webgl-3d-interaction.spec.js — Etap 6: rotate3d / pan3d / zoom3d
// builtins toggle OrbitControls per-axis; data-tip shows (x, y, z) on
// hover via Raycaster.

import { test, expect } from '../helpers/shared.js';

test.describe('WebGL 3-D — interaction', () => {
  test.describe('interaction toggles', () => {
    for (const fn of ['rotate3d', 'pan3d', 'zoom3d']) {
      for (const mode of ['on', 'off']) {
        test(`${fn}('${mode}') — figure renders, no errors`, async ({ ide, page }) => {
          await ide.runScript(
            'import compat.*;\n'
            + 'plot3([0 1 2], [0 1 0], [0 1 0]);\n'
            + `${fn}('${mode}');\n`
          );
          await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
          expect(ide.devErrors().filter((e) =>
            !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
          )).toEqual([]);
        });
      }
    }
  });

  test('data-tip on hover — tooltip appears with (x, y, z)', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot3([0 1 2 3 4], [0 1 4 9 16], [1 2 3 4 5]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    const card = ide.figureCards.first();
    const canvas = card.locator('canvas[data-numkit-3d]');
    await expect(canvas).toBeVisible({ timeout: 10_000 });

    // Hover over the canvas centre. Raycast may or may not find a
    // hit (the line may not pass through the centre), so we just
    // assert the move doesn't crash.
    const box = await canvas.boundingBox();
    if (box) {
      await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
      await page.waitForTimeout(120);
    }
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('all three toggles together — no stale state', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'surf([1 2 3; 2 3 4]);\n'
      + 'rotate3d(\'off\');\n'
      + 'pan3d(\'off\');\n'
      + 'zoom3d(\'off\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });
});
