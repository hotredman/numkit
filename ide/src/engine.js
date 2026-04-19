/**
 * Engine abstraction layer.
 *
 * Both WASM and fallback engines expose a unified interface:
 *   init()              → string
 *   execute(code)       → { output: string, figures: object[], closedFigureIds: number[], closeAllFigures: bool, errorLine: number|null }
 *   complete(partial)   → string[]
 *   reset()             → string
 *   workspace()         → string
 *   getVars()           → object
 *
 * Debug API:
 *   debugSetBreakpoints(lines)  → void     (lines = [1, 5, 10])
 *   debugStart(code)            → { status, line?, variables?, callStack?, output?, ... }
 *   debugResume(action)         → { status, line?, variables?, callStack?, output?, ... }
 *   debugStop()                 → void
 *
 * Figure objects: { id, datasets: [{x,y,type,label?,style?}], config: {title,xlabel,ylabel,xlim?,ylim?,grid,legend?} }
 */

/**
 * Parse __FIGURE_DATA__, __FIGURE_CLOSE__, __FIGURE_CLOSE_ALL__,
 * __PLOT_DATA__ and __ERROR_LINE__ markers.
 * Returns { cleanOutput, figures, closedFigureIds, closeAllFigures, errorLine }.
 */
function extractMarkers(rawOutput) {
  if (!rawOutput) return { cleanOutput: '', figures: [], closedFigureIds: [], closeAllFigures: false, errorLine: null };

  const figureMarker = '__FIGURE_DATA__:';
  const plotMarker = '__PLOT_DATA__:';
  const errorMarker = '__ERROR_LINE__:';
  const closeMarker = '__FIGURE_CLOSE__:';
  const closeAllMarker = '__FIGURE_CLOSE_ALL__';

  const lines = rawOutput.split('\n');
  const cleanLines = [];
  const figures = [];
  const closedFigureIds = [];
  let closeAllFigures = false;
  let errorLine = null;
  let legacyId = 1000;

  for (const line of lines) {
    const errIdx = line.indexOf(errorMarker);
    if (errIdx !== -1) {
      const num = parseInt(line.substring(errIdx + errorMarker.length).trim(), 10);
      if (!isNaN(num) && num > 0) errorLine = num;
      continue;
    }
    if (line.trim() === closeAllMarker) { closeAllFigures = true; continue; }
    const closeIdx = line.indexOf(closeMarker);
    if (closeIdx !== -1) {
      const id = parseInt(line.substring(closeIdx + closeMarker.length).trim(), 10);
      if (!isNaN(id)) closedFigureIds.push(id);
      continue;
    }
    const figIdx = line.indexOf(figureMarker);
    if (figIdx !== -1) {
      const before = line.substring(0, figIdx).trimEnd();
      if (before) cleanLines.push(before);
      const jsonStr = extractJson(line.substring(figIdx + figureMarker.length));
      if (jsonStr) { try { figures.push(JSON.parse(jsonStr)); } catch (e) { console.warn('[REPL] Failed to parse figure data:', e); } }
      continue;
    }
    const mIdx = line.indexOf(plotMarker);
    if (mIdx !== -1) {
      const before = line.substring(0, mIdx).trimEnd();
      if (before) cleanLines.push(before);
      const jsonStr = extractJson(line.substring(mIdx + plotMarker.length));
      if (jsonStr) {
        try {
          const legacy = JSON.parse(jsonStr);
          figures.push({ id: legacyId++, datasets: legacy.datasets.map(ds => ({ ...ds, type: legacy.config?.type || 'line' })), config: legacy.config || {} });
        } catch (e) { console.warn('[REPL] Failed to parse legacy plot data:', e); }
      }
      continue;
    }
    cleanLines.push(line);
  }
  return { cleanOutput: cleanLines.join('\n').trimEnd(), figures, closedFigureIds, closeAllFigures, errorLine };
}

function extractJson(str) {
  str = str.trim();
  if (!str.startsWith('{')) return null;
  let depth = 0, end = 0;
  for (let i = 0; i < str.length; i++) {
    if (str[i] === '{') depth++;
    else if (str[i] === '}') { depth--; if (depth === 0) { end = i + 1; break; } }
  }
  return end > 0 ? str.substring(0, end) : null;
}

function extractVarsData(rawOutput) {
  if (!rawOutput) return {};
  const marker = '__VARS__:';
  const idx = rawOutput.indexOf(marker);
  if (idx === -1) return {};
  try {
    const structured = JSON.parse(rawOutput.substring(idx + marker.length).trim());
    // Pass through structured data: { type, size, bytes, preview }
    return structured;
  } catch (e) { console.warn('[REPL] Failed to parse workspace JSON:', e); return {}; }
}

/**
 * Extract figure/close markers from debug result output.
 * Returns the result with cleanOutput, figures, closedFigureIds, closeAllFigures added.
 */
