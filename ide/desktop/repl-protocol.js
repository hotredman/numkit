// ide/desktop/repl-protocol.js
//
// Pure parsing logic for the native REPL pipe protocol.
// Extracted from ReplSession._flush() so it can be unit-tested without Electron.
//
// Exports:
//   extractFigureMarkers(output)
//     Strip __FIGURE_DATA__ / __FIGURE_CLOSE__ / __FIGURE_CLOSE_ALL__ from a
//     raw output string.  Returns { cleanOutput, figures, closedFigureIds,
//     closeAllFigures, errorLine }.
//
//   parseReplChunk(chunk, stderr, isStep)
//     Parse a response chunk (everything before the terminator marker).
//     isStep = true  → chunk came from __END_OF_STEP__ (a debug pause).
//     Returns one of:
//       { type: 'run',       stdout, stderr, vars, figures, closedFigureIds,
//                            closeAll, exitCode }
//       { type: 'data',      data }             introspection JSON result
//       { type: 'reset',     ok:true, vars }
//       { type: 'error',     error }
//       { type: 'dbstep',    status:'paused',   pauseState, output, figures,
//                            closedFigureIds, closeAllFigures }
//       { type: 'dbend',     status:'completed'|'error', output, figures,
//                            closedFigureIds, closeAllFigures, vars, message?, line? }
//       { type: 'dbstop' }
//
//   extractResponse(buf)
//     Find one complete response in a streaming buffer (checks for both
//     __END_OF_RUN__ and __END_OF_STEP__).
//     Returns { result, remainder, isStep } where result is parseReplChunk()
//     output, or { result:null } if the end-marker is not yet present.

// ── Figure / error-line marker extraction ────────────────────────────────────

const FIGURE_MARKER     = '__FIGURE_DATA__:';
const CLOSE_MARKER      = '__FIGURE_CLOSE__:';
const CLOSE_ALL_MARKER  = '__FIGURE_CLOSE_ALL__';
const ERROR_LINE_MARKER = '__ERROR_LINE__:';

/**
 * Strip figure/close markers from a raw output string.
 * Used for both regular run output and the `output` field in debug results.
 *
 * @param {string} output  Raw stdout/stderr text (may contain protocol markers)
 * @returns {{ cleanOutput:string, figures:object[], closedFigureIds:number[],
 *             closeAllFigures:boolean, errorLine:number|null }}
 */
export function extractFigureMarkers(output) {
  const lines          = (output || '').split('\n');
  const cleanLines     = [];
  const figures        = [];
  const closedFigureIds = [];
  let closeAllFigures  = false;
  let errorLine        = null;

  for (const line of lines) {
    // Error-line hint
    const errIdx = line.indexOf(ERROR_LINE_MARKER);
    if (errIdx !== -1) {
      const num = parseInt(line.substring(errIdx + ERROR_LINE_MARKER.length).trim(), 10);
      if (!isNaN(num) && num > 0) errorLine = num;
      continue;
    }
    // close all figures
    if (line.trim() === CLOSE_ALL_MARKER) { closeAllFigures = true; continue; }
    // close one figure
    const closeIdx = line.indexOf(CLOSE_MARKER);
    if (closeIdx !== -1) {
      const id = parseInt(line.substring(closeIdx + CLOSE_MARKER.length).trim(), 10);
      if (!isNaN(id)) closedFigureIds.push(id);
      continue;
    }
    // figure data
    const figIdx = line.indexOf(FIGURE_MARKER);
    if (figIdx !== -1) {
      const before = line.substring(0, figIdx).trimEnd();
      if (before) cleanLines.push(before);
      const jsonStr = _extractJson(line.substring(figIdx + FIGURE_MARKER.length));
      if (jsonStr) {
        try { figures.push(JSON.parse(jsonStr)); }
        catch (e) { console.warn('[repl-protocol] bad figure JSON:', e.message); }
      }
      continue;
    }
    cleanLines.push(line);
  }

  // trim trailing blank
  while (cleanLines.length && cleanLines[cleanLines.length - 1] === '') cleanLines.pop();

  return {
    cleanOutput: cleanLines.join('\n'),
    figures,
    closedFigureIds,
    closeAllFigures,
    errorLine,
  };
}

// Extract the first balanced JSON object from a string
function _extractJson(str) {
  str = str.trim();
  if (!str.startsWith('{')) return null;
  let depth = 0, end = 0;
  for (let i = 0; i < str.length; i++) {
    if (str[i] === '{') depth++;
    else if (str[i] === '}') { depth--; if (depth === 0) { end = i + 1; break; } }
  }
  return end > 0 ? str.substring(0, end) : null;
}

// ── Introspection markers (single-line command responses) ─────────────────────

const INTROSPECT_MARKERS = [
  '__VAR_DATA__:', '__SHAPE_DATA__:', '__PAGE_DATA__:',
  '__STATS_DATA__:', '__TILE_DATA__:', '__PATH_DATA__:',
  '__AST_DATA__:', '__GRAPH_DATA__:',
];

// ── Main chunk parser ─────────────────────────────────────────────────────────

/**
 * Parse one complete response chunk.
 *
 * @param {string}  chunk   Everything before the end-marker (CRLF already normalised)
 * @param {string}  stderr  Accumulated stderr for this request
 * @param {boolean} isStep  true when the terminator was __END_OF_STEP__
 */
