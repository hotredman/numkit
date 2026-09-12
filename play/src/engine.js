// Engine loader for Play — the wasm glue built by engine-build --wasm
// (MODULARIZE factory). Same artifact the IDE loads; Play consumes the
// play* reactive bindings (portion C).
import { createRequire } from 'node:module';
const require = createRequire(import.meta.url);

let glueUrl;
if (typeof window !== 'undefined') {
  // Browser: the glue + wasm ship as static assets (see scripts/
  // build-gallery.mjs copying them into public/).
  glueUrl = new URL('/nk_ide.js', window.location.origin).href;
}

let modPromise = null;

function loadInNode() {
  // Node (tests / smokes): load straight from the build tree.
  const path = require('node:path');
  const glue = path.resolve('..', 'build', 'wasm', 'release', 'wasm', 'dist', 'nk_ide.js');
  const factory = require(glue);
  return factory({ print: () => {}, printErr: () => {} });
}

export function getEngine() {
  if (!modPromise) {
    modPromise = typeof window === 'undefined'
      ? Promise.resolve(loadInNode())
      : import(/* @vite-ignore */ glueUrl).then((m) => m.default ?? m);
  }
  return modPromise;
}

// ── Figure-wire parsing (the SAME markers the IDE consumes) ──────────
// __FIGURE_DATA__:{json} lines → objects; close markers → events.
export function parseFigureWire(text) {
  const figures = [];
  const closed = [];
  for (const line of text.split('\n')) {
    const t = line.trim();
    if (t.startsWith('__FIGURE_DATA__:')) {
      try {
        figures.push(JSON.parse(t.slice('__FIGURE_DATA__:'.length)));
      } catch { /* skip malformed */ }
    } else if (t.startsWith('__FIGURE_CLOSE__:')) {
      closed.push(parseInt(t.split(':')[1], 10));
    } else if (t === '__FIGURE_CLOSE_ALL__') {
      closed.push('all');
    }
  }
  return { figures, closed };
}
