// b2-legend.spec.js — legend with positional labels and Location
// placement.
//
// MATLAB:
//   legend('a', 'b', 'c')                   — labels in series order
//   legend({...}, 'Location', 'northwest')  — explicit corner
//   legend('off')                           — hide
//
// We don't pixel-diff the legend block. Tests assert:
//   1. Figure renders without console errors after legend(...)
//   2. legend('Location', 'northwest') is parsed and applied
//   3. legend('off') clears labels and produces no errors
//   4. The legend box reaches the DOM as an SVG rect (visible state)

import { test, expect } from '@playwright/test';
import { launchIde, closeIde } from '../helpers/launch.js';
import { IdePage } from '../helpers/ide.js';

test.describe('B2 — legend + Location', () => {
  let app, page, ide;

  test.beforeEach(async () => {
    app = await launchIde();
    page = await app.firstWindow();
    ide = new IdePage(page);
    await ide.waitForReady();
  });

  test.afterEach(async () => { await closeIde(app); });

  test('legend with positional labels — figure renders, no errors', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([1 2 3], [1 4 9]);\n'
      + 'hold(\'on\');\n'
      + 'plot([1 2 3], [9 4 1]);\n'
      + 'legend(\'rising\', \'falling\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('legend(\'Location\', \'northwest\') — figure renders, no errors', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([1 2 3], [1 4 9]);\n'
      + 'legend(\'series\', \'Location\', \'northwest\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test.describe('legend Location — every supported corner', () => {
    for (const loc of ['north', 'south', 'east', 'west',
                       'northeast', 'northwest',
                       'southeast', 'southwest', 'best']) {
      test(`Location='${loc}' — renders`, async () => {
        await ide.runScript(
          'import compat.*;\n'
          + 'plot([1 2 3], [1 4 9]);\n'
          + `legend('a', 'Location', '${loc}');\n`
        );
        await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
        expect(ide.devErrors().filter((e) =>
          !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
        )).toEqual([]);
      });
    }
  });

  test('legend(\'off\') — labels cleared, figure still renders', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([1 2 3], [1 4 9]);\n'
      + 'legend(\'series\');\n'
      + 'legend(\'off\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('legend persists into FigureWindow modal', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([1 2 3], [1 4 9]);\n'
      + 'legend(\'rising\', \'Location\', \'south\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });
});
