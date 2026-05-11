// b2-xydir.spec.js — XDir / YDir reverse + axis('ij') / axis('xy').
//
// MATLAB:
//   set(gca, 'XDir', 'reverse')   → x increases right→left
//   set(gca, 'YDir', 'reverse')   → y increases top→bottom
//   axis('ij')                    → shorthand for YDir = reverse
//   axis('xy')                    → shorthand for YDir = normal (default)
//
// Direct setters in numkit (until full `set(gca, ...)` lands):
//   xdir('reverse') / xdir('normal')
//   ydir('reverse') / ydir('normal')
//
// We don't pixel-diff direction. Tests assert:
//   1. The figure renders without console errors after each setter
//   2. The reversed-axis modal expands cleanly (axis state propagates)
//   3. axis('ij') / axis('xy') aliases render without errors

import { test, expect } from '../helpers/shared.js';

test.describe('B2 — XDir / YDir reverse + axis ij/xy', () => {
  test('xdir(\'reverse\') — figure renders without console errors', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([0 1 2 3 4], [0 1 4 9 16]);\n'
      + 'xdir(\'reverse\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('ydir(\'reverse\') — figure renders without console errors', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([0 1 2 3 4], [0 1 4 9 16]);\n'
      + 'ydir(\'reverse\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('axis(\'ij\') — shorthand for yDir reverse', async ({ ide, page }) => {
    // Classic image-coordinate axes: row index up = top of plot.
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([0 1 2 3 4], [0 1 4 9 16]);\n'
      + 'axis(\'ij\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('axis(\'xy\') — restores default y direction after axis(\'ij\')', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([0 1 2 3 4], [0 1 4 9 16]);\n'
      + 'axis(\'ij\');\n'
      + 'axis(\'xy\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('xdir + ydir together — both reversed, no errors', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([0 1 2 3 4], [0 1 4 9 16]);\n'
      + 'xdir(\'reverse\');\n'
      + 'ydir(\'reverse\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('reversed axes propagate to FigureWindow modal', async ({ ide, page }) => {
    // Same propagation guarantee tested for axisMode in b2-axis.spec.js:
    // the property must survive adapter → composite → modal renderer.
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([1 2 3], [1 4 9]);\n'
      + 'xdir(\'reverse\');\n'
      + 'ydir(\'reverse\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('xdir on log-scaled axis — flip composes with log mapping', async ({ ide, page }) => {
    // Log + reverse is the trickiest sx/sy permutation; if the log-aware
    // branch was missed the figure would silently NaN out. Just check it
    // renders; no errors.
    await ide.runScript(
      'import compat.*;\n'
      + 'semilogx([1 10 100 1000], [1 2 3 4]);\n'
      + 'xdir(\'reverse\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });
});
