// t1d-fplot.spec.js — Tier-1 batch D: function-handle plots.
//
// Engine::callFunctionHandle bridges C++ builtins to user anonymous
// functions (`@(x) sin(x)`). fplot evaluates 1-arg handles on a
// dense X grid; fcontour / fsurf / fmesh sample 2-arg handles on a
// 30×30 grid and proxy through compat.contour / compat.surf.

import { test, expect } from '../../helpers/shared.js';

test.describe('Tier 1D — fplot family', () => {
  test('fplot(@(x) sin(x), [0 2*pi]) — figure renders, no errors', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'fplot(@(x) sin(x), [0 2*pi]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('fplot uses default range when [a, b] omitted', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'fplot(@(x) x.^2);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('fplot survives f(x) = NaN at some samples', async ({ ide, page }) => {
    // log of negative — engine produces NaN; fplot should emit `null`
    // for those samples and keep the figure clean.
    await ide.runScript(
      'import compat.*;\n'
      + 'fplot(@(x) log(x), [-2 5]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('fcontour(@(x,y) x.^2 + y.^2, [-2 2 -2 2]) — renders contour lines', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'fcontour(@(x, y) x.^2 + y.^2, [-2 2 -2 2]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('fsurf(@(x,y) sin(x) .* cos(y), [-pi pi]) — wireframe', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'fsurf(@(x, y) sin(x) .* cos(y), [-pi pi]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('fmesh(@(x,y) x + y) — same wireframe path as fsurf', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'fmesh(@(x, y) x + y, [-1 1 -1 1]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('fplot with non-handle first arg — gracefully no-op', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'fplot([1 2 3]);\n'
      + 'fprintf(\'survived\\n\');\n'
    );
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
    // Engine usable for the next plot.
    await ide.runScript('import compat.*;\nplot([1 2 3], [1 2 3]);\n');
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  });

  test('fplot opens cleanly in modal', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'fplot(@(x) sin(x) ./ x, [-10 10]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });
});
