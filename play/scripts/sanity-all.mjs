// Sanity: run ALL gallery .m scripts through playLoadScript (engine)
// via Node — any parse/exec error fails the batch. Also verifies each
// script produces at least one __FIGURE_DATA__ in the drained wire
// (except scripts with no plot, which the runner reports).
import { createRequire } from 'node:module';
import { readdirSync, readFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const galDir = path.resolve(here, '../gallery');
const glue = path.resolve(here, '../../build/wasm/release/wasm/dist/nk_ide.js');
const require = createRequire(import.meta.url);
const M = await (require(glue)({ print: () => {}, printErr: () => {} }));
M.repl_init();

const scripts = readdirSync(galDir, { recursive: true })
  .filter((f) => f.endsWith('.m') && !f.includes('node_modules'))
  .sort();

let pass = 0, fail = 0, nofig = 0;
for (const rel of scripts) {
  const src = readFileSync(path.join(galDir, rel), 'utf-8');
  try {
    const man = M.playLoadScript(src);
    if (man.includes('"__error"')) {
      console.log(`FAIL ${rel}: ${JSON.parse(man).message}`);
      fail++;
      continue;
    }
    const wire = M.playEvalDirty();
    if (!wire.includes('__FIGURE_DATA__')) nofig++;
    pass++;
  } catch (e) {
    console.log(`FAIL ${rel}: ${e.message}`);
    fail++;
  }
}
console.log(`
sanity: ${pass}/${scripts.length} load clean, ${nofig} without figure output`);
if (fail > 0) process.exit(1);
