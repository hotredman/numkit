// colormap-custom.spec.js — colormap(M) with N×3 RGB matrix arg.

import { test, expect } from '../../helpers/shared.js';

test.describe('colormap(M) — custom N×3 RGB palette', () => {
  test('colormap(M) — figure renders heatmap without errors', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'imagesc([1 2 3; 4 5 6; 7 8 9]);\n'
      + 'M = [1 0 0; 0 1 0; 0 0 1];\n'   // red → green → blue
      + 'colormap(M);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('colormap("name") — back-compat with named palette', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'imagesc([1 2; 3 4]);\n'
      + 'colormap(\'hot\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });
});