function enrichDebugResult(result) {
  if (result.output) {
    const extracted = extractMarkers(result.output);
    result.output = extracted.cleanOutput;
    result.figures = extracted.figures;
    result.closedFigureIds = extracted.closedFigureIds;
    result.closeAllFigures = extracted.closeAllFigures;
    if (extracted.errorLine) result.errorLine = extracted.errorLine;
  }
  return result;
}

function parseWorkspaceText(text) {
  if (!text || typeof text !== 'string') return {};
  const lines = text.split('\n').map(l => l.trimEnd()).filter(l => l.trim());
  if (lines.length === 0) return {};
  const lower = text.toLowerCase();
  if (lower.includes('no variables') || lower.includes('empty workspace')) return {};
  const vars = {};
  const headerIdx = lines.findIndex(l => /\bname\b/i.test(l) && (/\bsize\b/i.test(l) || /\bclass\b/i.test(l)));
  if (headerIdx !== -1) {
    for (let i = headerIdx + 1; i < lines.length; i++) {
      const line = lines[i].trim();
      if (!line || line.startsWith('-') || line.startsWith('=')) continue;
      const parts = line.split(/\s+/).filter(Boolean);
      if (parts.length >= 1) {
        const name = parts[0];
        if (/^[-=]+$/.test(name)) continue;
        vars[name] = buildPlaceholderValue({ _size: parts[1], _class: parts[2], _value: parts.slice(3).join(' ') });
      }
    }
    return vars;
  }
  const assignLines = lines.filter(l => /^\s*\w+\s*=/.test(l));
  if (assignLines.length > 0) {
    for (const line of assignLines) {
      const m = line.match(/^\s*(\w+)\s*=\s*(.+)$/);
      if (m) vars[m[1]] = tryParseValue(m[2].trim());
    }
    return vars;
  }
  for (const line of lines) {
    const names = line.trim().split(/\s+/).filter(n => /^[a-zA-Z_]\w*$/.test(n));
    for (const name of names) vars[name] = '?';
  }
  return vars;
}

function buildPlaceholderValue(entry) {
  const size = entry._size || '1x1';
  const cls = entry._class || 'double';
  if (entry._value) return tryParseValue(entry._value);
  if (size === '1x1') return cls === 'char' ? '<string>' : 0;
  const dm = size.match(/(\d+)x(\d+)/);
  if (dm) {
    const rows = parseInt(dm[1]), cols = parseInt(dm[2]);
    if (rows === 1) return new Array(cols).fill(0);
    return Array.from({ length: rows }, () => new Array(cols).fill(0));
  }
  return '?';
}

function tryParseValue(s) {
  s = s.trim();
  if (/^-?\d+(\.\d+)?([eE][+-]?\d+)?$/.test(s)) return parseFloat(s);
  if (/^'.*'$/.test(s)) return s.slice(1, -1);
  if (/^".*"$/.test(s)) return s.slice(1, -1);
  if (s.startsWith('[') && s.endsWith(']')) {
    const inner = s.slice(1, -1).trim();
    if (inner.includes(';')) return inner.split(';').map(row => row.trim().split(/[\s,]+/).map(Number));
    const nums = inner.split(/[\s,]+/).map(Number);
    if (nums.every(n => !isNaN(n))) return nums;
  }
  return s;
}