export function parseReplChunk(chunk, stderr = '', isStep = false) {
  const text  = chunk.replace(/\r\n/g, '\n');
  const lines = text.split('\n');

  // ── Debug step (pause at breakpoint / step) ──────────────────────────────
  if (isStep) {
    const bpLine = lines.find(l => l.startsWith('__BREAKPOINT__:'));
    if (bpLine) {
      try {
        const parsed = JSON.parse(bpLine.slice('__BREAKPOINT__:'.length));
        // Enrich the output field exactly as engine.js enrichDebugResult() does
        const rawOutput = parsed.output || '';
        const { cleanOutput, figures, closedFigureIds, closeAllFigures, errorLine }
          = extractFigureMarkers(rawOutput);
        return {
          type:            'dbstep',
          status:          'paused',
          pauseState:      parsed.pauseState || null,
          output:          cleanOutput,
          figures,
          closedFigureIds,
          closeAllFigures,
          errorLine,
        };
      } catch (e) {
        return { type: 'error', error: 'Failed to parse __BREAKPOINT__ JSON: ' + e.message };
      }
    }
    return { type: 'error', error: 'No __BREAKPOINT__: line in debug-step chunk' };
  }

  // ── Debug stopped ─────────────────────────────────────────────────────────
  if (lines.find(l => l === '__DEBUG_STOPPED__')) {
    return { type: 'dbstop' };
  }

  // ── Debug completion (__DEBUG_END__) ──────────────────────────────────────
  if (lines.find(l => l === '__DEBUG_END__')) {
    let result = { status: 'completed' };
    const drLine = lines.find(l => l.startsWith('__DEBUG_RESULT__:'));
    if (drLine) {
      try { result = JSON.parse(drLine.slice('__DEBUG_RESULT__:'.length)); } catch { /* keep default */ }
    }
    // vars from __VARS__: line (present in debug completion)
    let vars = null;
    const vl = lines.find(l => l.startsWith('__VARS__:'));
    if (vl) try { vars = JSON.parse(vl.slice(9)); } catch { /* ignore */ }

    // Collect output lines (strip markers)
    const rawOutputLines = [];
    for (const l of lines) {
      if (l === '__DEBUG_END__') break;
      if (l.startsWith('__VARS__:') || l.startsWith('__DEBUG_RESULT__:')) continue;
      rawOutputLines.push(l);
    }
    while (rawOutputLines.length && rawOutputLines[rawOutputLines.length - 1] === '')
      rawOutputLines.pop();

    const { cleanOutput, figures, closedFigureIds, closeAllFigures, errorLine }
      = extractFigureMarkers(rawOutputLines.join('\n'));

    return {
      type:            'dbend',
      status:          result.status || 'completed',
      message:         result.message,
      line:            result.line,
      output:          cleanOutput,
      figures,
      closedFigureIds,
      closeAllFigures,
      errorLine,
      vars,
    };
  }

  // ── Introspection / command responses ─────────────────────────────────────
  for (const m of INTROSPECT_MARKERS) {
    const found = lines.find(l => l.startsWith(m));
    if (found) {
      try {
        return { type: 'data', data: JSON.parse(found.slice(m.length)) };
      } catch (e) {
        return { type: 'error', error: 'JSON parse error: ' + e.message, raw: found.slice(0, 200) };
      }
    }
  }

  // ── Reset acknowledgement ─────────────────────────────────────────────────
  if (lines.find(l => l === '__RESET_OK__')) {
    let vars = null;
    const vl = lines.find(l => l.startsWith('__VARS__:'));
    if (vl) try { vars = JSON.parse(vl.slice(9)); } catch { /* ignore */ }
    return { type: 'reset', ok: true, vars };
  }

  // ── Command-level error ───────────────────────────────────────────────────
  const errLine = lines.find(l => l.startsWith('__CMD_ERROR__:'));
  if (errLine) return { type: 'error', error: errLine.slice(14) };

  // ── Regular run response ──────────────────────────────────────────────────
  let vars = null;
  const rawOutputLines = [];
  for (const line of lines) {
    const trimmed = line.trimEnd();
    if (trimmed.startsWith('__VARS__:')) {
      try { vars = JSON.parse(trimmed.slice(9)); } catch { /* ignore */ }
    } else {
      rawOutputLines.push(line);
    }
  }
  while (rawOutputLines.length && rawOutputLines[rawOutputLines.length - 1] === '')
    rawOutputLines.pop();

  const { cleanOutput, figures, closedFigureIds, closeAllFigures, errorLine }
    = extractFigureMarkers(rawOutputLines.join('\n'));

  return {
    type:            'run',
    stdout:          cleanOutput,
    stderr,
    vars,
    figures,
    closedFigureIds,
    closeAll:        closeAllFigures,
    errorLine,
    exitCode:        0,
  };
}

// ── Streaming buffer splitter ─────────────────────────────────────────────────

const END_RUN  = '__END_OF_RUN__\n';
const END_STEP = '__END_OF_STEP__\n';

/**
 * Find one complete response in a streaming buffer (either end-marker).
 * Returns { result, remainder, isStep } where result is parseReplChunk() output,
 * or { result: null, remainder: buf } if no complete response is present yet.
 */
export function extractResponse(buf) {
  const normalized = buf.replace(/\r\n/g, '\n');

  const runIdx  = normalized.indexOf(END_RUN);
  const stepIdx = normalized.indexOf(END_STEP);

  // Pick whichever valid marker comes first
  let idx, markerLen, isStep;
  if (stepIdx !== -1 && (runIdx === -1 || stepIdx < runIdx)) {
    idx = stepIdx; markerLen = END_STEP.length; isStep = true;
  } else if (runIdx !== -1) {
    idx = runIdx;  markerLen = END_RUN.length;  isStep = false;
  } else {
    return { result: null, remainder: buf };
  }

  const chunk     = normalized.slice(0, idx);
  const remainder = normalized.slice(idx + markerLen);
  return { result: parseReplChunk(chunk, '', isStep), remainder, isStep };
}
