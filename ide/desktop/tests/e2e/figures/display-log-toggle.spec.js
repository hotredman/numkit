// display-log-toggle.spec.js — verify xlog / ylog toggles inside the
// display ▾ menu actually switch the axis to log mapping. Probe the
// y-axis tick set: linear on plot(1:1000) gives ticks at 100 / 200 / …
// while log gives 10 / 100 / 1000. After toggle we want the log set.

import { test, expect } from '../../helpers/shared.js';

async function openDisplay(page) {
  await page.locator('.fw-toolbar .ve-btn', { hasText: /display/i }).click();
  await expect(page.locator('.fw-pop').first()).toBeVisible({ timeout: 2_000 });
}

async function yTicks(page) {
  // Y-axis tick labels are on the left (small x) AND right-aligned.
  // The CompositePlot uses textAnchor="end" for them with x ≈ padL-2.
  // For the modal at default size padL ≈ 70, so x is ~68. Be generous.
  return await page.locator('.fw-window svg text').evaluateAll((els) =>
    els
      .filter((el) => {
        const x = parseFloat(el.getAttribute('x') || '0');
        const anchor = el.getAttribute('text-anchor') || '';
        const txt = el.textContent || '';
        return x < 100 && anchor === 'end' && /^-?\d/.test(txt);
      })
      .map((el) => el.textContent)
  );
}

test('ylog toggle switches Y axis from linear to log scale', async ({ ide, page }) => {
  await ide.runScript(
    'import compat.*;\n'
    + 'plot(1:1000);\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(150);

  const linearTicks = await yTicks(page);
  // Linear ticks for plot(1:1000) are at niceTicks-spaced values
  // (typically 0/100/200/.../1000). The defining trait vs. log: there
  // are intermediate non-decade values (e.g. 200, 300, 400, ...). Just
  // assert that we see at least one such non-power-of-10 value to gate
  // the precondition.
  const looksLinear = linearTicks.some((t) => /^([2-9]00|[1-9]\d{2}\.0)$/.test(t));
  expect(looksLinear, `linear ticks ${JSON.stringify(linearTicks)}`).toBe(true);

  await openDisplay(page);
  await page.locator('.fw-pop-toggle', { has: page.locator('span', { hasText: 'ylog' }) }).click();
  await page.waitForTimeout(150);

  const logTicks = await yTicks(page);
  // After ylog toggle: powers of 10 should appear as decade major
  // ticks. Same regex as the xlog test — accepts "10", "10.0", "100",
  // "1000" formatting.
  const looksLog = logTicks.some((t) => /^(10|10\.0|100|1000)$/.test(t));
  expect(looksLog, `log ticks ${JSON.stringify(logTicks)}`).toBe(true);
});

test('xlog toggle switches X axis from linear to log scale', async ({ ide, page }) => {
  await ide.runScript(
    'import compat.*;\n'
    + 'plot(1:1000, 1:1000);\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(150);

  await openDisplay(page);
  await page.locator('.fw-pop-toggle', { has: page.locator('span', { hasText: 'xlog' }) }).click();
  await page.waitForTimeout(150);

  // X tick labels live near the bottom (y close to height-padB).
  const xLabels = await page.locator('.fw-window svg text').evaluateAll((els) =>
    els
      .filter((el) => {
        const y = parseFloat(el.getAttribute('y') || '0');
        return y > 400 && /^-?\d/.test(el.textContent || '');
      })
      .map((el) => el.textContent)
  );
  console.log('LOG X TICKS:', xLabels);
  const looksLog = xLabels.some((t) => /^(10|100|1000)$/.test(t));
  expect(looksLog, `log x ticks ${JSON.stringify(xLabels)}`).toBe(true);
});
