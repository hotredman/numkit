/**
 * inspectorOps.js — pure helpers for the struct/matrix Variable inspector.
 *
 * These build the MATLAB expressions the inspector feeds to
 * engine.execute() for read-write editing and struct-field management.
 * Keeping them pure makes the quoting/escaping (the security- and
 * correctness-critical part) unit-testable without React or the engine.
 */

/**
 * Convert a ';'-delimited inspect path (same format the C++ resolver
 * consumes) into a MATLAB lvalue rooted at `rootName`.
 *
 *   pathToMatlabLValue('car', '')                 → 'car'
 *   pathToMatlabLValue('car', 'f:engine;f:hp')    → 'car.engine.hp'
 *   pathToMatlabLValue('s',   'e:2;f:data')        → 's(3).data'
 *   pathToMatlabLValue('c',   'c:3')               → 'c{4}'
 *
 * Step kinds: f = field (.name), e = element index (struct array → (i+1)),
 * c = cell index ({i+1}). Indices are 0-based in the path, 1-based in
 * MATLAB. Field names are identifiers, so no escaping is needed.
 */
export function pathToMatlabLValue(rootName, pathStr) {
  let expr = rootName;
  if (!pathStr) return expr;
  for (const tok of pathStr.split(';')) {
    if (!tok) continue;
    const colon = tok.indexOf(':');
    if (colon === -1) continue;
    const kind = tok[0];
    const val = tok.slice(colon + 1);
    if (kind === 'f') expr += `.${val}`;
    else if (kind === 'e') expr += `(${(parseInt(val, 10) || 0) + 1})`;
    else if (kind === 'c') expr += `{${(parseInt(val, 10) || 0) + 1}}`;
  }
  return expr;
}

/**
 * Turn a user-entered cell value into a MATLAB RHS literal + the JS
 * value to mirror locally, based on the cell's type. Returns null when
 * the input is invalid for the type (caller aborts the edit).
 *
 *   { rhs: '3.14',   value: 3.14 }    // numeric
 *   { rhs: 'true',   value: true }    // logical
 *   { rhs: "'x'",    value: 'x' }     // char  (', escaped → '')
 *   { rhs: '"hi"',   value: 'hi' }    // string (", escaped → "")
 *
 * Escaping is the correctness/safety crux — the rhs is interpolated into
 * an engine.execute() string, so quotes must be doubled (MATLAB's escape)
 * to neither break the expression nor allow injection. Complex editing is
 * deferred (returns null).
 */
export function valueToMatlabRHS(input, type) {
  const t = String(type || 'double');

  if (t === 'logical') {
    const s = String(input).trim().toLowerCase();
    if (s === 'true' || s === '1')  return { rhs: 'true',  value: true };
    if (s === 'false' || s === '0') return { rhs: 'false', value: false };
    return null;
  }

  if (t === 'char') {
    const s = String(input);
    return { rhs: `'${s.replace(/'/g, "''")}'`, value: s };
  }

  if (t === 'string') {
    const s = String(input);
    return { rhs: `"${s.replace(/"/g, '""')}"`, value: s };
  }

  if (t === 'complex') return null;   // deferred

  // Numeric (double / single / int* / uint*) — also the fallback for
  // any unrecognised type: parseFloat → reject non-finite.
  const n = parseFloat(input);
  if (!Number.isFinite(n)) return null;
  return { rhs: String(n), value: n };
}

/** MATLAB identifier rule — used to validate new / renamed field names
 *  before building an assignment expression (also blocks injection). */
export function isValidIdentifier(name) {
  return /^[A-Za-z_][A-Za-z0-9_]*$/.test(String(name || ''));
}
