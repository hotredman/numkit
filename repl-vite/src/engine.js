/**
 * Engine abstraction layer.
 *
 * Both WASM and fallback engines expose a unified interface:
 *   init()              → string          (welcome message)
 *   execute(code)       → { output: string, plots: object[] }
 *   complete(partial)   → string[]        (completions)
 *   reset()             → string          (message)
 *   workspace()         → string          (whos output)
 *   getVars()           → object          (variable map for inspector)
 */

/**
 * Parse __PLOT_DATA__ and __ERROR_LINE__ markers out of WASM output.
 * Returns { cleanOutput, plots, errorLine }.
 */
function extractMarkers(rawOutput) {
  if (!rawOutput) return { cleanOutput: '', plots: [], errorLine: null };

  const plotMarker = '__PLOT_DATA__:';
  const errorMarker = '__ERROR_LINE__:';

  const lines = rawOutput.split('\n');
  const cleanLines = [];
  const plots = [];
  let errorLine = null;

  for (const line of lines) {
    // Check for error line marker
    const errIdx = line.indexOf(errorMarker);
    if (errIdx !== -1) {
      const num = parseInt(line.substring(errIdx + errorMarker.length).trim(), 10);
      if (!isNaN(num) && num > 0) errorLine = num;
      continue; // Don't include marker line in output
    }

    // Check for plot data marker
    const mIdx = line.indexOf(plotMarker);
    if (mIdx === -1) {
      cleanLines.push(line);
      continue;
    }
    const before = line.substring(0, mIdx).trimEnd();
    if (before) cleanLines.push(before);

    let jsonStr = line.substring(mIdx + plotMarker.length).trim();
    let depth = 0, jsonEnd = 0;
    for (let i = 0; i < jsonStr.length; i++) {
      if (jsonStr[i] === '{') depth++;
      else if (jsonStr[i] === '}') { depth--; if (depth === 0) { jsonEnd = i + 1; break; } }
    }
    if (jsonEnd > 0) jsonStr = jsonStr.substring(0, jsonEnd);

    try {
      plots.push(JSON.parse(jsonStr));
    } catch (e) {
      console.warn('[REPL] Failed to parse plot data:', e);
    }
  }

  return { cleanOutput: cleanLines.join('\n').trimEnd(), plots, errorLine };
}

/**
 * Parse __VARS__:{...json...} markers from WASM repl_get_vars output.
 * The JSON has structure: { "name": { "type":"double", "size":"1x1", "bytes":8, "preview":value }, ... }
 * We convert to simple values that VarInspector understands.
 */
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
        // Build placeholder from size info
        const sizeStr = info.size || '1x1';
        const m = sizeStr.match(/(\d+)x(\d+)/);
        if (m) {
          const rows = parseInt(m[1]), cols = parseInt(m[2]);
          if (rows === 1 && cols === 1) {
            result[name] = info.type === 'char' ? '<string>' : 0;
          } else if (rows === 1) {
            result[name] = new Array(cols).fill(0);
          } else {
            result[name] = Array.from({ length: rows }, () => new Array(cols).fill(0));
          }
        } else {
          result[name] = `[${sizeStr} ${info.type || 'unknown'}]`;
        }
      }
    }
    return result;
  } catch (e) {
    console.warn('[REPL] Failed to parse workspace JSON:', e);
    return {};
  }
}

/**
 * Parse text output from whos/workspace into a variable map.
 *
 * Expected formats from C++ repl_workspace():
 *
 * Format A (table):
 *   Name          Size        Class
 *   c             1x1         double
 *   myVec         1x5         double
 *
 * Format B (simple list):
 *   c = 1500
 *   f = 10000
 *
 * Format C (just names):
 *   c  f  lambda  R
 *
 * Returns an object like { c: { size: "1x1", class: "double" }, ... }
 * The VarInspector can display this even without actual values.
 */
