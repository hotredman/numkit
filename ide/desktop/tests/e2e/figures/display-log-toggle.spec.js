// display-log-toggle.spec.js — verify the `log scale` toggles inside
// the axes ▾ menu switch the axis to log mapping. Probe the y-axis
// tick set: linear on plot(1:1000) gives ticks at 100 / 200 / …
// while log gives 10 / 100 / 1000. Row labels are single letters
// (`X` / `Y` / `Z`) under the `log scale` section head — section-
// scoped selector because the same letters live in `reverse:` too.

import { test, expect } from '../../helpers/shared.js';

async function openDisplay(page) {
  await page.locator('.fw-toolbar .ve-btn', { hasText: /axes/i }).click();
  await expect(page.locator('.fw-pop').first()).toBeVisible({ timeout: 2_000 });
}

/** Locate a `.fw-pop-toggle` row by (section head, row text). */
function row(page, sectionHead, rowText) {
  return page.locator('.fw-pop-section', {
    has: page.locator('.fw-pop-head', { hasText: new RegExp(`^${sectionHead}$`) }),
  }).locator('.fw-pop-toggle', {
    has: page.locator('span', { hasText: new RegExp(`^${rowText}$`) }),
  });
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
  await row(page, 'log scale', 'Y').click();
  await page.waitForTimeout(150);

  const logTicks = await yTicks(page);
  // After ylog toggle: powers of 10 should appear as decade major
  // ticks. Same regex as the xlog test — accepts "10", "10.0", "100",
  // "1000" formatting.
  const looksLog = logTicks.some((t) => /^(10|10\.0|100|1000)$/.test(t));
  expect(looksLog, `log ticks ${JSON.stringify(logTicks)}`).toBe(true);
});

test('toolbar display ▾ xlog ✓ on subplot_demo (2x2 sin/cos)', async ({ ide, page }) => {
  // Reproduces user-reported bug: subplot_demo.m → click toolbar
  // display ▾ xlog → ✓ wasn't appearing because toggleAxisLog
  // dereferenced viewport.x[0] but FigureWindow.viewport is null for
  // subplot figures (per-cell viewports live in SubplotGrid). The
  // throw aborted the handler before setXLog ran.
  await ide.runScript(
    'import compat.*;\n'
    + 'x = linspace(0, 2*pi, 100);\n'
    + 'figure;\n'
    + 'subplot(2,2,1); plot(x, sin(x));   title(\'sin(x)\');   grid on;\n'
    + 'subplot(2,2,2); plot(x, cos(x));   title(\'cos(x)\');   grid on;\n'
    + 'subplot(2,2,3); plot(x, sin(2*x)); title(\'sin(2x)\');  grid on;\n'
    + 'subplot(2,2,4); plot(x, cos(2*x)); title(\'cos(2x)\');  grid on;\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(150);

  await openDisplay(page);
  const xlogToggle = row(page, 'log scale', 'X');
  expect((await xlogToggle.locator('.fw-pop-check').textContent()).trim()).toBe('');

  await xlogToggle.click();
  await page.waitForTimeout(120);
  expect((await xlogToggle.locator('.fw-pop-check').textContent()).trim()).toBe('✓');
});

test('toolbar display ▾ xlog click sets the ✓ checkmark', async ({ ide, page }) => {
  await ide.runScript('import compat.*;\nplot(1:1000);\n');
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(120);

  await openDisplay(page);
  // Pre: xlog NOT active.
  const xlogToggle = row(page, 'log scale', 'X');
  expect((await xlogToggle.locator('.fw-pop-check').textContent()).trim()).toBe('');

  // Click. Menu stays open. ✓ should appear immediately.
  await xlogToggle.click();
  await page.waitForTimeout(80);
  expect((await xlogToggle.locator('.fw-pop-check').textContent()).trim()).toBe('✓');
});

test('toolbar xlog applies log scale to ALL subplot cells', async ({ ide, page }) => {
  await ide.runScript(
    'import compat.*;\n'
    + 'figure;\n'
    + 'subplot(1,2,1); plot(1:1000);\n'
    + 'subplot(1,2,2); plot(1:1000);\n'
  );
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(150);

  await openDisplay(page);
  await row(page, 'log scale', 'X').click();
  await page.waitForTimeout(200);

  // Both cells should now show log-scale ticks (powers of 10).
  const tickPerCell = await page.locator('.fw-window .fw-canvas-wrap svg').evaluateAll((els) =>
    els.map((svg) => Array.from(svg.querySelectorAll('text'))
      .filter((t) => parseFloat(t.getAttribute('y') || '0') > 200
                  && /^-?\d/.test(t.textContent || ''))
      .map((t) => t.textContent))
  );
  // Each cell's tick set should contain at least one decade label.
  const looksLog = (ticks) => ticks.some((t) => /^(10|10\.0|100|1000|1\.00)$/.test(t));
  expect(tickPerCell[0].length, `cell A ticks empty: ${JSON.stringify(tickPerCell)}`).toBeGreaterThan(0);
  expect(tickPerCell[1].length, `cell B ticks empty: ${JSON.stringify(tickPerCell)}`).toBeGreaterThan(0);
  expect(looksLog(tickPerCell[0]), `cell A not log: ${JSON.stringify(tickPerCell[0])}`).toBe(true);
  expect(looksLog(tickPerCell[1]), `cell B not log: ${JSON.stringify(tickPerCell[1])}`).toBe(true);
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
  await row(page, 'log scale', 'X').click();
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
