// imshow-preview-orientation.spec.js — preview card must match modal.
//
// Bug: imshow / imagesc inline-preview path (renderHeatmapDataURLFromIndices)
// applied an unconditional vertical flip. For axis-ij (yDir='reverse') this
// flipped the image vertically — visible in the FiguresPane preview card
// (which only renders the inline preview). The modal window also rendered
// the flipped inline preview, but the post-pan tile overlay (correctly
// non-flipped after a prior fix) covered it on top. So preview and modal
// disagreed on orientation for non-panned imshow / imagesc.
//
// The fix makes the inline-preview flip conditional on yDir, matching the
// tile overlay. We assert by comparing canvas-pixel data sampled from both
// the preview <image> element and the modal <image> element.

import { test, expect } from '../helpers/shared.js';

const NON_FATAL = (e) =>
  !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e);

// Sample the top half AVG vs bottom half AVG of an image element's data
// URL. Returns { topAvg, botAvg } as 0..255 grayscale (R channel).
async function sampleHalves(page, imageLocator) {
  const href = await imageLocator.getAttribute('href');
  return await page.evaluate((url) => new Promise((resolve) => {
    const img = new Image();
    img.onload = () => {
      const cv = document.createElement('canvas');
      cv.width = img.width; cv.height = img.height;
      const ctx = cv.getContext('2d');
      ctx.drawImage(img, 0, 0);
      const data = ctx.getImageData(0, 0, img.width, img.height).data;
      const half = Math.floor(img.height / 2);
      let topSum = 0, topN = 0, botSum = 0, botN = 0;
      for (let y = 0; y < img.height; y++) {
        for (let x = 0; x < img.width; x++) {
          const off = (y * img.width + x) * 4;
          if (y < half) { topSum += data[off]; topN++; }
          else          { botSum += data[off]; botN++; }
        }
      }
      resolve({
        topAvg: topN ? topSum / topN : 0,
        botAvg: botN ? botSum / botN : 0,
      });
    };
    img.src = url;
  }), href);
}

test.describe('Imshow preview orientation matches modal', () => {
  test('imshow: preview card and modal show same vertical orientation', async ({ ide, page }) => {
    // Build an asymmetric mask: top half = white, bottom half = black.
    // After yDir='reverse' rendering, top of canvas should be the WHITE
    // half (matrix row 1..N/2 = top of matrix). Both preview and modal
    // must agree.
    await ide.runScript(
      'import compat.*;\n'
      + 'mask = false(32, 32);\n'
      + 'mask(1:16, :) = true;\n'   // top half (rows 1..16) white
      + 'imshow(mask);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });

    // Sample the preview <image> in the figure card (.fp-card).
    const previewImg = page.locator('.fp-card image').first();
    await expect(previewImg).toBeVisible({ timeout: 5_000 });
    await page.waitForTimeout(150);
    const previewSample = await sampleHalves(page, previewImg);

    // Open modal, sample its inline preview <image>.
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    await page.waitForTimeout(150);
    const modalImg = page.locator('.fw-canvas-wrap image').first();
    await expect(modalImg).toBeVisible({ timeout: 5_000 });
    const modalSample = await sampleHalves(page, modalImg);

    // For yDir='reverse' (imshow default), matrix row 1..16 (white) at TOP,
    // rows 17..32 (black) at BOTTOM. So topAvg ≫ botAvg.
    expect(previewSample.topAvg).toBeGreaterThan(previewSample.botAvg + 50);
    expect(modalSample.topAvg).toBeGreaterThan(modalSample.botAvg + 50);

    // Preview and modal agree on which half is brighter.
    expect(Math.sign(previewSample.topAvg - previewSample.botAvg))
      .toBe(Math.sign(modalSample.topAvg - modalSample.botAvg));

    expect(ide.devErrors().filter(NON_FATAL)).toEqual([]);
  });

  test('plot: y-up axis preview unchanged (no regression)', async ({ ide }) => {
    // Sanity that plot (no heatmap) preview still renders without errors.
    await ide.runScript(
      'import compat.*;\n'
      + 'plot([1 2 3 4], [1 4 9 16]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter(NON_FATAL)).toEqual([]);
  });
});
