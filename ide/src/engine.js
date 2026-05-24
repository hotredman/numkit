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

// One-shot flag so we don't spam the console once per registerFs call
// when the WASM binary is stale. Module-scoped so the engine object
// itself stays free of diagnostic bookkeeping.
let warnedStaleWasm = false;

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

  // Expose the Emscripten Module on window so the heap-trace probe
  // (StatusBar HeapBadge) can read Module.HEAP8.byteLength to track
  // WASM linear memory growth — performance.memory ONLY reports V8's
  // JS heap and hides WASM allocations entirely, which is the
  // diagnostic gap the round-2 heap badge had on its own. Dev mode
  // additionally keeps the legacy `__numkitIdeModule` name for
  // hand-poking from devtools.
  if (typeof window !== 'undefined') {
    window.numkit = window.numkit || {};
    window.numkit.__module = Module;
    if (import.meta.env?.DEV) window.__numkitIdeModule = Module;
  }

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

    // Script-graph visualizer — parse + lower the given .m source to
    // a NodeGraph IR (statement-level data-flow graph). Returns the
    // already-parsed JS object (or `{ error: '...' }` on failure /
    // when the WASM binding is missing — older builds simply lack
    // buildScriptGraph and the caller renders an empty graph).
    buildScriptGraph(source) {
      if (typeof Module.buildScriptGraph !== 'function') {
        return { error: 'buildScriptGraph not available in this WASM build' };
      }
      try {
        const raw = Module.buildScriptGraph(source || '');
        return JSON.parse(raw);
      } catch (e) {
        console.warn('[engine] buildScriptGraph failed', e);
        return { error: e?.message || String(e) };
      }
    },

    // Literal parse-tree dump for the IDE's AST inspector view.
    // Returns the recursive AST JSON (see ast_serialize.cpp schema)
    // or `{ error: '...' }`.
    buildAST(source) {
      if (typeof Module.buildAST !== 'function') {
        return { error: 'buildAST not available in this WASM build' };
      }
      try {
        const raw = Module.buildAST(source || '');
        return JSON.parse(raw);
      } catch (e) {
        console.warn('[engine] buildAST failed', e);
        return { error: e?.message || String(e) };
      }
    },

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

    // Full matrix data for the Variable Editor table. Returns:
    //   { name, type, rows, cols, data: number[][] | string[][] | null[][] }
    // or { error: '...' } on failure (variable missing, stale WASM, etc.).
    // The repl_get_var_data binding was added in the v3 IDE; older WASM
    // builds simply lack it and we return null so the caller can fall back
    // to the preview-only data already in workspace().
    getVarData(name) {
      if (typeof Module.repl_get_var_data !== 'function') return null;
      try {
        const raw = Module.repl_get_var_data(name);
        return JSON.parse(raw);
      } catch (e) {
        console.warn('[engine] getVarData failed for', name, e);
        return { error: e?.message || String(e) };
      }
    },

    // Cheap dimension-only query — used to size the editor grid before
    // we know whether full or tile-mode fetching makes sense.
    //   { name, type, rows, cols, numel } | { error }
    getVarShape(name) {
      if (typeof Module.repl_get_var_shape !== 'function') return null;
      try { return JSON.parse(Module.repl_get_var_shape(name)); }
      catch (e) {
        console.warn('[engine] getVarShape failed for', name, e);
        return { error: e?.message || String(e) };
      }
    },

    // Tile fetch — returns a rectangular submatrix
    //   [r0..r0+rows) × [c0..c0+cols)  →  { r0, c0, rows, cols, type, data }
    // Used by VariableEditor for huge matrices where a full fetch would OOM.
    getVarTile(name, r0, c0, rows, cols) {
      if (typeof Module.repl_get_var_tile !== 'function') return null;
      try { return JSON.parse(Module.repl_get_var_tile(name, r0|0, c0|0, rows|0, cols|0)); }
      catch (e) {
        console.warn('[engine] getVarTile failed for', name, e);
        return { error: e?.message || String(e) };
      }
    },

    // Aggregate stats — { rows, cols, n, min, max, mean, hasNaN }.
    // Used by the VariableEditor heatmap in tile-mode where loading every
    // cell to JS would be impractical.
    getVarStats(name) {
      if (typeof Module.repl_get_var_stats !== 'function') return null;
      try { return JSON.parse(Module.repl_get_var_stats(name)); }
      catch (e) {
        console.warn('[engine] getVarStats failed for', name, e);
        return { error: e?.message || String(e) };
      }
    },

    // Source-grid tile fetcher (legacy JSON path). Returns
    //   { rows, cols, data: Uint8Array }
    // Stage C added the binary getFigureDisplayTile below — the IDE Heatmap
    // now uses that one. This stays callable for code that wants a source-
    // grid tile without resampling (e.g. fitColorsToVisible scans for stats).
    getFigureTile(figId, axIdx, dsIdx, r0, c0, h, w, lod) {
      if (typeof Module.repl_get_figure_tile !== 'function') return null;
      try {
        const raw = Module.repl_get_figure_tile(figId|0, axIdx|0, dsIdx|0,
                                                r0|0, c0|0, h|0, w|0, lod|0);
        const obj = JSON.parse(raw);
        if (obj.error) return obj;
        // After Stage A obj.data is a plain JSON array of uint8 indices.
        return { rows: obj.rows, cols: obj.cols,
                 data: Uint8Array.from(obj.data || []) };
      } catch (e) {
        console.warn('[engine] getFigureTile failed', e);
        return { error: e?.message || String(e) };
      }
    },

    // Display-grid tile fetcher with binary transit. Returns a fresh
    // Uint8Array of size displayH × displayW row-major (idx 255 = NaN).
    // The engine resamples zQuantized to display resolution in one pass,
    // applying log10 axis transforms when xLog/yLog is set.
    //
    // Behind the scenes: WASM returns a typed_memory_view INTO an engine-
    // side buffer that gets reused on the next call — we copy it into a
    // standalone Uint8Array so the consumer can hold it without races.
    getFigureDisplayTile(figId, axIdx, dsIdx,
                         srcR0, srcC0, srcH, srcW,
                         displayH, displayW, xLog, yLog) {
      if (typeof Module.repl_get_figure_display_tile !== 'function') return null;
      try {
        const view = Module.repl_get_figure_display_tile(
          figId|0, axIdx|0, dsIdx|0,
          +srcR0, +srcC0, +srcH, +srcW,
          displayH|0, displayW|0,
          !!xLog, !!yLog);
        if (!view) return null;
        // Copy the heap view into a standalone Uint8Array.
        return new Uint8Array(view);
      } catch (e) {
        console.warn('[engine] getFigureDisplayTile failed', e);
        return null;
      }
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
        if (!warnedStaleWasm) {
          console.warn('[engine] WASM binary is stale — missing VFS bindings. '
            + 'Rebuild the WASM module or hard-refresh to pick up the latest numkit_ide.{js,wasm}.');
          warnedStaleWasm = true;
        }
        return;
      }
      Module.repl_register_fs(name, handler);
    },

    // Tell the engine which FS the current script came from — so
    // csvread('foo.csv') with no explicit prefix and no NUMKIT_FS env var
    // resolves relative to that FS. The optional 2nd arg passes the
    // script's containing directory so sibling .m files resolve without
    // addpath (helper.m next to caller.m).
    pushScriptOrigin(fsName, scriptDir) {
      if (scriptDir &&
          typeof Module.repl_push_script_origin_with_dir === 'function') {
        Module.repl_push_script_origin_with_dir(fsName, scriptDir);
      } else if (typeof Module.repl_push_script_origin === 'function') {
        Module.repl_push_script_origin(fsName);
      }
    },
    popScriptOrigin() {
      if (typeof Module.repl_pop_script_origin === 'function') {
        Module.repl_pop_script_origin();
      }
    },

    // Engine build timestamp ("YYYY-MM-DD HH:MM:SS"), or null if the
    // WASM binary predates the binding.
    version() {
      if (typeof Module.repl_version !== 'function') return null;
      try { return Module.repl_version(); } catch { return null; }
    },
  };
}

