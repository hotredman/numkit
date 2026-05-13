// fit-many-series.spec.js — when a script legitimately creates many
// series (e.g. delaunay for-loop, or any loop calling plot() per
// iteration), the fit ▾ menu must list ALL of them. Earlier we capped
// the per-series rows at 8 with a "N series — per-series fit hidden"
// note; that was wrong UX — the outer .fw-pop already scrolls, so
// every row stays accessible.

import { test, expect } from '../../helpers/shared.js';

test('fit ▾ lists every series when there are many', async ({ ide, page }) => {
  // 12 separate plot() calls → 12 series. Loop in M.
  await ide.runScript(
    'import compat.*;\n'
    + 'figure;\n'
    + 'hold on;\n'
    + 'for k = 1:12\n'
    + '  plot([0 1], [k k], \'b-\');\n'
    + 'end\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(150);

  // Open fit ▾.
  await page.locator('.fw-toolbar .ve-btn', { hasText: /fit/i }).click();
  await expect(page.locator('.fw-pop').first()).toBeVisible({ timeout: 2_000 });

  // Every series row carries a name + xy/x/y buttons. Count rows.
  const rows = page.locator('.fw-pop-row');
  await expect(rows).toHaveCount(12);

  // The "N series — per-series fit hidden" note must NOT be present.
  await expect(page.locator('.fw-pop-note')).toHaveCount(0);

  // Last row should be clickable (proves outer scroll exposes it).
  await rows.last().locator('button', { hasText: /^xy$/ }).click();
  // popover closes on action — no error.
});
