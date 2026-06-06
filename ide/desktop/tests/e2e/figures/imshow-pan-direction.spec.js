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

import { test, expect } from '../../helpers/shared.js';

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
  test('imshow + drag DOWN → yMin DECREASES (image follows mouse)', async ({ ide, page }) => {
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

  test('plot (yDir normal) + drag DOWN → yMin INCREASES (control case)', async ({ ide, page }) => {
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

  test('subplot + imshow (morphology_pipeline shape) + drag DOWN', async ({ ide, page }) => {
    // Mirrors examples/Image/morphology_pipeline.m: figure with a 2×3
    // subplot grid, imshow in each cell. Verifies pan direction works
    // inside SubplotGrid → CompositePlot, not just on top-level figures.
    // Each subplot cell has its own viewport state; we pan the first cell.
    await ide.runScript(
      'import compat.*;\n'
      + '[X, Y] = meshgrid(1:32, 1:32);\n'
      + 'mask = (X - 16).^2 + (Y - 16).^2 <= 8^2;\n'
      + 'figure;\n'
      + 'subplot(2,3,1); imshow(mask); title(\'a\');\n'
      + 'subplot(2,3,2); imshow(mask); title(\'b\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });

    // Subplot mode hides the .fw-range-row footer (per FigureWindow.jsx
    // !isSubplot gate), so we can't readRange(). Read viewport state
    // straight from the SVG: the image href changes on viewport change,
    // BUT a more reliable signal is the y-tick label text. We inspect
    // the SVG bounding box of the FIRST cell's plot area and pan it.
    const cellSvgs = page.locator('.fw-canvas-wrap svg');
    const count = await cellSvgs.count();
    expect(count).toBeGreaterThanOrEqual(2);  // at least 2 subplot cells

    const firstCell = cellSvgs.first();
    await expect(firstCell).toBeVisible({ timeout: 5_000 });
    await page.waitForTimeout(150);

    // Read the visible image's y attribute — that's the screen-space top
    // of the SVG <image> element drawn by CompositePlot. With pan, this
    // value SHIFTS DOWN (larger y) when the user drags the mouse down on
    // a yDir='reverse' axis (because the image follows the mouse).
    const imgY = async () => {
      const v = await firstCell.locator('image').first().getAttribute('y');
      return parseFloat(v);
    };
    const yBefore = await imgY();

    const box = await firstCell.boundingBox();
    const cx = box.x + box.width / 2;
    const cy = box.y + box.height / 2;

    await page.mouse.move(cx, cy);
    await page.mouse.down();
    await page.mouse.move(cx, cy + 60, { steps: 8 });
    await page.mouse.up();
    await page.waitForTimeout(250);

    const yAfter = await imgY();

    // Image moved DOWN visually (larger SVG y) → direct manipulation
    // works inside subplot cells too. Without the pan-fix this would
    // have decreased (image went UP, opposite of mouse).
    expect(yAfter).toBeGreaterThan(yBefore);

    expect(ide.devErrors().filter(NON_FATAL)).toEqual([]);
  });

  test('subplot + imshow: image rect stable before/after tile-fetch settles', async ({ ide, page }) => {
    // The bug being chased: drag DOWN on imshow inside subplot, image
    // visually moves correctly DURING drag. On mouse-up the engine's
    // display-tile fetch fires (~60ms debounce) and replaces the inline
    // preview with a re-resampled tile. If the tile renderer disagrees
    // with the inline preview about y-orientation (yDir='reverse'), the
    // visible image area "snaps" to a different rectangle on release —
    // user perception: "clipping after release".
    //
    // We assert the visible <image> bounding rect (intersection with the
    // panel clip) is the SAME just-after-release vs. after-tile-settles.
    await ide.runScript(
      'import compat.*;\n'
      + '[X, Y] = meshgrid(1:32, 1:32);\n'
      + 'mask = (X - 16).^2 + (Y - 16).^2 <= 8^2;\n'
      + 'mask(1:4, 1:4) = true;\n'   // distinctive marker at TOP-LEFT
      + 'figure;\n'
      + 'subplot(2,3,1); imshow(mask); title(\'a\');\n'
      + 'subplot(2,3,2); imshow(mask); title(\'b\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });

    const cellSvg = page.locator('.fw-canvas-wrap svg').first();
    await expect(cellSvg).toBeVisible({ timeout: 5_000 });
    await page.waitForTimeout(150);

    // Snapshot every <image> element's rect before pan.
    async function snapImages() {
      const imgs = cellSvg.locator('image');
      const n = await imgs.count();
      const out = [];
      for (let i = 0; i < n; i++) {
        const el = imgs.nth(i);
        out.push({
          y: parseFloat(await el.getAttribute('y')),
          h: parseFloat(await el.getAttribute('height')),
        });
      }
      return out;
    }

    const box = await cellSvg.boundingBox();
    const cx = box.x + box.width / 2;
    const cy = box.y + box.height / 2;

    await page.mouse.move(cx, cy);
    await page.mouse.down();
    await page.mouse.move(cx, cy + 60, { steps: 8 });
    // RIGHT-after-release snapshot (tile-fetch hasn't fired yet — debounce
    // is 60ms; we wait less than that). Inline preview only.
    await page.mouse.up();
    await page.waitForTimeout(20);
    const justAfterRelease = await snapImages();

    // Wait long enough for the tile-fetch to settle.
    await page.waitForTimeout(400);
    const afterSettle = await snapImages();

    // Tile placement (oy) must be CONSISTENT before vs after settle —
    // that's the actual orientation bug. Tile HEIGHT can shrink when
    // the new tile is sized to in-bounds source data only (legit "pan
    // past data" — empty area above/below image, MATLAB-equivalent).
    // What we forbid: tile y attribute jumping (= image flips orientation
    // or relocates).
    expect(justAfterRelease.length).toBe(afterSettle.length);
    for (let i = 0; i < justAfterRelease.length; i++) {
      expect(Math.abs(afterSettle[i].y - justAfterRelease[i].y))
        .toBeLessThan(5);  // sub-pixel tolerance
    }

    expect(ide.devErrors().filter(NON_FATAL)).toEqual([]);
  });

  test('imagesc (auto axis ij) + drag DOWN → yMin DECREASES', async ({ ide, page }) => {
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
