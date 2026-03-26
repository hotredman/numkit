/**
 * Engine abstraction layer.
 *
 * Both the real WASM engine and the JS fallback expose:
 *   init()              → string   (welcome message)
 *   execute(code)       → string   (output)
 *   complete(partial)   → string[] (completions)
 *   reset()             → string   (message)
 *   workspace()         → string   (whos output)
 *   getVars()           → object   (variables map, fallback only)
 */

// ── WASM engine wrapper ──
export async function createWasmEngine(createModule) {
  const Module = await createModule({
    locateFile: (path) => `/${path}`,
    print: (text) => console.log('[WASM stdout]', text),
    printErr: (text) => console.warn('[WASM stderr]', text),
  });

  if (typeof Module.repl_init !== 'function') {
    throw new Error('repl_init not found in WASM module');
  }

  return {
    type: 'wasm',

    init() {
      return Module.repl_init();
    },

    execute(code) {
      return Module.repl_execute(code);
    },

    complete(partial) {
      const raw = Module.repl_complete(partial);
      if (!raw) return [];
      return raw.split(',').filter(Boolean);
    },

    reset() {
      return Module.repl_reset();
    },

    workspace() {
      return Module.repl_workspace();
    },

    // WASM engine doesn't expose a JS vars map — workspace() is text-based.
    getVars() {
      return {};
    },
  };
}

// ── Fallback JS engine ──
import { createInterpreter } from './interpreter';

export function createFallbackEngine() {
  const interp = createInterpreter();

  return {
    type: 'fallback',

    init() {
      return 'MLab REPL v2.1 — Demo Mode\nType commands below. "help <topic>" for function info.';
    },

    execute(code) {
      const result = interp.execute(code);
      const ret = result.output || '';
      this._lastPlot = result.plot || null;
      return ret;
    },

    getLastPlot() {
      const p = this._lastPlot;
      this._lastPlot = null;
      return p;
    },

    complete(partial) {
      return interp.complete(partial);
    },

    reset() {
      interp.reset();
      return 'Workspace cleared.';
    },

    workspace() {
      const vars = interp.getVars();
      const keys = Object.keys(vars);
      if (!keys.length) return 'No variables in workspace.';
      const lines = ['  Name          Size        Class'];
      for (const k of keys) {
        const v = vars[k];
        let sz = '1x1', cls = typeof v;
        if (Array.isArray(v)) {
          if (v.length && Array.isArray(v[0])) sz = `${v.length}x${v[0].length}`;
          else sz = `1x${v.length}`;
          cls = 'double';
        } else if (typeof v === 'string') cls = 'char';
        else cls = 'double';
        lines.push(`  ${k.padEnd(14)}${sz.padEnd(12)}${cls}`);
      }
      return lines.join('\n');
    },

    getVars() {
      return interp.getVars();
    },

    _lastPlot: null,
  };
}
