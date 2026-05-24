// preview-subplot-grid.spec.js — preview card for a subplot figure
// must show grid lines when the script said `grid on`. Bug: SubplotGrid
// fed by FiguresPane (no cellState passed) used to render every cell
// with grid=false because s.showMajor was undefined and the fallback
// path didn't read cell.grid.

import { test, expect } from '../../helpers/shared.js';

test('subplot preview shows script-set grid in every cell', async ({ ide, page }) => {
  await ide.runScript(
    'import compat.*;\n'
    + 'figure;\n'
    + 'subplot(1,2,1); plot(1:10); grid on;\n'
    + 'subplot(1,2,2); plot(1:10); grid on;\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await page.waitForTimeout(200);

  // Count grid lines inside the preview card (NOT the modal).
  const previewSvgs = page.locator('.fp-card svg');
  await expect(previewSvgs).toHaveCount(2, { timeout: 5_000 });
  const gridCounts = await previewSvgs.evaluateAll((els) =>
    els.map((svg) => svg.querySelectorAll('line[stroke*="--plot-grid"]:not([stroke*="--plot-grid-min"])').length)
  );
  expect(gridCounts[0], `cell A grid lines in preview: ${gridCounts[0]}`).toBeGreaterThan(0);
  expect(gridCounts[1], `cell B grid lines in preview: ${gridCounts[1]}`).toBeGreaterThan(0);
});
