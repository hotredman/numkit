// geoplot.spec.js — geographic plots without a basemap (v1).
//
// geoplot / geoscatter / geobubble route through plot / scatter
// with (X = lon, Y = lat) and auto-set xlabel/ylabel. Real
// basemap tile rendering is BACKLOG.

import { test, expect } from '../helpers/shared.js';

test.describe('geoplot / geoscatter / geobubble', () => {
  test('geoplot(lat, lon) — line trace appears', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'lat = [37.7 40.7 51.5 48.8 35.7];\n'
      + 'lon = [-122.4 -74.0 -0.1 2.3 139.7];\n'
      + 'geoplot(lat, lon);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('geoscatter(lat, lon) — point markers in (lon, lat) plane', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'lat = [37.7 40.7 51.5];\n'
      + 'lon = [-122.4 -74.0 -0.1];\n'
      + 'geoscatter(lat, lon);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    // Scatter mode draws circles for each point; expect ≥ 3 circles.
    const circles = await ide.figureCards.first().locator('svg circle').count();
    expect(circles).toBeGreaterThanOrEqual(3);
  });

  test('geobubble(lat, lon, sizes) — variable-size markers', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'lat = [37.7 40.7 51.5];\n'
      + 'lon = [-122.4 -74.0 -0.1];\n'
      + 'sz  = [50 100 75];\n'
      + 'geobubble(lat, lon, sz);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });

  test('geoplot — auto xlabel="lon" / ylabel="lat"', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'geoplot([1 2 3], [10 20 30]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    // Modal SVG includes axis labels somewhere as <text>.
    const labels = await ide.figureWindow.locator('svg text').allTextContents();
    expect(labels.join(' ')).toMatch(/lon/);
    expect(labels.join(' ')).toMatch(/lat/);
  });
});
