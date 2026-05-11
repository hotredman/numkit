// imshow-pan-direction.spec.js — pan must follow mouse on yDir='reverse'.
//
// Bug: dragging mouse DOWN on an imshow figure made the image visually
// move UP (pan went the wrong way for axes with yDir='reverse'). Same
// applies to imagesc (which auto-sets axis ij = yDir='reverse') and
// to any explicit ydir('reverse') / xdir('reverse').
//
// Fix sketch (CompositePlot.jsx onMouseMove): invert the raw screen-
// fraction when xDir/yDir is 'reverse', so the existing `viewport.y +=
// dy` math goes the right way without a per-branch tweak.
//
// We assert direct manipulation: dragging the canvas DOWN moves
// viewport.y to LOWER data values (yMin AND yMax both decrease). The
// .fw-range-row inputs are the user-visible source of truth for the
// current viewport — read those.

import { test, expect } from '@playwright/test';
import { launchIde, closeIde } from '../helpers/launch.js';
import { IdePage } from '../helpers/ide.js';

const NON_FATAL = (e) =>
  !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e);

async function readRange(page) {
  // .fw-range-row layout (non-3D, non-polar): X label, x[0], '→', x[1],
  // Y label, y[0], '→', y[1] — 4 inputs total in DOM order.
  const inputs = page.locator('.fw-range-row input');
  await expect(inputs).toHaveCount(4);
  return {
    xMin: parseFloat(await inputs.nth(0).inputValue()),
    xMax: parseFloat(await inputs.nth(1).inputValue()),
    yMin: parseFloat(await inputs.nth(2).inputValue()),
    yMax: parseFloat(await inputs.nth(3).inputValue()),
  };
}

test.describe('Imshow pan direction', () => {
  let app, page, ide;

  test.beforeEach(async () => {
    app = await launchIde();
    page = await app.firstWindow();
    ide = new IdePage(page);
    await ide.waitForReady();
  });

  test.afterEach(async () => { await closeIde(app); });

  test('imshow + drag DOWN → yMin DECREASES (image follows mouse)', async () => {
    // 8×8 grayscale, normalised to [0,1]. imshow auto-sets yDir='reverse'.
    await ide.runScript(
      'import compat.*;\n'
      + 'imshow(reshape(0:63, 8, 8) / 63);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });

    // The plot SVG inside the figure window.
    const svg = page.locator('.fw-canvas-wrap svg').first();
    await expect(svg).toBeVisible({ timeout: 5_000 });
    await page.waitForTimeout(150);  // viewport state settles

    const before = await readRange(page);
    // Sanity: imshow on 8×8 → yRange ≈ [0.5, 8.5].
    expect(before.yMin).toBeCloseTo(0.5, 1);
    expect(before.yMax).toBeCloseTo(8.5, 1);

    const box = await svg.boundingBox();
    const cx = box.x + box.width / 2;
    const cy = box.y + box.height / 2;

    // Drag DOWN by 80 pixels. With direct manipulation on yDir='reverse',
    // the viewport.y must shift to LOWER values (image content follows
    // the mouse cursor downward).
    await page.mouse.move(cx, cy);
    await page.mouse.down();
    await page.mouse.move(cx, cy + 80, { steps: 8 });
    await page.mouse.up();
    await page.waitForTimeout(200);

    const after = await readRange(page);

    // Assertion: yMin and yMax both decreased.
    expect(after.yMin).toBeLessThan(before.yMin);
    expect(after.yMax).toBeLessThan(before.yMax);
    // Width of viewport unchanged (pan, not zoom).
    expect(after.yMax - after.yMin).toBeCloseTo(before.yMax - before.yMin, 3);

    expect(ide.devErrors().filter(NON_FATAL)).toEqual([]);
  });

  test('plot (yDir normal) + drag DOWN → yMin INCREASES (control case)', async () => {
    // Sanity check that the fix doesn't regress non-reverse axes. For
    // yDir='normal' (Y up, mathematical convention), drag DOWN means
    // viewport center moves UP in data: yMin/yMax both increase.
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([0 1 2 3 4], [0 1 4 9 16]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });

    const svg = page.locator('.fw-canvas-wrap svg').first();
    await expect(svg).toBeVisible({ timeout: 5_000 });
    await page.waitForTimeout(150);

    const before = await readRange(page);
    const box = await svg.boundingBox();
    const cx = box.x + box.width / 2;
    const cy = box.y + box.height / 2;

    await page.mouse.move(cx, cy);
    await page.mouse.down();
    await page.mouse.move(cx, cy + 80, { steps: 8 });
    await page.mouse.up();
    await page.waitForTimeout(200);

    const after = await readRange(page);

    expect(after.yMin).toBeGreaterThan(before.yMin);
    expect(after.yMax).toBeGreaterThan(before.yMax);

    expect(ide.devErrors().filter(NON_FATAL)).toEqual([]);
  });

  test('imagesc (auto axis ij) + drag DOWN → yMin DECREASES', async () => {
    // imagesc must auto-set yDir='reverse' (MATLAB axis ij convention).
    // With both engine + IDE fixes in place, pan must follow the mouse
    // same as imshow.
    await ide.runScript(
      'import compat.*;\n'
      + 'imagesc(reshape(1:64, 8, 8));\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });

    const svg = page.locator('.fw-canvas-wrap svg').first();
    await expect(svg).toBeVisible({ timeout: 5_000 });
    await page.waitForTimeout(150);

    const before = await readRange(page);
    expect(before.yMin).toBeCloseTo(0.5, 1);
    expect(before.yMax).toBeCloseTo(8.5, 1);

    const box = await svg.boundingBox();
    const cx = box.x + box.width / 2;
    const cy = box.y + box.height / 2;

    await page.mouse.move(cx, cy);
    await page.mouse.down();
    await page.mouse.move(cx, cy + 80, { steps: 8 });
    await page.mouse.up();
    await page.waitForTimeout(200);

    const after = await readRange(page);
    expect(after.yMin).toBeLessThan(before.yMin);
    expect(after.yMax).toBeLessThan(before.yMax);

    expect(ide.devErrors().filter(NON_FATAL)).toEqual([]);
  });
});
