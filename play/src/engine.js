// Engine loader for Play — the wasm glue built by engine-build --wasm
// (MODULARIZE factory). Same artifact the IDE loads; Play consumes the
// play* reactive bindings (portion C). Node-only imports are lazy so
// the browser bundle never references node: modules.
let modPromise = null;

async function loadInNode() {
  const { createRequire } = await import('node:module');
  const require = createRequire(import.meta.url);
  const path = (await import('node:path')).default;
  const glue = path.resolve('..', 'build', 'wasm', 'release', 'wasm',
                            'dist', 'nk_ide.js');
  const factory = require(glue);
  return factory({ print: () => {}, printErr: () => {} });
}

async function loadInBrowser() {
  const url = new URL('/nk_ide.js', window.location.origin).href;
  const m = await import(/* @vite-ignore */ url);
  return (m.default ?? m)({ print: () => {}, printErr: () => {} });
}

export function getEngine() {
  if (!modPromise)
    modPromise = (typeof window === 'undefined'
      ? loadInNode()
      : loadInBrowser());
  return modPromise;
}
