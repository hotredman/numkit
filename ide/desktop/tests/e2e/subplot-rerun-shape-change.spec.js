// subplot-rerun-shape-change.spec.js — re-running a script that lands on
// the same Figure-1 id but with a DIFFERENT subplot shape used to crash:
//
//   TypeError: Cannot read properties of undefined (reading 'x')
//
// Cause: SubplotGrid's per-cell `viewports` state was seeded once on mount.
// React kept the SubplotGrid mounted across the figure swap (same fig id),
// so when the new figure had MORE cells, viewports[idx] was undefined for
// the new ones and CompositePlot crashed on viewport.x[0].
//
// Fix: re-init viewports inside a useEffect keyed on figure.id + cells
// shape, plus defensive fallback to defaultViewport(cell) in the render
// call to cover the transient pre-effect render.

import { test, expect } from '@playwright/test';
import { launchIde, closeIde } from '../helpers/launch.js';
import { IdePage } from '../helpers/ide.js';

const NON_FATAL = (e) =>
  !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e);

test.describe('Subplot grid: re-run with shape change', () => {
  let app, page, ide;

  test.beforeEach(async () => {
    app = await launchIde();
    page = await app.firstWindow();
    ide = new IdePage(page);
    await ide.waitForReady();
  });

  test.afterEach(async () => { await closeIde(app); });

  test('1×3 subplot → close all → 2×3 subplot — no crash', async () => {
    // Mirrors the user-reported sequence: color_space_demo (1×3 imshow)
    // then morphology_pipeline (2×3 imshow with skipped slot).
    await ide.runScript(
      'import compat.*;\n'
      + 'mask = false(8, 8); mask(1:4, :) = true;\n'
      + 'figure;\n'
      + 'subplot(1,3,1); imshow(mask); title(\'1\');\n'
      + 'subplot(1,3,2); imshow(mask); title(\'2\');\n'
      + 'subplot(1,3,3); imshow(mask); title(\'3\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });

    // Close all figures.
    await ide.figureCloseAll.click();
    await expect(ide.figureCards).toHaveCount(0, { timeout: 5_000 });
    await page.waitForTimeout(100);

    // Run the second script — Figure 1 id is reused, cells shape changes
    // 3 → 5. This was the crash trigger.
    await ide.runScript(
      'import compat.*;\n'
      + 'mask = false(8, 8); mask(1:4, :) = true;\n'
      + 'figure;\n'
      + 'subplot(2,3,1); imshow(mask); title(\'1\');\n'
      + 'subplot(2,3,2); imshow(mask); title(\'2\');\n'
      + 'subplot(2,3,3); imshow(mask); title(\'3\');\n'
      + 'subplot(2,3,5); imshow(mask); title(\'5\');\n'
      + 'subplot(2,3,6); imshow(mask); title(\'6\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    await page.waitForTimeout(300);

    // 5 cells render (and 1 placeholder for the skipped slot).
    expect(await page.locator('.fw-window .fw-canvas-wrap svg').count()).toBe(5);

    // Critical: no "Cannot read properties of undefined (reading 'x')".
    const errors = ide.devErrors().filter(NON_FATAL);
    const xCrash = errors.find((e) => /reading 'x'/.test(e));
    expect(xCrash).toBeUndefined();
    expect(errors).toEqual([]);
  });

  test('5-cell subplot → close all → 1-cell plot — no crash (shrink)', async () => {
    // Mirror in reverse: shape shrinks. Stale viewports beyond new length
    // shouldn't crash either.
    await ide.runScript(
      'import compat.*;\n'
      + 'figure;\n'
      + 'subplot(2,3,1); plot([1 2 3], [1 4 9]);\n'
      + 'subplot(2,3,2); plot([1 2 3], [9 4 1]);\n'
      + 'subplot(2,3,3); plot([1 2 3], [1 1 1]);\n'
      + 'subplot(2,3,5); plot([1 2 3], [1 2 3]);\n'
      + 'subplot(2,3,6); plot([1 2 3], [3 2 1]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });

    await ide.figureCloseAll.click();
    await expect(ide.figureCards).toHaveCount(0, { timeout: 5_000 });
    await page.waitForTimeout(100);

    await ide.runScript(
      'import compat.*;\n'
      + 'figure;\n'
      + 'plot([1 2 3 4], [1 4 9 16]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    await page.waitForTimeout(200);

    expect(ide.devErrors().filter(NON_FATAL)).toEqual([]);
  });
});
