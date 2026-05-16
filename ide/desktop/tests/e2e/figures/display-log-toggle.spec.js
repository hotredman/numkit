// display-log-toggle.spec.js — verify the log-scale buttons inside
// the axes ▾ matrix switch the axis to log mapping. Probe the y-axis
// tick set: linear on plot(1:1000) gives ticks at 100 / 200 / …
// while log gives 10 / 100 / 1000.
//
// After the axes ▾ matrix-layout refactor: per-axis log lives in
// column 1 of the matrix row whose label is `X` / `Y` / `Z`.

import { test, expect } from '../../helpers/shared.js';

async function openDisplay(page) {
  await page.locator('.fw-toolbar .ve-btn', { hasText: /axes/i }).click();
  await expect(page.locator('.fw-pop').first()).toBeVisible({ timeout: 2_000 });
}

/** Locate one cell in the axes-matrix by (axis, col index).
 *  col: 0 = reverse, 1 = log. */
function axisBtn(page, axis, col) {
  return page.locator('.fw-pop-matrix .fw-pop-mrow', {
    has: page.locator('.fw-pop-mrow-label', { hasText: new RegExp(`^${axis}$`) }),
  }).locator('.fw-pop-mbtn').nth(col);
}
async function isActive(btn) {
  const cls = (await btn.getAttribute('class')) || '';
  return /\bis-active\b/.test(cls);
}

async function yTicks(page) {
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
  const looksLinear = linearTicks.some((t) => /^([2-9]00|[1-9]\d{2}\.0)$/.test(t));
  expect(looksLinear, `linear ticks ${JSON.stringify(linearTicks)}`).toBe(true);

  await openDisplay(page);
  await axisBtn(page, 'Y', 1).click();    // Y log
  await page.waitForTimeout(150);

  const logTicks = await yTicks(page);
  const looksLog = logTicks.some((t) => /^(10|10\.0|100|1000)$/.test(t));
  expect(looksLog, `log ticks ${JSON.stringify(logTicks)}`).toBe(true);
});

test('toolbar axes ▾ xlog ✓ on subplot_demo (2x2 sin/cos)', async ({ ide, page }) => {
  // Reproduces user-reported bug: subplot_demo.m → click toolbar
  // xlog button → ✓ wasn't appearing because toggleAxisLog
  // dereferenced viewport.x[0] but FigureWindow.viewport is null for
  // subplot figures (per-cell viewports live in SubplotGrid).
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
  const xlogBtn = axisBtn(page, 'X', 1);
  expect(await isActive(xlogBtn), 'X log should start inactive').toBe(false);

  await xlogBtn.click();
  await page.waitForTimeout(120);
  expect(await isActive(xlogBtn), 'X log should be active after click').toBe(true);
});

test('toolbar axes ▾ xlog click sets the is-active state', async ({ ide, page }) => {
  await ide.runScript('import compat.*;\nplot(1:1000);\n');
  await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
  await ide.figureCards.first().click();
  await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(120);

  await openDisplay(page);
  const xlogBtn = axisBtn(page, 'X', 1);
  expect(await isActive(xlogBtn)).toBe(false);
  await xlogBtn.click();
  await page.waitForTimeout(80);
  expect(await isActive(xlogBtn)).toBe(true);
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
  await axisBtn(page, 'X', 1).click();   // X log
  await page.waitForTimeout(200);

  const tickPerCell = await page.locator('.fw-window .fw-canvas-wrap svg').evaluateAll((els) =>
    els.map((svg) => Array.from(svg.querySelectorAll('text'))
      .filter((t) => parseFloat(t.getAttribute('y') || '0') > 200
                  && /^-?\d/.test(t.textContent || ''))
      .map((t) => t.textContent))
  );
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
  await axisBtn(page, 'X', 1).click();   // X log
  await page.waitForTimeout(150);

  const xLabels = await page.locator('.fw-window svg text').evaluateAll((els) =>
    els
      .filter((el) => {
        const y = parseFloat(el.getAttribute('y') || '0');
        return y > 400 && /^-?\d/.test(el.textContent || '');
      })
      .map((el) => el.textContent)
  );
  const looksLog = xLabels.some((t) => /^(10|100|1000)$/.test(t));
  expect(looksLog, `log x ticks ${JSON.stringify(xLabels)}`).toBe(true);
});
