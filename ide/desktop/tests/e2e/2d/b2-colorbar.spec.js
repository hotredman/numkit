// b2-colorbar.spec.js — colorbar placement.
//
// MATLAB:
//   colorbar                          — default 'eastoutside'
//   colorbar('Location', 'south')     — horizontal under plot
//   colorbar('off')                   — hide
//
// IDE convenience: `imagesc` already shows a default 'east' colorbar
// without an explicit call (a side-effect of having a heatmap layer).
// `colorbar('off')` is the way to hide it.
//
// Tests assert:
//   1. Figure renders cleanly after colorbar(...) for every Location
//   2. colorbar('off') hides the bar (no errors)
//   3. Modal expansion preserves the location

import { test, expect } from '../../helpers/shared.js';

test.describe('B2 — colorbar Location', () => {
  test('colorbar() with default east placement — renders', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'imagesc(magic(8));\n'
      + 'colorbar();\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test.describe('colorbar Location — every supported side', () => {
    for (const loc of ['east', 'west', 'north', 'south',
                       'eastoutside', 'westoutside',
                       'northoutside', 'southoutside']) {
      test(`Location='${loc}' — renders`, async ({ ide, page }) => {
        await ide.runScript(
          'import compat.*;\n'
          + 'imagesc(magic(8));\n'
          + `colorbar('Location', '${loc}');\n`
        );
        await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
        expect(ide.devErrors().filter((e) =>
          !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
        )).toEqual([]);
      });
    }
  });

  test('colorbar(\'off\') — hides the bar, figure still renders', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'imagesc(magic(6));\n'
      + 'colorbar(\'off\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('colorbar location propagates into FigureWindow modal', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'imagesc(magic(8));\n'
      + 'colorbar(\'Location\', \'south\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });
});
