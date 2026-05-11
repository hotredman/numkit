// graphics-output-check.spec.js — diagnostic spec that exercises the
// "draws something" path of functions whose existing tests are purely
// numerical. Opens the figure modal and snapshots — failures attach
// the screenshot so we can see WHAT (if anything) was rendered.

import { test, expect } from '../../helpers/shared.js';

const NON_FATAL = (e) =>
  !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e);

async function runAndOpen(ide, page, script) {
  await ide.runScript(script);
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(400);  // tile-fetch / 3-D first frame
}

async function snapshot(page, name) {
  await page.screenshot({
    path: `test-results/_diag/${name}.png`,
    fullPage: false,
  });
}

test.describe('graphics output — visual smoke', () => {
  test('histogram2 yDir is normal (axis xy, MATLAB parity)', async ({ ide, page }) => {
    await runAndOpen(ide, page,
      'import compat.*;\n'
      + 'rng(7);\n'
      + 'x = randn(100, 1); y = randn(100, 1);\n'
      + 'histogram2(x, y);\n'
    );
    // Find the figure JSON via the engine, inspect cfg.yDir.
    const yDir = await page.evaluate(() => {
      const figs = window.__numkit_lastFigure
        || (window.numkit && window.numkit.figures);
      // Fallback: look at React props attached to the SVG.
      const svg = document.querySelector('.fw-canvas-wrap svg');
      // Last resort — inspect the y-tick label order: small label at top
      // vs bottom indicates ij vs xy.
      const ticks = [...document.querySelectorAll('.fw-canvas-wrap text')]
        .map((t) => ({ y: t.getAttribute('y'), txt: t.textContent }))
        .filter((t) => /^-?\d/.test(t.txt))
        .sort((a, b) => Number(a.y) - Number(b.y));
      return { ticks: ticks.slice(0, 6), figs: !!figs, hasSvg: !!svg };
    });
    console.log('histogram2 ticks (top to bottom):', JSON.stringify(yDir.ticks, null, 2));
    expect(ide.devErrors().filter(NON_FATAL)).toEqual([]);
  });

  test('histogram2 — 2-D histogram heatmap renders', async ({ ide, page }) => {
    await runAndOpen(ide, page,
      'import compat.*;\n'
      + 'rng(42);\n'
      + 'x = randn(1000, 1);\n'
      + 'y = randn(1000, 1);\n'
      + 'histogram2(x, y);\n'
    );
    await snapshot(page, 'histogram2');
    // Heatmap: at least one <image> element inside the figure window.
    const images = page.locator('.fw-window .fw-canvas-wrap image');
    expect(await images.count(), 'no <image> in histogram2 figure').toBeGreaterThanOrEqual(1);
    expect(ide.devErrors().filter(NON_FATAL)).toEqual([]);
  });

  test('convhull (no output) — should plot the hull polygon', async ({ ide, page }) => {
    // MATLAB: convhull called WITHOUT a return value plots the hull.
    // numkit may or may not honour this; document the actual behaviour.
    await runAndOpen(ide, page,
      'import compat.*;\n'
      + 'x = [0 1 1 0 0.5];\n'
      + 'y = [0 0 1 1 0.5];\n'
      + 'convhull(x, y);\n'
    );
    // We expect SOMETHING to render. Closed polygon = a <path> element.
    const paths = page.locator('.fw-window .fw-canvas-wrap path');
    expect(await paths.count(), 'convhull(x,y) produced no path/line').toBeGreaterThanOrEqual(1);
    expect(ide.devErrors().filter(NON_FATAL)).toEqual([]);
  });

  test('inpolygon (visualised) — points coloured by inside/outside', async ({ ide, page }) => {
    // MATLAB pattern:
    //   plot(xv, yv); hold on; plot(xq(in), yq(in), 'r+'); plot(xq(~in), yq(~in), 'bo')
    await runAndOpen(ide, page,
      'import compat.*;\n'
      + 'xv = [0 1 1 0]; yv = [0 0 1 1];\n'
      + 'xq = rand(50, 1); yq = rand(50, 1);\n'
      + 'in = inpolygon(xq, yq, xv, yv);\n'
      + 'plot(xv, yv);\n'
      + 'hold on;\n'
      + 'plot(xq(in), yq(in), \'r+\');\n'
      + 'plot(xq(~in), yq(~in), \'bo\');\n'
    );
    await snapshot(page, 'inpolygon-viz');
    // Markers + polygon → multiple line/path layers expected.
    const paths = page.locator('.fw-window .fw-canvas-wrap path');
    expect(await paths.count(), 'inpolygon viz produced no plot layers').toBeGreaterThanOrEqual(1);
    expect(ide.devErrors().filter(NON_FATAL)).toEqual([]);
  });

  test('coneplot — 3-D cone field renders WebGL canvas', async ({ ide, page }) => {
    await runAndOpen(ide, page,
      'import compat.*;\n'
      + '[X, Y, Z] = meshgrid(-2:1:2, -2:1:2, -2:1:2);\n'
      + 'U = ones(size(X)); V = zeros(size(X)); W = zeros(size(X));\n'
      + 'coneplot(X, Y, Z, U, V, W);\n'
    );
    await snapshot(page, 'coneplot');
    const canvas = page.locator('.fw-window canvas[data-numkit-3d]');
    await expect(canvas, 'coneplot did not mount a 3-D WebGL canvas')
      .toBeVisible({ timeout: 5_000 });
    // Read back canvas pixel data — if all transparent or all bg-color,
    // there's no actual geometry being drawn (the bug pattern).
    const nonBgPixels = await canvas.first().evaluate((cv) => {
      const ctx = cv.getContext('webgl2') || cv.getContext('webgl');
      if (!ctx) return -1;
      const w = cv.width, h = cv.height;
      const px = new Uint8Array(w * h * 4);
      ctx.readPixels(0, 0, w, h, ctx.RGBA, ctx.UNSIGNED_BYTE, px);
      let nonZero = 0;
      for (let i = 0; i < px.length; i += 4) {
        if (px[i] || px[i+1] || px[i+2] || px[i+3]) nonZero++;
      }
      return nonZero;
    });
    expect(nonBgPixels, `coneplot canvas drew ${nonBgPixels} non-bg pixels`).toBeGreaterThan(100);
    expect(ide.devErrors().filter(NON_FATAL)).toEqual([]);
  });

  test('histcounts2 — auto-plot if no output captured (probe MATLAB parity)', async ({ ide, page }) => {
    // histcounts2 NEVER plots in MATLAB — it's the count-only variant.
    // If a figure appears it would be surprising. Probe behaviour.
    await ide.runScript(
      'import compat.*;\n'
      + 'rng(7);\n'
      + 'x = rand(100, 1); y = rand(100, 1);\n'
      + 'histcounts2(x, y);\n'
    );
    await page.waitForTimeout(300);
    // Document whichever way it goes.
    const cardCount = await ide.figureCards.count();
    console.log(`histcounts2 with no output created ${cardCount} figure card(s)`);
    expect(ide.devErrors().filter(NON_FATAL)).toEqual([]);
  });
});
