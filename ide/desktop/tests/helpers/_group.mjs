// _group.mjs — move spec files into topic subdirs under e2e/.
// Adjusts import paths inside (../helpers/x → ../../helpers/x).
// One-shot, kept in the tree for the next regrouping wave.

import { readFileSync, writeFileSync, readdirSync, mkdirSync, existsSync } from 'node:fs';
import path from 'node:path';

const e2eDir = path.resolve(import.meta.dirname, '..', 'e2e');

// Group → spec-file mapping. Each spec lands in exactly one group.
// Any spec not listed stays at e2e/ root (the "leftover" bucket).
const groups = {
  core: [
    // Fresh-launch / IDE shell — usually pre-test setup or boot logs.
    'smoke.spec.js',
    'engine-init.spec.js',
    'vfs.spec.js',
    'memory.spec.js',
    'local-folder.spec.js',
    'ide-smoke.spec.js',
    'figures.spec.js',
    'workspace.spec.js',
    'figure-state-parity.spec.js',
  ],
  '2d': [
    // 2-D plotting primitives + their family.
    'b1-bar-area.spec.js',
    'b1-errorbar.spec.js',
    'b1-pcolor.spec.js',
    'b1-quiver.spec.js',
    'b2-axis.spec.js',
    'b2-colorbar.spec.js',
    'b2-legend.spec.js',
    'b2-linkaxes.spec.js',
    'b2-polish.spec.js',
    'b2-xydir.spec.js',
    'b2-yyaxis.spec.js',
    'b3-contour.spec.js',
    'b3-histogram2.spec.js',
    'b3-streamline.spec.js',
    'animatedline.spec.js',
    'area-stacked.spec.js',
    'bar-matrix.spec.js',
    'box.spec.js',
    'bubble-swarm.spec.js',
    'cla.spec.js',
    'colormap-custom.spec.js',
    'comet.spec.js',
    'confusionchart.spec.js',
    'contourf.spec.js',
    'daspect.spec.js',
    'geoplot.spec.js',
    'groups.spec.js',
    'heatmap-parallel.spec.js',
    'histcounts2.spec.js',
    'legend-boxoff.spec.js',
    'linespec-params.spec.js',
    'polar-thetalim.spec.js',
    'subtitle-sgtitle.spec.js',
    'ticks.spec.js',
    'xline-yline.spec.js',
    'linkprop.spec.js',
    't1a-misc.spec.js',
    't1b-stats.spec.js',
    't1c-polar.spec.js',
    't1d-fplot.spec.js',
    't2a-patch.spec.js',
    't2b-shapes.spec.js',
    'grid-matlab-parity.spec.js',
    'delaunay.spec.js',
    'griddata.spec.js',
    'inpolygon-convhull.spec.js',
    'voronoi.spec.js',
  ],
  '3d': [
    // 3-D / WebGL plotting.
    '3d-grid-camera.spec.js',
    '3d-view-presets.spec.js',
    'b1-plot3.spec.js',
    'b3-surf.spec.js',
    'coneplot-streamtube.spec.js',
    'figure-3d-renders.spec.js',
    'isosurface.spec.js',
    'slice.spec.js',
    'webgl-3d.spec.js',
    'webgl-3d-axes.spec.js',
    'webgl-3d-edge.spec.js',
    'webgl-3d-export.spec.js',
    'webgl-3d-interaction.spec.js',
    'webgl-3d-lighting.spec.js',
    'webgl-3d-shapes.spec.js',
    'webgl-3d-window.spec.js',
    'webgl-3d-wrappers.spec.js',
  ],
  figures: [
    // Figure-window / imshow / subplot grid mechanics.
    'figure-window-2d.spec.js',
    'figure-window-layout.spec.js',
    'imshow.spec.js',
    'imshow-pan-direction.spec.js',
    'imshow-preview-orientation.spec.js',
    'subplot-empty-slot.spec.js',
    'subplot-rerun-shape-change.spec.js',
    'tiledlayout.spec.js',
  ],
  regressions: [
    // Pinned bug-N regression guards.
    'arrayfun-bug11.spec.js',
    'bug14-logical-colon.spec.js',
    'impzlength-bug32.spec.js',
    'interp2-grid-bug21.spec.js',
    'interpn-bug31.spec.js',
    'mldivide-bug28.spec.js',
  ],
  examples: [
    'examples-smoke.spec.js',
    'plotting-examples.spec.js',
  ],
};

// Inverse map for quick lookup.
const fileToGroup = {};
for (const [g, files] of Object.entries(groups)) {
  for (const f of files) fileToGroup[f] = g;
}

// Sanity: every existing spec is in a group, no missing files.
const allSpecs = readdirSync(e2eDir).filter((f) => f.endsWith('.spec.js'));
const ungrouped = allSpecs.filter((f) => !(f in fileToGroup));
const missing = Object.keys(fileToGroup).filter((f) => !allSpecs.includes(f));
if (ungrouped.length) {
  console.log('Ungrouped (staying at e2e/ root):');
  for (const f of ungrouped) console.log('  ·', f);
}
if (missing.length) {
  console.log('Listed in groups but not found on disk:');
  for (const f of missing) console.log('  ·', f);
}

// Move + path rewrite.
let moved = 0;
for (const file of allSpecs) {
  const group = fileToGroup[file];
  if (!group) continue;
  const targetDir = path.join(e2eDir, group);
  if (!existsSync(targetDir)) mkdirSync(targetDir, { recursive: true });
  const src = path.join(e2eDir, file);
  const dst = path.join(targetDir, file);

  // Read content + rewrite '../helpers/x' → '../../helpers/x'.
  let body = readFileSync(src, 'utf8');
  body = body.replace(/from\s*'\.\.\/helpers\//g, "from '../../helpers/");
  body = body.replace(/from\s*'\.\.\/fixtures\//g, "from '../../fixtures/");
  writeFileSync(dst, body);
  renameSync(src, dst);   // safer: write+rename to overwrite

  moved++;
}

console.log(`moved ${moved}/${allSpecs.length}`);
