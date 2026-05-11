// animatedline.spec.js — incremental line plot.
//   h = animatedline;
//   addpoints(h, x, y);
//   clearpoints(h);
//   [x, y] = getpoints(h);
//   drawnow;
//
// numkit doesn't model handles; the cluster targets the most-recent
// animated dataset on the current axes. Coverage:

import { test, expect } from '../../helpers/shared.js';

test.describe('animatedline + addpoints / clearpoints / getpoints', () => {
  test('animatedline + addpoints — line dataset receives accumulated points', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'h = animatedline;\n'
      + 'for k = 1:10\n'
      + '  addpoints(h, k, k * 2);\n'
      + 'end\n'
      + 'drawnow;\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('animatedline(x0, y0) — initial points seed the line', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'h = animatedline([1 2 3 4 5], [1 4 9 16 25]);\n'
      + 'addpoints(h, 6, 36);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('clearpoints empties the line; addpoints rebuilds it', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'h = animatedline([1 2 3], [1 2 3]);\n'
      + 'clearpoints(h);\n'
      + 'addpoints(h, [10 20], [100 200]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('getpoints returns current x, y vectors', async ({ ide, page }) => {
    let outputs = [];
    page.on('console', (msg) => outputs.push(msg.text()));
    await ide.runScript(
      'import compat.*;\n'
      + 'h = animatedline([1 2 3], [10 20 30]);\n'
      + '[xv, yv] = getpoints(h);\n'
      + 'fprintf(\'len=%d xv1=%g yv1=%g xv3=%g yv3=%g\\n\', '
      + 'length(xv), xv(1), yv(1), xv(3), yv(3));\n'
    );
    // Wait a beat for fprintf output to surface in the IDE console.
    await page.waitForTimeout(200);
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('drawnow without animatedline — safe no-op', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([1 2 3], [1 4 9]);\n'
      + 'drawnow;\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });
});
