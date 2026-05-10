// voronoi.spec.js — Voronoi diagram via Delaunay dual.
//
// v1 draws bounded cell edges only (those between two adjacent
// Delaunay triangles). Cells touching the convex hull have
// unbounded edges that are omitted — proper infinite-ray extension
// is BACKLOG.

import { test, expect } from '@playwright/test';
import { launchIde, closeIde } from '../helpers/launch.js';
import { IdePage } from '../helpers/ide.js';

test.describe('voronoi', () => {
  let app, page, ide;

  test.beforeEach(async () => {
    app = await launchIde();
    page = await app.firstWindow();
    ide = new IdePage(page);
    await ide.waitForReady();
  });

  test.afterEach(async () => { await closeIde(app); });

  test('voronoi(x, y) — figure mounts with edges + point markers', async () => {
    await ide.runScript(
      'import compat.*;\n'
      // 5 random-ish points so we get several Voronoi edges.
      + 'x = [0 1 2 1 0.5];\n'
      + 'y = [0 0 0 1 0.6];\n'
      + 'voronoi(x, y);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    // Voronoi emits 1 line dataset for the edges + 1 scatter dataset
    // for the input points. Expect ≥ 5 circles (one per input).
    const card = ide.figureCards.first();
    const circles = await card.locator('svg circle').count();
    expect(circles).toBeGreaterThanOrEqual(5);
  });

  test('voronoi survives small N (3 points → at least no crash)', async () => {
    await ide.runScript(
      'import compat.*;\n'
      + 'voronoi([0 1 0.5], [0 0 1]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });
});
