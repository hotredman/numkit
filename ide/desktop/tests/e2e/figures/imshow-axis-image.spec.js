// imshow-axis-image.spec.js — verify that imshow renders with a 1:1
// data aspect (square pixels) in BOTH the preview card and the modal,
// matching MATLAB `axis image` behaviour. Without the fix the image
// stretches to fill its panel, so a 64×64 image looked rectangular and
// the visual aspect differed between preview (1.7 wide) and modal
// (panel-aspect dependent).

import { test, expect } from '../../helpers/shared.js';

async function imageAspect(locator) {
  const box = await locator.first().boundingBox();
  if (!box || !box.width || !box.height) return null;
  return box.width / box.height;
}

test.describe('imshow axis image — preview/modal pixel-equivalent', () => {
  test('64×64 image renders square in preview AND modal', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + '[X, Y] = meshgrid(linspace(0, 1, 64));\n'
      + 'I = 0.5 * (X + Y);\n'
      + 'imshow(I);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await page.waitForTimeout(300);

    // PREVIEW: <image> inside the card should be square (or nearly so)
    // because data aspect is 1:1 (xRange ≈ yRange).
    const previewImg = page.locator('.fp-card svg image');
    await expect(previewImg).toHaveCount(1, { timeout: 5_000 });
    const previewAspect = await imageAspect(previewImg);
    expect(previewAspect, `preview image aspect ${previewAspect} ≠ 1`)
      .toBeGreaterThan(0.9);
    expect(previewAspect).toBeLessThan(1.1);

    // MODAL: open + same check.
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    await page.waitForTimeout(300);
    const modalImg = page.locator('.fw-window svg image');
    const modalAspect = await imageAspect(modalImg);
    expect(modalAspect, `modal image aspect ${modalAspect} ≠ 1`)
      .toBeGreaterThan(0.9);
    expect(modalAspect).toBeLessThan(1.1);
  });

  test('non-square image keeps data aspect across preview/modal', async ({ ide, page }) => {
    // 32 rows × 64 cols → data aspect 64/32 = 2.0 (wider than tall).
    await ide.runScript(
      'import compat.*;\n'
      + '[X, Y] = meshgrid(linspace(0, 1, 64), linspace(0, 1, 32));\n'
      + 'I = X + Y;\n'
      + 'imshow(I);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await page.waitForTimeout(300);

    const previewImg = page.locator('.fp-card svg image');
    const previewAspect = await imageAspect(previewImg);
    expect(previewAspect, `preview aspect ${previewAspect} should ≈ 2.0`)
      .toBeGreaterThan(1.7);
    expect(previewAspect).toBeLessThan(2.3);

    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    await page.waitForTimeout(300);
    const modalImg = page.locator('.fw-window svg image');
    const modalAspect = await imageAspect(modalImg);
    expect(modalAspect, `modal aspect ${modalAspect} should ≈ 2.0`)
      .toBeGreaterThan(1.7);
    expect(modalAspect).toBeLessThan(2.3);
  });

  test('subplot imshow letterbox is centred horizontally + vertically', async ({ ide, page }) => {
    // 2x2 grid of squares — every cell will letterbox along whichever
    // side is "too long" relative to the image's 1:1 aspect.
    await ide.runScript(
      'import compat.*;\n'
      + '[X, Y] = meshgrid(linspace(0, 1, 64));\n'
      + 'I = X + Y;\n'
      + 'figure;\n'
      + 'subplot(2,2,1); imshow(I);\n'
      + 'subplot(2,2,2); imshow(I);\n'
      + 'subplot(2,2,3); imshow(I);\n'
      + 'subplot(2,2,4); imshow(I);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    await page.waitForTimeout(400);

    // For each <image>, find its containing SVG and assert the image is
    // centred — left + right margins within ~3 px of each other, same
    // for top + bottom.
    const offsets = await page.locator('.fw-window svg image').evaluateAll((els) => {
      return els.map((el) => {
        const ib = el.getBoundingClientRect();
        const sb = el.ownerSVGElement.getBoundingClientRect();
        return {
          left:   ib.left   - sb.left,
          right:  sb.right  - ib.right,
          top:    ib.top    - sb.top,
          bottom: sb.bottom - ib.bottom,
        };
      });
    });
    // Heatmap renderer emits >1 <image> per cell (tile + overlay), so
    // length is N * cellCount. Assert centring for every one.
    expect(offsets.length).toBeGreaterThanOrEqual(4);
    for (const o of offsets) {
      expect(Math.abs(o.left - o.right), `H off-centre: ${JSON.stringify(o)}`).toBeLessThan(4);
      expect(Math.abs(o.top - o.bottom), `V off-centre: ${JSON.stringify(o)}`).toBeLessThan(4);
    }
  });
});