// ── WASM engine wrapper ──
export async function createWasmEngine(createModule) {
  const Module = await createModule({
    locateFile: (path) => {
      const base = import.meta.env.BASE_URL || '/';
      return base + path;
    },
    print: (text) => console.log('[WASM stdout]', text),
    printErr: (text) => console.warn('[WASM stderr]', text),
  });

  if (typeof Module.repl_init !== 'function') {
    throw new Error('repl_init not found in WASM module');
  }

  // Print a one-line binding audit so a stale WASM binary is obvious in
  // the console — nothing to dig through. Every entry should be 'function'
  // on the current build; 'undefined' means the WASM predates that API.
  console.log('[engine] WASM bindings:', {
    repl_init: typeof Module.repl_init,
    repl_execute: typeof Module.repl_execute,
    repl_register_fs: typeof Module.repl_register_fs,
    repl_push_script_origin: typeof Module.repl_push_script_origin,
    repl_pop_script_origin: typeof Module.repl_pop_script_origin,
  });

  // Expose the instance once so it's reachable from devtools as
  // `window.__mlabModule` for ad-hoc poking during debugging.
  if (typeof window !== 'undefined') window.__mlabModule = Module;

  return {
    type: 'wasm',
    init() { return Module.repl_init(); },

    execute(code) {
      const raw = Module.repl_execute(code);
      const { cleanOutput, figures, closedFigureIds, closeAllFigures, errorLine } = extractMarkers(raw);
      return { output: cleanOutput, figures, closedFigureIds, closeAllFigures, errorLine };
    },

    complete(partial) {
      const raw = Module.repl_complete(partial);
      if (!raw) return [];
      return raw.split(',').filter(Boolean);
    },

    reset() { return Module.repl_reset(); },
    workspace() { return Module.repl_workspace(); },

    getVars() {
      if (typeof Module.repl_get_vars === 'function') {
        const raw = Module.repl_get_vars();
        const parsed = extractVarsData(raw);
        if (Object.keys(parsed).length > 0) return parsed;
      }
      if (typeof Module.repl_workspace === 'function') {
        return parseWorkspaceText(Module.repl_workspace());
      }
      return {};
    },

    // ── Debug API ──
    get hasDebugger() {
      return typeof Module.repl_debug_start === 'function';
    },

    debugSetBreakpoints(lines) {
      if (typeof Module.repl_debug_set_breakpoints === 'function') {
        Module.repl_debug_set_breakpoints(JSON.stringify(lines));
      }
    },

    debugStart(code) {
      if (typeof Module.repl_debug_start !== 'function') {
        return { status: 'error', message: 'Debug not supported in this WASM build' };
      }
      const raw = Module.repl_debug_start(code);
      try {
        const result = JSON.parse(raw);
        return enrichDebugResult(result);
      } catch (e) { return { status: 'error', message: 'Failed to parse debug result' }; }
    },

    debugResume(action = 0) {
      if (typeof Module.repl_debug_resume !== 'function') {
        return { status: 'error', message: 'Debug not supported in this WASM build' };
      }
      const raw = Module.repl_debug_resume(action);
      try {
        const result = JSON.parse(raw);
        return enrichDebugResult(result);
      } catch (e) { return { status: 'error', message: 'Failed to parse debug result' }; }
    },

    debugStop() {
      if (typeof Module.repl_debug_stop === 'function') {
        Module.repl_debug_stop();
      }
    },

    // ── Virtual filesystem bridge ──
    //
    // Register a sync FS adapter under a name the engine will recognise
    // (typically 'temporary' or 'local'). Handler must expose sync methods
    // readFile(path) -> string, writeFile(path, content) -> void,
    // exists(path) -> boolean. See ide/src/fs/vfs-adapter.js for a
    // concrete adapter that mirrors temporary.js into a sync Map.
    //
    // If the WASM build predates the VFS bindings we warn loudly — with
    // the adapter present but the C++ side unaware, csvread/csvwrite
    // would fail at execution time with a confusing "filesystem 'X' is
    // not available" from the engine's path resolver.
    registerFs(name, handler) {
      if (typeof Module.repl_register_fs !== 'function') {
        if (!this._warnedStaleWasm) {
          console.warn('[engine] WASM binary is stale — missing VFS bindings. '
            + 'Rebuild the WASM module or hard-refresh to pick up the latest mlab_repl.{js,wasm}.');
          this._warnedStaleWasm = true;
        }
        return;
      }
      Module.repl_register_fs(name, handler);
    },

    // Tell the engine which FS the current script came from — so
    // csvread('foo.csv') with no explicit prefix and no MLAB_FS env var
    // resolves relative to that FS.
    pushScriptOrigin(fsName) {
      if (typeof Module.repl_push_script_origin === 'function') {
        Module.repl_push_script_origin(fsName);
      }
    },
    popScriptOrigin() {
      if (typeof Module.repl_pop_script_origin === 'function') {
        Module.repl_pop_script_origin();
      }
    },
  };
}

// ── Fallback JS engine ──
import { createInterpreter } from './interpreter';

export function createFallbackEngine() {
  const interp = createInterpreter();
  return {
    type: 'fallback',
    init() { return 'MLab REPL v2.5 — Demo Mode'; },

    execute(code) {
      const result = interp.execute(code);
      const figures = result.plot ? [{
        id: Date.now(),
        datasets: result.plot.datasets.map(ds => ({ ...ds, type: result.plot.config?.type || 'line' })),
        config: result.plot.config || {},
      }] : [];
      return { output: result.output, figures, closedFigureIds: [], closeAllFigures: false, errorLine: null };
    },

    complete(partial) { return interp.complete(partial); },
    reset() { interp.reset(); return 'Workspace cleared.'; },
    workspace() {
      const vars = interp.getVars();
      const keys = Object.keys(vars);
      if (!keys.length) return 'No variables.';
      return keys.join(', ');
    },
    getVars() { return interp.getVars(); },

    // ── Debug API (stub for fallback) ──
    get hasDebugger() { return false; },
    debugSetBreakpoints() {},
    debugStart() { return { status: 'error', message: 'Debug not available in demo mode' }; },
    debugResume() { return { status: 'error', message: 'Debug not available in demo mode' }; },
    debugStop() {},

    // ── VFS stubs — fallback engine has no file I/O ──
    registerFs() {},
    pushScriptOrigin() {},
    popScriptOrigin() {},
  };
}
