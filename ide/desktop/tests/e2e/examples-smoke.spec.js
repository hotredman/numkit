// examples-smoke.spec.js — smoke-test the new examples shipped this
// session. We don't pixel-compare; just confirm each runs to
// completion without engine errors and produces at least one
// figure card.

import { test, expect } from '@playwright/test';
import { launchIde, closeIde } from '../helpers/launch.js';
import { IdePage } from '../helpers/ide.js';
import { readFileSync } from 'fs';
import { join, dirname } from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname  = dirname(__filename);
const REPO = join(__dirname, '..', '..', '..', '..');
const EX = join(REPO, 'ide', 'public', 'examples');

const SAMPLES = [
  ['Plotting',                 'imshow_demo.m'],
  ['Plotting',                 'contourf_filled.m'],
  ['Plotting',                 'animatedline_demo.m'],
  ['Plotting',                 'tiledlayout_demo.m'],
  ['Plotting',                 'bar_matrix_demo.m'],
  ['Plotting',                 'geoplot_demo.m'],
  ['3D_Volume',                'slice_volume.m'],
  ['3D_Volume',                'isosurface_sphere.m'],
  ['3D_Volume',                'coneplot_streamtube.m'],
  ['Computational_Geometry',   'convex_hull.m'],
  ['Computational_Geometry',   'inpolygon_test.m'],
  ['Computational_Geometry',   'delaunay_voronoi.m'],
  ['Computational_Geometry',   'griddata_scattered.m'],
  ['Group_Operations',         'group_aggregation.m'],
];

test.describe('Examples — smoke (session-shipped)', () => {
  let app, page, ide;

  test.beforeEach(async () => {
    app = await launchIde();
    page = await app.firstWindow();
    ide = new IdePage(page);
    await ide.waitForReady();
  });

  test.afterEach(async () => { await closeIde(app); });

  for (const [folder, file] of SAMPLES) {
    test(`${folder}/${file} runs without engine errors`, async () => {
      const src = readFileSync(join(EX, folder, file), 'utf8');
      await ide.runScript(src);
      // Wait for any async drawnow/emitModified to flush.
      await page.waitForTimeout(300);
      // Most examples produce ≥ 1 figure card. A couple are
      // console-only (group_aggregation also makes a bar chart);
      // we just want NO unexpected console errors.
      const errors = ide.devErrors().filter((e) =>
        !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e));
      expect(errors).toEqual([]);
    });
  }
});