// ── Fallback JS engine ──
import { createInterpreter } from './interpreter';

export function createFallbackEngine() {
  const interp = createInterpreter();
  return {
    type: 'fallback',
    init() { return 'Numkit IDE v2.5 — Demo Mode'; },

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
    getVarData(name) {
      // Fallback engine stores plain JS values — coerce to mockup shape.
      const vars = interp.getVars();
      const v = vars[name];
      if (v == null) return { error: `variable '${name}' not found` };
      if (typeof v === 'number') {
        return { name, type: 'double', rows: 1, cols: 1, data: [[v]] };
      }
      if (typeof v === 'string') {
        return { name, type: 'char', rows: 1, cols: v.length,
                 data: [v.split('').map((c) => c)] };
      }
      if (Array.isArray(v)) {
        if (v.length && Array.isArray(v[0])) {
          return { name, type: 'double', rows: v.length, cols: v[0].length,
                   data: v.map((r) => r.slice()) };
        }
        return { name, type: 'double', rows: 1, cols: v.length, data: [v.slice()] };
      }
      return { name, type: typeof v, rows: 1, cols: 1, data: [[String(v)]] };
    },
    getVarShape(name) {
      const r = this.getVarData(name);
      if (!r || r.error) return r;
      return { name, type: r.type, rows: r.rows, cols: r.cols, numel: r.rows * r.cols };
    },
    getVarTile(name, r0, c0, rows, cols) {
      const full = this.getVarData(name);
      if (!full || full.error) return full;
      const rEnd = Math.min(full.rows, r0 + rows);
      const cEnd = Math.min(full.cols, c0 + cols);
      const data = [];
      for (let r = r0; r < rEnd; r++) {
        const row = [];
        for (let c = c0; c < cEnd; c++) row.push(full.data[r]?.[c]);
        data.push(row);
      }
      return { r0, c0, rows: rEnd - r0, cols: cEnd - c0, type: full.type, data };
    },
    getVarStats(name) {
      const r = this.getVarData(name);
      if (!r || r.error) return r;
      let mn = Infinity, mx = -Infinity, sum = 0, n = 0, hasNaN = false;
      for (const row of r.data) for (const v of row) {
        if (typeof v !== 'number') continue;
        if (Number.isNaN(v)) { hasNaN = true; continue; }
        if (!Number.isFinite(v)) continue;
        if (v < mn) mn = v;
        if (v > mx) mx = v;
        sum += v; n++;
      }
      return { rows: r.rows, cols: r.cols, n, min: n ? mn : null,
               max: n ? mx : null, mean: n ? sum / n : null, hasNaN };
    },
    getFigureTile() { return null; },          // fallback engine doesn't track figures
    getFigureDisplayTile() { return null; },

    // Script-graph visualizer — fallback has no parser/AST; surface
    // a stable error-shape so the renderer can show "graph unavailable
    // in demo mode" instead of crashing.
    buildScriptGraph() {
      return { error: 'Graph view requires the WASM engine (demo mode is parser-less)' };
    },
    buildAST() {
      return { error: 'AST view requires the WASM engine (demo mode is parser-less)' };
    },

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
    version() { return null; },
  };
}
