// Engine loader for Play — the wasm glue built by engine-build --wasm.
// CRITICAL: the glue is UMD (MODULARIZE + EXPORT_NAME), NOT an ES
// module — a dynamic `import()` of it resolves to an empty namespace
// and every play* call silently does nothing. The IDE works because
// index.html loads it via a plain <script src> tag that defines
// window.createNkIdeModule. Play uses the same technique: inject the
// script tag once, then wrap the global factory.

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

interface GlueFactory {
  (opts?: {
    print?: (t: string) => void;
    printErr?: (t: string) => void;
  }): Promise<PlayEngine>;
}

declare global {
  interface Window {
    createNkIdeModule?: GlueFactory;
  }
}

let modPromise: Promise<PlayEngine> | null = null;

function injectGlueScript(): Promise<void> {
  return new Promise((resolve, reject) => {
    if (window.createNkIdeModule) {
      resolve();
      return;
    }
    const s = document.createElement('script');
    s.src = '/nk_ide.js';
    s.onload = () => resolve();
    s.onerror = () => reject(new Error('failed to load /nk_ide.js'));
    document.head.appendChild(s);
  });
}

async function loadInBrowser(): Promise<PlayEngine> {
  await injectGlueScript();
  const factory = window.createNkIdeModule;
  if (!factory)
    throw new Error('createNkIdeModule missing after glue load');
  return factory({
    print: () => {},
    printErr: () => {},
  });
}

async function loadInNode(): Promise<PlayEngine> {
  const { createRequire } = await import('node:module');
  const require = createRequire(import.meta.url);
  const path = (await import('node:path')).default;
  const glue = path.resolve(
    '..', 'build', 'wasm', 'release', 'wasm', 'dist', 'nk_ide.js');
  const factory = require(glue) as GlueFactory;
  return factory({ print: () => {}, printErr: () => {} });
}

export function getEngine(): Promise<PlayEngine> {
  if (!modPromise)
    modPromise =
      typeof window === 'undefined' ? loadInNode() : loadInBrowser();
  return modPromise;
}