function parseWorkspaceText(text) {
  if (!text || typeof text !== 'string') return {};

  const lines = text.split('\n').map(l => l.trimEnd()).filter(l => l.trim());
  if (lines.length === 0) return {};

  // Check for "No variables" messages
  const lower = text.toLowerCase();
  if (lower.includes('no variables') || lower.includes('empty workspace')) return {};

  const vars = {};

  // Try Format A: detect header line with "Name" and "Size" or "Class"
  const headerIdx = lines.findIndex(l =>
    /\bname\b/i.test(l) && (/\bsize\b/i.test(l) || /\bclass\b/i.test(l))
  );

  if (headerIdx !== -1) {
    // Parse table rows after header
    for (let i = headerIdx + 1; i < lines.length; i++) {
      const line = lines[i].trim();
      if (!line || line.startsWith('-') || line.startsWith('=')) continue;

      // Split by whitespace — expect: name size class [value...]
      const parts = line.split(/\s+/).filter(Boolean);
      if (parts.length >= 1) {
        const name = parts[0];
        // Skip if name looks like a separator or header
        if (/^[-=]+$/.test(name)) continue;

        const entry = { _name: name };
        if (parts.length >= 2) entry._size = parts[1];
        if (parts.length >= 3) entry._class = parts[2];
        if (parts.length >= 4) entry._value = parts.slice(3).join(' ');

        // Create a placeholder value based on size/class for display
        vars[name] = buildPlaceholderValue(entry);
      }
    }
    return vars;
  }

  // Try Format B: "name = value" lines
  const assignLines = lines.filter(l => /^\s*\w+\s*=/.test(l));
  if (assignLines.length > 0) {
    for (const line of assignLines) {
      const m = line.match(/^\s*(\w+)\s*=\s*(.+)$/);
      if (m) {
        const name = m[1];
        const rawVal = m[2].trim();
        vars[name] = tryParseValue(rawVal);
      }
    }
    return vars;
  }

  // Try Format C: space-separated names on one or more lines
  for (const line of lines) {
    const names = line.trim().split(/\s+/).filter(n => /^[a-zA-Z_]\w*$/.test(n));
    for (const name of names) {
      vars[name] = '?'; // Unknown value, but variable exists
    }
  }

  return vars;
}

/**
 * Build a placeholder value for VarInspector display from whos metadata.
 */
function buildPlaceholderValue(entry) {
  const size = entry._size || '1x1';
  const cls = entry._class || 'double';

  // If we have an actual value string, try to parse it
  if (entry._value) {
    return tryParseValue(entry._value);
  }

  // Build a descriptive placeholder
  if (size === '1x1') {
    if (cls === 'char') return '<string>';
    return 0; // scalar placeholder
  }

  // Matrix/vector — parse dimensions
  const dm = size.match(/(\d+)x(\d+)/);
  if (dm) {
    const rows = parseInt(dm[1]), cols = parseInt(dm[2]);
    if (rows === 1) return new Array(cols).fill(0);
    return Array.from({ length: rows }, () => new Array(cols).fill(0));
  }

  return '?';
}

/**
 * Try to parse a string value into a JS value.
 */
function tryParseValue(s) {
  s = s.trim();
  // Number
  if (/^-?\d+(\.\d+)?([eE][+-]?\d+)?$/.test(s)) return parseFloat(s);
  // String in quotes
  if (/^'.*'$/.test(s)) return s.slice(1, -1);
  if (/^".*"$/.test(s)) return s.slice(1, -1);
  // Vector [1 2 3]
  if (s.startsWith('[') && s.endsWith(']')) {
    const inner = s.slice(1, -1).trim();
    if (inner.includes(';')) {
      return inner.split(';').map(row =>
        row.trim().split(/[\s,]+/).map(Number)
      );
    }
    const nums = inner.split(/[\s,]+/).map(Number);
    if (nums.every(n => !isNaN(n))) return nums;
  }
  return s;
}

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

    /**
     * Execute code and return { output, plots } — same shape as fallback.
     * Plot data is extracted from __PLOT_DATA__ markers in the raw output.
     */
    execute(code) {
      const raw = Module.repl_execute(code);
      const { cleanOutput, plots, errorLine } = extractMarkers(raw);
      return { output: cleanOutput, plots, errorLine };
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

    /**
     * Get variables for the workspace inspector.
     * Strategy:
     * 1. If C++ exposes repl_get_vars() with __VARS__ JSON, use that.
     * 2. Otherwise, parse the text output of repl_workspace() (whos format).
     */
    getVars() {
      // Preferred: structured JSON from C++
      if (typeof Module.repl_get_vars === 'function') {
        const raw = Module.repl_get_vars();
        const parsed = extractVarsData(raw);
        if (Object.keys(parsed).length > 0) return parsed;
      }

      // Fallback: parse whos-style text output from workspace()
      if (typeof Module.repl_workspace === 'function') {
        return parseWorkspaceText(Module.repl_workspace());
      }

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
      return 'MLab REPL v2.2 — Demo Mode';
    },

    /**
     * Execute code and return { output, plots } — unified interface.
     */
    execute(code) {
      const result = interp.execute(code);
      const plots = result.plot ? [result.plot] : [];
      return { output: result.output, plots, errorLine: null };
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
      if (!keys.length) return 'No variables.';
      return keys.join(', ');
    },

    getVars() {
      return interp.getVars();
    },
  };
}
