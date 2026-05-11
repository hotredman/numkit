// b1-quiver.spec.js — vector field arrows.
//
// quiver(x, y, u, v) → N arrows. Each arrow renders as 3 SVG <line>s
// (shaft + 2 head fins). Zero-length vectors are skipped — so a
// well-defined input of 5 non-zero arrows should produce ≥ 15 lines.

import { test, expect } from '../../helpers/shared.js';

test.describe('B1 — quiver', () => {
  test('quiver(x, y, u, v) — 5 arrows → ≥ 15 lines', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'quiver([1 2 3 4 5], [1 1 1 1 1], [0.5 0.5 0.5 0.5 0.5], [0.3 -0.2 0.4 -0.1 0.2]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    const lines = await ide.figureCards.first().locator('svg line').count();
    expect(lines, `quiver drew ${lines} <line>s; expected ≥ 15 (5 arrows × 3 segs)`)
      .toBeGreaterThanOrEqual(15);
  });

  test('quiver with custom scale renders without errors', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'quiver([1 2 3], [2 2 2], [1 1 1], [0 1 -1], 0.5);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('quiver overlay on imagesc (composite)', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'imagesc([1 2 3], [1 2 3], [1 2 3; 4 5 6; 7 8 9]);\n'
      + 'hold on;\n'
      + 'quiver([1 2 3], [1 2 3], [0.3 0.3 0.3], [0.3 0.3 0.3]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    const card = ide.figureCards.first();
    // Image (heatmap) + arrows both rendered.
    expect(await card.locator('svg image').count()).toBeGreaterThanOrEqual(1);
    expect(await card.locator('svg line').count()).toBeGreaterThanOrEqual(9);
  });
});
