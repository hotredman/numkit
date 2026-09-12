// End-to-end reactive-loop smoke for the play/ app contract (D1):
// the REAL gallery script through the REAL wasm bindings → wire JSON →
// param tick → updated wire. Headless (Node) — the Svelte layer renders
// from the same wire this test validates.
import { createRequire } from 'node:module';
const require = createRequire(import.meta.url);
const path = await import('node:path');
const { fileURLToPath } = await import('node:url');
const { readFile } = await import('node:fs/promises');

const here = path.dirname(fileURLToPath(import.meta.url));
const gluePath = path.resolve(here, '../../build/wasm/release/wasm/dist/nk_ide.js');
const Module = require(gluePath);
const M = await Module({ print: () => {}, printErr: () => {} });
M.repl_init();

let failed = 0;
const ok = (cond, label, extra = '') => {
  console.log(`${cond ? 'PASS' : 'FAIL'}  ${label}${extra ? '  [' + extra + ']' : ''}`);
  if (!cond) failed++;
};

const source = await readFile(
  path.resolve(here, '../gallery/phased-array-beamforming.m'), 'utf-8');

// The same wire parser the Svelte app uses.
const parseWire = (text) => ({
  figures: text.split('\n')
    .filter((l) => l.startsWith('__FIGURE_DATA__:'))
    .map((l) => JSON.parse(l.slice('__FIGURE_DATA__:'.length))),
});

// 1. Load: manifest with all 4 controls.
const manifest = M.playLoadScript(source);
const m = JSON.parse(manifest);
const ctrls = (m.groups ?? []).flatMap((g) => g.controls ?? []);
ok(manifest.includes('"theta0"') && ctrls.length === 4,
   'gallery manifest parsed', `${ctrls.length} controls`);

// 2. Initial wire: one figure, polar axes, one line dataset.
let wire = M.playEvalDirty();
let figs = parseWire(wire).figures;
ok(figs.length === 1 && figs[0].axes?.length === 1,
   'initial figure in wire');
let ds = figs[0].axes[0].datasets.find((d) => d.type === 'line')
       ?? figs[0].axes[0].datasets[0];
const n0 = ds?.y?.length ?? 0;
ok(n0 > 300, 'polar series has the theta sweep', `${n0} pts`);
const mag0Max = Math.max(...ds.y.filter(Number.isFinite));
ok(Math.abs(mag0Max - 1) < 1e-9, 'normalized beam peak = 1', `max=${mag0Max?.toFixed?.(3)}`);

// 3. Steer the beam: theta0 = -30 → the peak must MOVE (the theta
//    index of the max shifts by the sweep).
M.playSetParamDouble('theta0', -30);
wire = M.playEvalDirty();
figs = parseWire(wire).figures;
ds = figs[0].axes[0].datasets.find((d) => d.type === 'line')
   ?? figs[0].axes[0].datasets[0];
const argmax = ds.y.indexOf(Math.max(...ds.y.filter(Number.isFinite)));
const thetaAtPeak = ds.x[argmax] * 180 / Math.PI;
ok(Math.abs(thetaAtPeak - (-30)) < 2.5,
   'beam steered to theta0=-30', `peak at ${thetaAtPeak.toFixed(1)}°`);
ok(Math.abs(Math.max(...ds.y.filter(Number.isFinite)) - 1) < 1e-9,
   'steered beam still normalized');

// 4. Select control: chebwin window widens the main lobe (peak value
//    with normalization by sum(w) stays 1; the FIRST NULL moves).
M.playSetParamString('win', 'chebwin');
wire = M.playEvalDirty();
figs = parseWire(wire).figures;
ds = figs[0].axes[0].datasets.find((d) => d.type === 'line')
   ?? figs[0].axes[0].datasets[0];
ok(Math.abs(Math.max(...ds.y.filter(Number.isFinite)) - 1) < 1e-9,
   'chebwin window still normalized to 1');

console.log(failed === 0 ? '\nALL PLAY-LOOP SMOKES GREEN' : `\n${failed} FAILURES`);
process.exit(failed === 0 ? 0 : 1);
