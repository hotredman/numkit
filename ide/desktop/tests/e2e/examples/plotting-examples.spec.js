// plotting-examples.spec.js — sanity-run a sample of the new
// examples/Plotting/*.m files through the IDE to confirm
// they execute clean. We don't run all 49 (that's a stress-test,
// not a smoke); a representative slice covering 3-D + new 2-D
// builtins is enough.

import { test, expect } from '../../helpers/shared.js';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const EXAMPLES_DIR = path.resolve(__dirname, '..', '..', '..', 'dist', 'examples', 'Plotting');

const SAMPLE_FILES = [
  // 3-D
  'plot3_helix.m',
  'scatter3_clusters.m',
  'surf_peaks.m',
  'surf_lighting.m',
  'surfl_demo.m',
  'mesh_grid.m',
  'bar3_chart.m',
  'waterfall_demo.m',
  'quiver3_field.m',
  'contour3_levels.m',
  'fill3_pyramid.m',
  'view_presets.m',
  'fplot_3d.m',
  'stem3_demo.m',
  // 2-D rich
  'errorbar_demo.m',
  'area_filled.m',
  'pcolor_grid.m',
  'boxplot_iqr.m',
  'pie_chart.m',
  'histfit_normal.m',
  'qqplot_normal.m',
  'pareto_chart.m',
  'histogram2_density.m',
  'contour_topo.m',
  'streamline_field.m',
  'yyaxis_demo.m',
  'compass_feather.m',
  'spy_sparsity.m',
  'patch_polygon.m',
];

test.describe('examples/Plotting — runtime sanity', () => {
  for (const fname of SAMPLE_FILES) {
    test(`${fname} runs clean`, async ({ ide, page }) => {
      const src = readFileSync(path.join(EXAMPLES_DIR, fname), 'utf8');
      await ide.runScript(src);
      // Give 3-D figures a moment to mount + axes frame to build.
      await page.waitForTimeout(250);
      // At least one figure card present. Some examples (e.g. fplot_3d
      // demonstrating both fplot and fsurf) call `figure` twice and
      // produce two cards — that's expected, so assert ≥ 1 rather
      // than === 1.
      await expect.poll(() => ide.figureCards.count(),
        { timeout: 10_000 }).toBeGreaterThanOrEqual(1);
      // No console errors during the script.
      const noisy = (e) =>
        /Autofill\.enable/i.test(e) || /\[hmr\]/i.test(e);
      const errs = ide.devErrors().filter((e) => !noisy(e));
      expect(errs, `unexpected errors in ${fname}:\n${errs.join('\n')}`)
        .toEqual([]);
    });
  }
});
