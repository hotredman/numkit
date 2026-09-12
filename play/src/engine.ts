// Engine loader for Play — the wasm glue built by engine-build --wasm
// (MODULARIZE factory). Same artifact the IDE loads; Play consumes
// the play* reactive bindings (portion C). Node-only imports are lazy
// so the browser bundle never references node: modules.

/** The reactive surface Play uses (portion C bindings on nk_ide.js). */
export interface PlayEngine {
  repl_init(): string;
  playLoadScript(source: string): string;
  playSetParamDouble(name: string, value: number): void;
  playSetParamString(name: string, value: string): void;
  playSetParamElement(name: string, r: number, c: number, v: number): void;
  /** Executes the dirty set; returns the drained __FIGURE_DATA__ wire. */
  playEvalDirty(): string;
  playHasDirty(): boolean;
  playLastError(): string;
  playGeneration(): number;
}

type GlueFactory = (opts: {
  print?: (t: string) => void;
  printErr?: (t: string) => void;
}) => Promise<PlayEngine>;

let modPromise: Promise<PlayEngine> | null = null;

async function loadInNode(): Promise<PlayEngine> {
  const { createRequire } = await import('node:module');
  const require = createRequire(import.meta.url);
  const path = (await import('node:path')).default;
  const glue = path.resolve(
    '..', 'build', 'wasm', 'release', 'wasm', 'dist', 'nk_ide.js');
  const factory = require(glue) as GlueFactory;
  return factory({ print: () => {}, printErr: () => {} });
}

async function loadInBrowser(): Promise<PlayEngine> {
  const url = new URL('/nk_ide.js', window.location.origin).href;
  const m = (await import(/* @vite-ignore */ url)) as {
    default?: GlueFactory;
  } & GlueFactory;
  const factory: GlueFactory = m.default ?? m;
  return factory({ print: () => {}, printErr: () => {} });
}

export function getEngine(): Promise<PlayEngine> {
  if (!modPromise)
    modPromise =
      typeof window === 'undefined' ? loadInNode() : loadInBrowser();
  return modPromise;
}
