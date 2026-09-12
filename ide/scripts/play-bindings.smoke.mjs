// Node smoke for the Play reactive bindings (portion C). Loads the
// freshly built wasm glue and drives a real gallery-shaped script:
// manifest extraction → param tick → equivalence vs closed form →
// zero-copy array view → figure wire on tick → string select param.
//
// Run: node ide/scripts/play-bindings.smoke.mjs   (after engine-build --wasm)
import { createRequire } from 'node:module';
const require = createRequire(import.meta.url);
const path = await import('node:path');
const { fileURLToPath } = await import('node:url');

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
const lastNumber = (s) => parseFloat(s.trim().split(/\s+/).pop());

// 1. Manifest extraction (canonical @play marker).
const manifest = M.playLoadScript(`%{
@play
{ "title": "t", "controls": [ { "var": "a", "default": 1 } ] }
%}
a = 1;
b = a * 2 + sin(a);
plot(b);
`);
ok(manifest.includes('"controls"') && manifest.includes('"var"'),
   'manifest JSON returned', manifest.slice(0, 40));

// 2. Plain script (no manifest) loads with "" manifest.
const plain = M.playLoadScript('x = 1; y = x + 41;');
ok(plain === '', 'plain script manifest is empty string');

// 3. Param tick: value matches the closed form.
M.playLoadScript(`%{
@play
{ "controls": [ { "var": "a", "default": 1 } ] }
%}
a = 1;
b = a * 2 + sin(a);
c = b * b;
`);
M.playSetParamDouble('a', 2.0);
ok(M.playHasDirty(), 'param tick marks dirty');
M.playEvalDirty();
ok(!M.playHasDirty(), 'evalDirty clears dirty');
const bAfter = lastNumber(M.repl_execute('disp(b);'));
ok(Math.abs(bAfter - (2 * 2 + Math.sin(2))) < 1e-3,  // disp rounds to 5 digits
   'reactive value matches closed form', `b=${bAfter}`);

// 4. Generation monotonicity.
const g1 = M.playGeneration();
M.playSetParamDouble('a', 3.0);
M.playEvalDirty();
ok(M.playGeneration() > g1, 'generation bumped', `${g1} -> ${M.playGeneration()}`);

// 5. Zero-copy array view: pointer into linear memory.
M.playLoadScript('v = [1 2 3 4 5 6];');
const info = JSON.parse(M.playArrayViewInfo('v'));
ok(info.present && info.rows === 1 && info.cols === 6,
   'array view info shape', JSON.stringify(info));
const view = M.playArrayView('v');
ok(view && view.length === 6 && view[0] === 1 && view[5] === 6,
   'zero-copy view reads the right data', view ? [...view].join(',') : 'undefined');

// 6. Figure wire rides the output channel on a tick. playEvalDirty
// runs statements whose plot calls emit __FIGURE_DATA__ through the
// engine output func — captured here via the print callback.
let figureLines = [];
const M2 = await Module({
  print: (t) => { if (t.includes('__FIGURE_DATA__')) figureLines.push(t); },
  printErr: () => {},
});
M2.repl_init();
M2.playLoadScript(`%{
@play
{ "controls": [ { "var": "a", "default": 1 } ] }
%}
a = 1;
y = a * (1:5);
plot(y);
`);
M2.playSetParamDouble('a', 2.0);
const wire = M2.playEvalDirty();
const figHit = wire.includes('"y":[2,4,6,8,10]');
ok(figHit && wire.includes('__FIGURE_DATA__'),
   'figure wire updated on tick', wire.slice(0, 70));

// 7. String param (select controls).
M.playLoadScript(`%{
@play
{ "controls": [ { "var": "win", "type": "select", "options": ["rect","hamming"], "default": "rect" } ] }
%}
win = 'rect';
if strcmp(win, 'hamming')
  n = 1;
else
  n = 2;
end
`);
M.playSetParamString('win', 'hamming');
M.playEvalDirty();
const nAfter = parseInt(lastNumber(M.repl_execute('disp(n);')), 10);
ok(nAfter === 1, 'string select param drives the branch', `n=${nAfter}`);

// 8. Element write (table editor): only the touching formula re-runs.
M.playLoadScript(`%{
@play
{ "controls": [] }
%}
C = cell(2, 2);
C{1,1} = 2;
C{1,2} = 3;
C{2,1} = C{1,1} * 10;
C{2,2} = C{1,2} * 100;
`);
M.playSetParamElement('C', 1, 1, 7.0);
ok(M.playHasDirty(), 'element write marks dirty');
M.playEvalDirty();
const c21 = parseInt(lastNumber(M.repl_execute('disp(C{2,1});')), 10);
ok(c21 === 70, 'element write recalcs the touching formula',
   `C{2,1}=${c21}`);

console.log(failed === 0 ? '\nALL PLAY-BINDING SMOKES GREEN' : `\n${failed} FAILURES`);
process.exit(failed === 0 ? 0 : 1);
