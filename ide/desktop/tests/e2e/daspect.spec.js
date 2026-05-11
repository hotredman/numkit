// daspect.spec.js — daspect / pbaspect aspect-ratio control.

import { test, expect } from '../helpers/shared.js';

test.describe('daspect / pbaspect', () => {
  test('daspect([1 1 1]) — accepts and (no error)', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([0 1 2 3], [0 1 4 9]);\n'
      + 'daspect([1 1 1]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('pbaspect([2 1 1]) — accepts (no error)', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([0 1 2], [0 1 2]);\n'
      + 'pbaspect([2 1 1]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('daspect("auto") — clears the mode', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([0 1 2], [0 1 2]);\n'
      + 'daspect([1 1 1]);\n'
      + 'daspect(\'auto\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });
});
