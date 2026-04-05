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
 *   debugExecute(code, skipBp)  → { status, pauseState?, output?, ... }
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
    const result = {};
    for (const [name, info] of Object.entries(structured)) {
      if (info.preview !== null && info.preview !== undefined) {
        result[name] = info.preview;
      } else {
        const sizeStr = info.size || '1x1';
        const m = sizeStr.match(/(\d+)x(\d+)/);
        if (m) {
          const rows = parseInt(m[1]), cols = parseInt(m[2]);
          if (rows === 1 && cols === 1) result[name] = info.type === 'char' ? '<string>' : 0;
          else if (rows === 1) result[name] = new Array(cols).fill(0);
          else result[name] = Array.from({ length: rows }, () => new Array(cols).fill(0));
        } else {
          result[name] = `[${sizeStr} ${info.type || 'unknown'}]`;
        }
      }
    }
    return result;
  } catch (e) { console.warn('[REPL] Failed to parse workspace JSON:', e); return {}; }
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
      return typeof Module.repl_debug_execute === 'function';
    },

    debugSetBreakpoints(lines) {
      if (typeof Module.repl_debug_set_breakpoints === 'function') {
        Module.repl_debug_set_breakpoints(JSON.stringify(lines));
      }
    },

    debugExecute(code, skipBp = 0) {
      if (typeof Module.repl_debug_execute !== 'function') {
        return { status: 'error', message: 'Debug not supported in this WASM build' };
      }
      const raw = Module.repl_debug_execute(code, skipBp);
      try {
        return JSON.parse(raw);
      } catch (e) {
        return { status: 'error', message: 'Failed to parse debug result' };
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
    debugExecute() { return { status: 'error', message: 'Debug not available in demo mode' }; },
  };
}
