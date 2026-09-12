// Copy the freshly built wasm glue (nk_ide.js + nk_ide.wasm) into
// play/public/ so the dev server and the static build serve it. The
// engine loader fetches /nk_ide.js from the origin — this makes that
// URL resolve. Run after engine-build --wasm; wired into dev/build.
import { copyFile, mkdir } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const srcDir = path.resolve(here, '../../build/wasm/release/wasm/dist');
const outDir = path.resolve(here, '../public');

await mkdir(outDir, { recursive: true });
for (const f of ['nk_ide.js', 'nk_ide.wasm']) {
  await copyFile(path.join(srcDir, f), path.join(outDir, f));
  console.log(`wasm: ${f} -> play/public/`);
}
