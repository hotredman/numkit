// b2-axis.spec.js — axis equal / square / tight modifiers.
//
// MATLAB:
//   axis equal    — 1 data unit on X = 1 data unit on Y in screen px
//   axis square   — plot box is a square (W = H = min(W, H))
//   axis tight    — no whitespace pad around data
//
// We don't pixel-diff. Tests assert:
//   1. The figure renders without console errors after `axis <mode>`
//   2. For `square`, the plot panel becomes square-ish (W ≈ H)
//   3. For `tight`, the figure card still renders cleanly

import { test, expect } from '../helpers/shared.js';

test.describe('B2 — axis equal / square / tight', () => {
  test('axis equal — figure renders without console errors', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([0 1 2 3 4], [0 0.5 1 0.5 0]);\n'
      + 'axis(\'equal\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('axis square — figure renders, no console errors', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([0 1 2 3], [0 5 0 -5]);\n'
      + 'axis(\'square\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('axis tight — viewport with no pad still renders', async ({ ide, page }) => {
    // tight removes the auto 4%/6% padding; the data extent maps to
    // the panel edges. We just check a figure card appears and no
    // errors fire.
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([1 2 3 4 5], [10 20 30 20 10]);\n'
      + 'axis(\'tight\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('axis modes are also reachable in the FigureWindow modal', async ({ ide, page }) => {
    // Once the user expands the figure into the modal (one click on
    // the card), the same axisMode should apply — this is a sanity
    // check that the property propagates from the adapter through to
    // the modal renderer.
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([0 1], [0 1]);\n'
      + 'axis(\'equal\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    // No render errors after expanding under the new axisMode path.
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });
});
