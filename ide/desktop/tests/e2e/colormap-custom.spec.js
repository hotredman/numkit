// colormap-custom.spec.js — colormap(M) with N×3 RGB matrix arg.

import { test, expect } from '@playwright/test';
import { launchIde, closeIde } from '../helpers/launch.js';
import { IdePage } from '../helpers/ide.js';

test.describe('colormap(M) — custom N×3 RGB palette', () => {
  let app, page, ide;

  test.beforeEach(async () => {
    app = await launchIde();
    page = await app.firstWindow();
    ide = new IdePage(page);
    await ide.waitForReady();
  });

  test.afterEach(async () => { await closeIde(app); });

  test('colormap(M) — figure renders heatmap without errors', async () => {
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

  test('colormap("name") — back-compat with named palette', async () => {
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
