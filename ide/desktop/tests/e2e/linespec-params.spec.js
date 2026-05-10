// linespec-params.spec.js — BUG #38 regression guard.
//   • LineWidth N-V actually changes the SVG stroke-width
//   • lineStyle ('--', ':', '-.') drives stroke-dasharray
//   • marker glyph from spec ('o', 's', '^', etc.) renders along the line
//   • MarkerSize N-V actually scales the markers
//
// Tests inspect rendered SVG attributes directly (no pixel-diff).

import { test, expect } from '@playwright/test';
import { launchIde, closeIde } from '../helpers/launch.js';
import { IdePage } from '../helpers/ide.js';

test.describe('linespec / N-V params — BUG #38', () => {
  let app, page, ide;

  test.beforeEach(async () => {
    app = await launchIde();
    page = await app.firstWindow();
    ide = new IdePage(page);
    await ide.waitForReady();
  });

  test.afterEach(async () => { await closeIde(app); });

  test('LineWidth N-V reaches stroke-width on SVG path', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'x = linspace(0, 1, 20);\n'
      + 'plot(x, x, \'b-\', \'LineWidth\', 4);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    // Find the data-layer path (excluding axis/frame/clip strokes) by
    // looking for a path with explicit stroke-width="4".
    const card = ide.figureCards.first();
    const paths = card.locator('svg path[stroke-width="4"]');
    expect(await paths.count()).toBeGreaterThanOrEqual(1);
  });

  test('lineStyle "--" → stroke-dasharray on SVG path', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'x = linspace(0, 1, 20);\n'
      + 'plot(x, x, \'r--\', \'LineWidth\', 2);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    // strokeDasharray is forwarded to stroke-dasharray; '6,4' for '--'.
    const dashed = ide.figureCards.first()
      .locator('svg path[stroke-dasharray="6,4"]');
    expect(await dashed.count()).toBeGreaterThanOrEqual(1);
  });

  test('lineStyle ":" → dotted dasharray', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'x = linspace(0, 1, 20);\n'
      + 'plot(x, x, \'g:\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    const dotted = ide.figureCards.first()
      .locator('svg path[stroke-dasharray="1,3"]');
    expect(await dotted.count()).toBeGreaterThanOrEqual(1);
  });

  test('marker "o" with line draws BOTH path and circles', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'x = linspace(0, 1, 10);\n'
      + 'plot(x, x, \'r-o\', \'MarkerSize\', 5);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    const card = ide.figureCards.first();
    // Expect at least one stroked path AND >= 10 circles for the markers.
    const paths = await card.locator('svg path').count();
    const circles = await card.locator('svg circle').count();
    expect(paths).toBeGreaterThanOrEqual(1);
    expect(circles).toBeGreaterThanOrEqual(10);
  });

  test('marker "s" → squares (rects) along the line', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'x = linspace(0, 1, 8);\n'
      + 'plot(x, x, \'b-s\', \'MarkerSize\', 6);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    // Rect glyph used for 's' marker: width = 2*r = 12.
    const rects = await ide.figureCards.first()
      .locator('svg rect[width="12"]').count();
    expect(rects).toBeGreaterThanOrEqual(8);
  });

  test('marker "^" → triangle path glyphs', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'x = linspace(0, 1, 6);\n'
      + 'plot(x, x, \'k-^\', \'MarkerSize\', 5);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    // Triangle glyph emits a path with M…L…L…Z. Just count total
    // paths — the line + 6 triangles ≈ ≥ 7.
    const paths = await ide.figureCards.first().locator('svg path').count();
    expect(paths).toBeGreaterThanOrEqual(7);
  });

  test('three-style overlay (b-, r--o, g:s) keeps all three distinct', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'x = linspace(0, 2*pi, 40);\n'
      + 'hold on;\n'
      + 'plot(x, sin(x),         \'b-\',   \'LineWidth\', 3);\n'
      + 'plot(x, sin(x - pi/4),  \'r--o\', \'LineWidth\', 1.5, \'MarkerSize\', 4);\n'
      + 'plot(x, sin(x - pi/2),  \'g:s\',                      \'MarkerSize\', 5);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    const card = ide.figureCards.first();
    // Solid LW=3 path
    expect(await card.locator('svg path[stroke-width="3"]').count())
      .toBeGreaterThanOrEqual(1);
    // Dashed LW=1.5 path
    expect(await card.locator('svg path[stroke-dasharray="6,4"][stroke-width="1.5"]').count())
      .toBeGreaterThanOrEqual(1);
    // Dotted path for 'g:s'
    expect(await card.locator('svg path[stroke-dasharray="1,3"]').count())
      .toBeGreaterThanOrEqual(1);
    // Marker overlays from r--o + g:s. Circles (o) + rects (s).
    expect(await card.locator('svg circle').count()).toBeGreaterThanOrEqual(20);
    expect(await card.locator('svg rect[width="10"]').count()).toBeGreaterThanOrEqual(20);
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('scatter marker spec respected (\'+\' instead of default \'o\')', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'scatter(linspace(0,1,8), linspace(0,1,8), \'+\');\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    // '+' glyph emits a <g> of 2 lines — count line elements.
    // Each '+' marker = 2 lines, 8 markers => 16+ lines (plus axis lines).
    const lines = await ide.figureCards.first().locator('svg line').count();
    expect(lines).toBeGreaterThanOrEqual(16);
  });
});
