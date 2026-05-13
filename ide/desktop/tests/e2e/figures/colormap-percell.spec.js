// colormap-percell.spec.js — toolbar Colormap ▾ applies to ALL cells;
// ПКМ Colormap inside one cell affects ONLY that cell. Toolbar pick
// drops every per-cell override (toolbar wins).

import { test, expect } from '../../helpers/shared.js';

async function rightClickCell(page, idx) {
  const svg = page.locator('.fw-window .fw-canvas-wrap svg').nth(idx);
  const box = await svg.boundingBox();
  await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
  await page.mouse.down({ button: 'right' });
  await page.mouse.up({ button: 'right' });
  await expect(page.locator('.ctx-menu')).toBeVisible({ timeout: 2_000 });
}

// Probe each cell's first heatmap layer's effective colormap via React
// fiber. Returns ['parula', 'jet', ...] one per cell.
async function cellColormaps(page) {
  return await page.evaluate(() => {
    const card = document.querySelector('.fp-card');
    if (!card) return [];
    const fk = Object.keys(card).find((k) => k.startsWith('__reactFiber'));
    let f = card[fk];
    while (f && !(f.memoizedProps && f.memoizedProps.figure)) f = f.return;
    if (!f) return [];
    const fig = f.memoizedProps.figure;
    if (!Array.isArray(fig.cells)) return [];
    return fig.cells.map((c) => {
      const hm = (c.layers || []).find((l) => l.kind === 'heatmap');
      return hm ? (hm.colormap || 'parula') : null;
    });
  });
}

// Effective colormap: tile <image> hrefs are dataURLs encoding the LUT,
// so we can't tell from the DOM alone. Instead, count cells whose
// CompositePlot received which colormapOverride prop via fiber walk.
async function cellEffectiveCmaps(page) {
  return await page.evaluate(() => {
    const wraps = document.querySelectorAll('.fw-window .fw-canvas-wrap > div > div > div > div');
    const out = [];
    document.querySelectorAll('.fw-window .fw-canvas-wrap svg').forEach((svg) => {
      // Walk up to find CompositePlot fiber.
      let el = svg;
      let fib = null;
      while (el && !fib) {
        const k = Object.keys(el).find((kk) => kk.startsWith('__reactFiber'));
        if (k) {
          let f = el[k];
          while (f && !(f.memoizedProps && f.memoizedProps.figure
                        && Array.isArray(f.memoizedProps.figure.layers))) {
            f = f.return;
          }
          if (f) fib = f;
        }
        el = el.parentElement;
      }
      if (fib) {
        const fig = fib.memoizedProps.figure;
        const hm = (fig.layers || []).find((l) => l.kind === 'heatmap');
        const override = fib.memoizedProps.colormapOverride;
        out.push(override || (hm ? (hm.colormap || 'parula') : null));
      } else {
        out.push(null);
      }
    });
    return out;
  });
}

test('ПКМ Colormap in cell A changes only cell A', async ({ ide, page }) => {
  await ide.runScript(
    'import compat.*;\n'
    + 'figure;\n'
    + 'subplot(1,2,1); imagesc(rand(8,8));\n'
    + 'subplot(1,2,2); imagesc(rand(8,8));\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(150);

  // Pre: both cells use script default (parula).
  const before = await cellEffectiveCmaps(page);
  expect(before[0]).toBe('parula');
  expect(before[1]).toBe('parula');

  // Right-click cell A → Colormap → jet.
  await rightClickCell(page, 0);
  await page.locator('.ctx-sub-trigger', { hasText: /Colormap/ }).hover();
  await page.waitForTimeout(60);
  await page.locator('.ctx-submenu button', { hasText: /^(✓ )?jet$/ }).click();
  await page.waitForTimeout(150);

  const after = await cellEffectiveCmaps(page);
  expect(after[0], `cellA cmap after pick: ${after[0]}`).toBe('jet');
  expect(after[1], `cellB cmap must stay parula, got: ${after[1]}`).toBe('parula');
});

test('toolbar Colormap ▾ applies to ALL cells, dropping per-cell picks', async ({ ide, page }) => {
  await ide.runScript(
    'import compat.*;\n'
    + 'figure;\n'
    + 'subplot(1,2,1); imagesc(rand(8,8));\n'
    + 'subplot(1,2,2); imagesc(rand(8,8));\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(150);

  // First put cell A on jet (per-cell override).
  await rightClickCell(page, 0);
  await page.locator('.ctx-sub-trigger', { hasText: /Colormap/ }).hover();
  await page.waitForTimeout(60);
  await page.locator('.ctx-submenu button', { hasText: /^(✓ )?jet$/ }).click();
  await page.waitForTimeout(120);

  // Now toolbar Colormap ▾ → hot.
  const toolbarCmap = page.locator('.fw-toolbar .ve-btn', { hasText: /^colormap/i });
  await expect(toolbarCmap).toBeVisible();
  await toolbarCmap.click();
  await expect(page.locator('.fw-pop').first()).toBeVisible({ timeout: 2_000 });
  await page.locator('.fw-pop button', { hasText: /^hot$/ }).click();
  await page.waitForTimeout(150);

  const after = await cellEffectiveCmaps(page);
  expect(after[0], `cellA after toolbar: ${after[0]}`).toBe('hot');
  expect(after[1], `cellB after toolbar: ${after[1]}`).toBe('hot');
});
