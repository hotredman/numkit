/**
 * editorOps.js — pure-logic editor operations.
 *
 * Every exported function takes a (text, selStart, selEnd, ...) tuple
 * and returns either null (no change) or { text, selStart, selEnd }
 * describing the new buffer state and where to place the caret /
 * selection afterwards. The functions never touch the DOM; the caller
 * (SyntaxEditor) drives React + textarea side-effects.
 *
 * Keeping the logic side-effect free has two benefits:
 *   1. Trivially unit-testable via vitest — no jsdom, no React.
 *   2. Composable — phase 2's line ops, phase 5's find/replace, etc.,
 *      all share the same Result<T> contract.
 *
 * Naming: every entrypoint starts with `apply…` (mutating intent) or
 * `find…` / `match…` (read-only query).
 */

/** Standard MATLAB-Editor indent width — matches every .m file in
 *  examples/, and what `mlint` expects. */
export const INDENT = '    ';
export const INDENT_LEN = INDENT.length;

/** MATLAB block-opener keywords. After Enter, when the previous line
 *  begins with one of these (trimmed), the new line gets +4 spaces of
 *  indent. */
const BLOCK_OPENERS = new Set([
  'for', 'parfor', 'while', 'if', 'switch', 'try',
  'function', 'classdef', 'properties', 'methods', 'events',
  'enumeration', 'spmd',
  // Branching keywords — middle of a construct, the body following
  // them is still indented one level deeper than the keyword line.
  'else', 'elseif', 'case', 'otherwise', 'catch',
]);

/** Keywords that, when typed alone on a line, should pull the line
 *  one indent level back (dedent). Detected after the user finishes
 *  typing the keyword (last char triggers the check). */
const DEDENT_KEYWORDS = new Set([
  'end', 'else', 'elseif', 'case', 'otherwise', 'catch',
]);

/** Characters that auto-close. Each maps "open" → "close". */
const AUTO_CLOSE_PAIRS = {
  '(': ')',
  '[': ']',
  '{': '}',
  '"': '"',
  "'": "'",
};

/** Find the start-of-line offset for a given char index. Returns 0
 *  for the first line. */
export function lineStartAt(text, offset) {
  return text.lastIndexOf('\n', Math.max(0, offset - 1)) + 1;
}

/** Find the end-of-line offset for a given char index. Returns
 *  text.length when there's no trailing newline. */
export function lineEndAt(text, offset) {
  const nl = text.indexOf('\n', offset);
  return nl === -1 ? text.length : nl;
}

/** Extract the leading whitespace (spaces only) of a line. */
export function leadingWhitespace(line) {
  const m = line.match(/^[ \t]*/);
  return m ? m[0] : '';
}

/** First non-whitespace token of a line, or '' if empty. Used to
 *  identify block-openers and dedent keywords. */
function firstWord(line) {
  const trimmed = line.replace(/^[ \t]+/, '');
  const m = trimmed.match(/^[A-Za-z_][A-Za-z0-9_]*/);
  return m ? m[0] : '';
}

// ───────────────────────────────────────────────────────────────────
// Tab / Shift+Tab
// ───────────────────────────────────────────────────────────────────

/**
 * Apply Tab or Shift+Tab.
 *
 * Behavior:
 *   • shift=false, single-line selection (or empty):
 *       replace selection with 4 spaces (MATLAB-Editor semantics).
 *   • shift=false, multi-line selection:
 *       prefix 4 spaces to every line in the block.
 *   • shift=true (always):
 *       strip up to 4 leading spaces from every line touched by
 *       the selection (or the current line when no selection).
 */
export function applyTab(text, selStart, selEnd, shift) {
  const multiLine = text.slice(selStart, selEnd).includes('\n');
  const useBlock = shift || multiLine;

  if (!useBlock) {
    const newText = text.slice(0, selStart) + INDENT + text.slice(selEnd);
    const caret = selStart + INDENT_LEN;
    return { text: newText, selStart: caret, selEnd: caret };
  }

  const blockStart = lineStartAt(text, selStart);
  let blockEnd = selEnd;
  if (selEnd === selStart || text[selEnd - 1] !== '\n') {
    blockEnd = lineEndAt(text, selEnd);
  }
  // Trim trailing \n from the block so split('\n') doesn't create
  // an empty tail element that would get spuriously indented (the
  // newline marks the boundary; the line AFTER it isn't ours).
  while (blockEnd > blockStart && text[blockEnd - 1] === '\n') {
    blockEnd -= 1;
  }
  const block = text.slice(blockStart, blockEnd);
  const lines = block.split('\n');
  let firstDelta = 0;
  let totalDelta = 0;
  const newLines = lines.map((ln, i) => {
    if (shift) {
      const m = ln.match(/^ {1,4}/);
      const removed = m ? m[0].length : 0;
      if (i === 0) firstDelta -= removed;
      totalDelta -= removed;
      return ln.slice(removed);
    }
    if (i === 0) firstDelta += INDENT_LEN;
    totalDelta += INDENT_LEN;
    return INDENT + ln;
  });
  const newText = text.slice(0, blockStart)
                + newLines.join('\n')
                + text.slice(blockEnd);
  const newSelStart = Math.max(blockStart, selStart + firstDelta);
  const newSelEnd = Math.max(newSelStart, selEnd + totalDelta);
  return { text: newText, selStart: newSelStart, selEnd: newSelEnd };
}

// ───────────────────────────────────────────────────────────────────
// Enter — auto-indent
// ───────────────────────────────────────────────────────────────────

/**
 * Apply Enter with auto-indent.
 *
 * The new line inherits the leading whitespace of the previous line.
 * If the previous line's first word is a MATLAB block-opener
 * (for/if/while/function/…), the new line additionally gets +4 spaces.
 *
 * If a selection is active, it's deleted first (matches Enter
 * semantics in every editor — replaces selection then inserts \n).
 */
export function applyEnter(text, selStart, selEnd) {
  // Compute the indent we'll inherit from the line BEFORE selStart.
  const lineStart = lineStartAt(text, selStart);
  const prevLine = text.slice(lineStart, selStart);
  const baseIndent = leadingWhitespace(prevLine);

  // Block-opener detection — check the first word of the FULL line
  // (selStart-truncated portion is enough since openers are at the
  // start). Skip if the line is purely a comment (starts with `%`).
  const trimmed = prevLine.replace(/^[ \t]+/, '');
  let extraIndent = '';
  if (!trimmed.startsWith('%')) {
    const word = firstWord(prevLine);
    if (BLOCK_OPENERS.has(word)) extraIndent = INDENT;
  }

  const insert = '\n' + baseIndent + extraIndent;
  const newText = text.slice(0, selStart) + insert + text.slice(selEnd);
  const caret = selStart + insert.length;
  return { text: newText, selStart: caret, selEnd: caret };
}

// ───────────────────────────────────────────────────────────────────
// Auto-close brackets / quotes
// ───────────────────────────────────────────────────────────────────

/**
 * Decide whether a `'` typed at this caret position should be
 * interpreted as transpose (don't auto-close) or string-open
 * (do auto-close). MATLAB rule: transpose binds to the right of
 * an identifier / number / `)` / `]` / `.`. Anywhere else, `'` opens
 * a string.
 */
function isTransposeContext(text, selStart) {
  if (selStart === 0) return false;
  const prev = text[selStart - 1];
  return /[A-Za-z0-9_)\].]/.test(prev);
}

/**
 * Apply auto-close for an opening bracket / quote.
 *
 * - Inserts the closing counterpart after the opener
 * - Leaves the caret between them
 * - If text is selected, wraps the selection (open before, close after)
 * - For `'`, skips auto-close in transpose context
 *
 * Returns null when the input char isn't a known opener.
 */
export function applyAutoClose(text, selStart, selEnd, openChar) {
  const closeChar = AUTO_CLOSE_PAIRS[openChar];
  if (!closeChar) return null;

  // `'` in transpose context: just insert the char without pairing.
  if (openChar === "'" && selStart === selEnd && isTransposeContext(text, selStart)) {
    return null;
  }

  if (selStart !== selEnd) {
    // Wrap selection.
    const selected = text.slice(selStart, selEnd);
    const newText = text.slice(0, selStart)
                  + openChar + selected + closeChar
                  + text.slice(selEnd);
    // Keep the original selection highlighted, shifted right by 1
    // (the open char). User can keep typing to replace, or press
    // right-arrow to land after the close char.
    return {
      text: newText,
      selStart: selStart + 1,
      selEnd: selEnd + 1,
    };
  }

  // Empty caret — insert pair, place caret between.
  const newText = text.slice(0, selStart)
                + openChar + closeChar
                + text.slice(selEnd);
  return {
    text: newText,
    selStart: selStart + 1,
    selEnd: selStart + 1,
  };
}

/**
 * Apply auto-skip — when the user types a closing char (`)`, `]`,
 * `}`, `'`, `"`) and the very next char in the buffer is the same,
 * we just advance the caret past it instead of inserting a duplicate.
 * Paired with auto-close so typing a balanced expression "just works".
 *
 * Returns null when no skip applies (caller should insert normally).
 */
export function applyAutoSkip(text, selStart, selEnd, typedChar) {
  if (selStart !== selEnd) return null;       // skip never applies to selections
  if (text[selStart] !== typedChar) return null;
  const closers = new Set([')', ']', '}', "'", '"']);
  if (!closers.has(typedChar)) return null;
  return {
    text,
    selStart: selStart + 1,
    selEnd: selStart + 1,
  };
}

/**
 * Apply auto-delete-pair — when the user presses Backspace with the
 * caret between an opener and its matching closer (e.g. `(|)`), we
 * delete BOTH chars so the pair vanishes together. Without this,
 * backspace leaves an orphaned closer behind.
 *
 * Returns null when no pair-deletion applies (caller should let
 * Backspace work normally).
 */
export function applyAutoDeletePair(text, selStart, selEnd) {
  if (selStart !== selEnd) return null;
  if (selStart === 0 || selStart >= text.length) return null;
  const prev = text[selStart - 1];
  const next = text[selStart];
  const expectedClose = AUTO_CLOSE_PAIRS[prev];
  if (!expectedClose || expectedClose !== next) return null;
  const newText = text.slice(0, selStart - 1) + text.slice(selStart + 1);
  return {
    text: newText,
    selStart: selStart - 1,
    selEnd: selStart - 1,
  };
}

// ───────────────────────────────────────────────────────────────────
// Smart dedent
// ───────────────────────────────────────────────────────────────────

/**
 * Apply smart dedent — after the user finishes typing a dedent
 * keyword (`end`, `else`, `elseif`, `case`, `otherwise`, `catch`)
 * on an otherwise-blank line, pull the line one indent level back.
 *
 * Trigger: every keystroke. We check whether the current line, with
 * the just-typed char included, has the shape `<spaces><kw>` and the
 * leading whitespace is ≥ INDENT_LEN. If so, strip INDENT_LEN spaces
 * from the start of the line.
 *
 * Pure function — caller invokes it AFTER inserting the typed char
 * (so `text` already includes the new char). Returns null when no
 * dedent applies.
 */
export function applySmartDedent(text, selStart, selEnd) {
  if (selStart !== selEnd) return null;
  const lineStart = lineStartAt(text, selStart);
  const lineEnd = lineEndAt(text, selStart);
  const line = text.slice(lineStart, lineEnd);
  const indent = leadingWhitespace(line);
  if (indent.length < INDENT_LEN) return null;
  const rest = line.slice(indent.length);
  // Match exactly one of the dedent keywords with no trailing tokens
  // beyond what's natural (whitespace / operators / `;`). For `end`
  // and `else`/`catch`/`otherwise` we accept the bare keyword OR
  // keyword followed by `;` / whitespace / `%comment`. For `elseif`
  // and `case` we accept keyword followed by some condition — they
  // commonly come with an expression on the same line.
  const m = rest.match(/^([A-Za-z]+)\b/);
  if (!m) return null;
  if (!DEDENT_KEYWORDS.has(m[1])) return null;
  // Skip when a proper prefix of this keyword is itself a dedent
  // keyword — that means dedent already fired earlier on this line
  // when the shorter keyword was completed (e.g. `else` triggered
  // dedent; continuing to type `if` should NOT dedent again).
  const word = m[1];
  for (let n = 1; n < word.length; n++) {
    if (DEDENT_KEYWORDS.has(word.slice(0, n))) return null;
  }
  // Caret must be right after the keyword (i.e. user just finished
  // typing the last char). Permits trailing space the user might
  // have added before tabbing — we only fire on the exact keystroke
  // that completed the keyword.
  if (selStart !== lineStart + indent.length + m[1].length) return null;

  const newIndent = indent.slice(INDENT_LEN);
  const newLine = newIndent + rest;
  const newText = text.slice(0, lineStart) + newLine + text.slice(lineEnd);
  const newCaret = lineStart + newIndent.length + (selStart - lineStart - indent.length);
  return {
    text: newText,
    selStart: newCaret,
    selEnd: newCaret,
  };
}

// ───────────────────────────────────────────────────────────────────
// Trim trailing whitespace
// ───────────────────────────────────────────────────────────────────

/**
 * Strip trailing spaces/tabs from every line, preserving line breaks.
 * Returns a new string; useful as a save-time cleanup or as an
 * explicit command bound to a menu/keybind.
 */
export function trimTrailingWhitespace(text) {
  return text.replace(/[ \t]+(?=\n|$)/g, '');
}

// ───────────────────────────────────────────────────────────────────
// Indent guides (VS Code-style)
// ───────────────────────────────────────────────────────────────────

/**
 * Compute indent-guide segments for the buffer. Each segment is a
 * contiguous vertical run at a single indent level — one rendered
 * `<span>` per segment instead of one per line.
 *
 * VS Code semantics:
 *   • Non-empty lines: guide level = floor(leading-spaces / 4).
 *   • Empty / blank lines INHERIT the MIN of the surrounding non-empty
 *     levels — so a blank line inside a `for` body shows the loop's
 *     indent, but a blank line BETWEEN unrelated blocks doesn't drag
 *     stray guides across it.
 *   • Tabs treated as 4 spaces for level computation (cheap heuristic
 *     — every .m file in examples/ uses spaces).
 *
 * Returns segments shape:
 *   [{ level: 1-based, startLine: 0-based, endLine: 0-based inclusive }]
 *
 * Pure function. Tested.
 */
export function computeIndentGuides(text) {
  const lines = text.split('\n');
  const N = lines.length;
  if (N === 0) return [];

  // First pass: per-line level. -1 = blank, to be filled.
  const lvl = new Array(N);
  for (let i = 0; i < N; i++) {
    const ln = lines[i];
    const m = ln.match(/^[ \t]*/);
    const lead = m ? m[0] : '';
    if (lead.length === ln.length) {
      lvl[i] = -1;          // blank line
    } else {
      // Tab counts as 4. Match the editor's INDENT_LEN convention.
      let cols = 0;
      for (const c of lead) cols += (c === '\t') ? 4 : 1;
      lvl[i] = Math.floor(cols / INDENT_LEN);
    }
  }

  // Second pass: fill blanks with min(prev-non-blank, next-non-blank).
  // Edge blanks (no prev/no next) get 0, so guides don't leak out.
  for (let i = 0; i < N; i++) {
    if (lvl[i] !== -1) continue;
    let prev = -1;
    for (let j = i - 1; j >= 0; j--) if (lvl[j] !== -1) { prev = lvl[j]; break; }
    let next = -1;
    for (let j = i + 1; j < N; j++) if (lvl[j] !== -1) { next = lvl[j]; break; }
    lvl[i] = Math.min(prev < 0 ? 0 : prev, next < 0 ? 0 : next);
  }

  // Third pass: for each level L, collect contiguous spans where
  // every covered line has lvl >= L. One segment per span.
  let maxL = 0;
  for (const v of lvl) if (v > maxL) maxL = v;
  const segments = [];
  for (let L = 1; L <= maxL; L++) {
    let start = -1;
    for (let i = 0; i < N; i++) {
      const has = lvl[i] >= L;
      if (has && start === -1) start = i;
      else if (!has && start !== -1) {
        segments.push({ level: L, startLine: start, endLine: i - 1 });
        start = -1;
      }
    }
    if (start !== -1) segments.push({ level: L, startLine: start, endLine: N - 1 });
  }
  return segments;
}

// ───────────────────────────────────────────────────────────────────
// Find / Replace
// ───────────────────────────────────────────────────────────────────

/**
 * Build a RegExp from a user query + options. Returns null for empty
 * or invalid input (so the caller can gracefully render "no match").
 *
 * Options:
 *   • caseSensitive  — case-sensitive matching when true
 *   • wholeWord      — wrap the (escaped) query in `\b…\b`
 *   • regex          — treat `query` as a regex pattern; otherwise
 *                      escape regex metacharacters in the query
 *
 * Returns a `g`-flagged regex so callers can lastIndex-step it.
 */
export function buildSearchRegex(query, opts = {}) {
  if (!query) return null;
  const flags = (opts.caseSensitive ? '' : 'i') + 'g';
  try {
    if (opts.regex) {
      return new RegExp(query, flags);
    }
    let pattern = query.replace(/[\\^$.*+?()[\]{}|]/g, '\\$&');
    if (opts.wholeWord) pattern = `\\b${pattern}\\b`;
    return new RegExp(pattern, flags);
  } catch {
    // Invalid regex — surface to caller as "no match".
    return null;
  }
}

/** All match ranges for the query in text. Stable order (left to right).
 *  Returns an array of { start, end } char offsets. */
export function findAllMatches(text, query, opts = {}) {
  const re = buildSearchRegex(query, opts);
  if (!re) return [];
  const out = [];
  let m;
  while ((m = re.exec(text)) !== null) {
    if (m[0].length === 0) {
      // Zero-width matches (e.g. /^/) would loop forever; advance.
      re.lastIndex += 1;
      continue;
    }
    out.push({ start: m.index, end: m.index + m[0].length });
  }
  return out;
}

/** Next match starting at or after `fromOffset`. Wraps around to the
 *  first match when no forward match exists. */
export function findNextMatch(text, fromOffset, query, opts = {}) {
  const all = findAllMatches(text, query, opts);
  if (all.length === 0) return null;
  for (const m of all) {
    if (m.start >= fromOffset) return m;
  }
  return all[0];
}

/** Previous match ending at or before `fromOffset`. Wraps around. */
export function findPrevMatch(text, fromOffset, query, opts = {}) {
  const all = findAllMatches(text, query, opts);
  if (all.length === 0) return null;
  for (let i = all.length - 1; i >= 0; i--) {
    if (all[i].end <= fromOffset) return all[i];
  }
  return all[all.length - 1];
}

/** Replace a single match range with the given replacement. Returns
 *  the standard { text, selStart, selEnd } where the selection covers
 *  the inserted text. */
export function applyReplace(text, match, replacement) {
  const newText = text.slice(0, match.start) + replacement + text.slice(match.end);
  return {
    text: newText,
    selStart: match.start,
    selEnd: match.start + replacement.length,
  };
}

/** Replace EVERY match. Returns the new text and how many replacements
 *  were performed. */
export function applyReplaceAll(text, query, replacement, opts = {}) {
  const re = buildSearchRegex(query, opts);
  if (!re) return { text, count: 0 };
  let count = 0;
  const out = text.replace(re, () => {
    count += 1;
    return replacement;
  });
  return { text: out, count };
}

// ───────────────────────────────────────────────────────────────────
// Bracket matching
// ───────────────────────────────────────────────────────────────────

const BRACKET_PAIR = { '(': ')', '[': ']', '{': '}',
                       ')': '(', ']': '[', '}': '{' };
const OPEN_BRACKETS = new Set(['(', '[', '{']);
const CLOSE_BRACKETS = new Set([')', ']', '}']);

/**
 * Find the matching bracket for whichever bracket is adjacent to
 * the caret. Adjacency rule (VS Code-style): prefer the char
 * IMMEDIATELY AFTER the caret; fall back to the char immediately
 * BEFORE it. Returns { open, close } with both char offsets, or
 * { open, close: -1 } / { open: -1, close } when unmatched.
 *
 * Plain scan with depth tracking — doesn't currently skip brackets
 * inside strings or comments. Acceptable v1: code rarely has unbalanced
 * literal brackets inside literals, and the visual highlight is a
 * helper, not a guarantee.
 */
export function findMatchingBracket(text, caretPos) {
  let pos = -1, ch = '';
  if (caretPos < text.length
      && (OPEN_BRACKETS.has(text[caretPos]) || CLOSE_BRACKETS.has(text[caretPos]))) {
    pos = caretPos; ch = text[caretPos];
  } else if (caretPos > 0
             && (OPEN_BRACKETS.has(text[caretPos - 1]) || CLOSE_BRACKETS.has(text[caretPos - 1]))) {
    pos = caretPos - 1; ch = text[caretPos - 1];
  }
  if (pos === -1) return null;

  const target = BRACKET_PAIR[ch];
  const isOpen = OPEN_BRACKETS.has(ch);
  let depth = 0;

  if (isOpen) {
    for (let i = pos + 1; i < text.length; i++) {
      if (text[i] === ch) depth += 1;
      else if (text[i] === target) {
        if (depth === 0) return { open: pos, close: i };
        depth -= 1;
      }
    }
    return { open: pos, close: -1 };
  }
  for (let i = pos - 1; i >= 0; i--) {
    if (text[i] === ch) depth += 1;
    else if (text[i] === target) {
      if (depth === 0) return { open: i, close: pos };
      depth -= 1;
    }
  }
  return { open: -1, close: pos };
}

// ───────────────────────────────────────────────────────────────────
// Selection match (highlight occurrences of selected identifier)
// ───────────────────────────────────────────────────────────────────

const IDENTIFIER_RE = /^[A-Za-z_][A-Za-z0-9_]*$/;

/**
 * If the selection covers a single identifier (no whitespace, no
 * punctuation), return every OTHER occurrence of that identifier as
 * a whole word. Used to draw the "selection match" overlay so the
 * user sees where else a variable or function name appears.
 *
 * Empty selections, multi-word selections, and non-identifier
 * selections all return [].
 */
export function findWordOccurrences(text, selStart, selEnd) {
  if (selStart === selEnd) return [];
  const sel = text.slice(selStart, selEnd);
  if (!IDENTIFIER_RE.test(sel)) return [];
  const all = findAllMatches(text, sel, { wholeWord: true, caseSensitive: true });
  return all.filter((m) => !(m.start === selStart && m.end === selEnd));
}

// ───────────────────────────────────────────────────────────────────
// Drag-drop (move / copy selection)
// ───────────────────────────────────────────────────────────────────

/**
 * Move or copy a range of text from `[srcStart, srcEnd)` to
 * `dstOffset`. `mode = 'move'` (default) deletes the source first
 * (and adjusts dstOffset by the deletion if dst was past the source);
 * `mode = 'copy'` leaves the source intact.
 *
 * Returns the standard { text, selStart, selEnd } with the selection
 * covering the inserted text. Returns null for:
 *   • empty source range
 *   • dropping inside the source range in 'move' mode (visually a no-op)
 *   • unrecognised mode
 *
 * Note: the native textarea already supports drag-drop of selected
 * text out-of-the-box; this helper exists for completeness, for unit
 * testing, and for programmatic callers (clipboard ops, refactors).
 */
export function applyDragDrop(text, srcStart, srcEnd, dstOffset, mode = 'move') {
  if (srcStart === srcEnd) return null;
  if (mode !== 'move' && mode !== 'copy') return null;
  const lo = Math.min(srcStart, srcEnd);
  const hi = Math.max(srcStart, srcEnd);
  const piece = text.slice(lo, hi);

  if (mode === 'copy') {
    const newText = text.slice(0, dstOffset) + piece + text.slice(dstOffset);
    return { text: newText, selStart: dstOffset, selEnd: dstOffset + piece.length };
  }

  // mode === 'move'
  if (dstOffset > lo && dstOffset < hi) return null;     // drop inside source
  let actualDst = dstOffset;
  if (dstOffset >= hi) actualDst -= (hi - lo);
  const without = text.slice(0, lo) + text.slice(hi);
  const newText = without.slice(0, actualDst) + piece + without.slice(actualDst);
  return { text: newText, selStart: actualDst, selEnd: actualDst + piece.length };
}

// ───────────────────────────────────────────────────────────────────
// Autocomplete
// ───────────────────────────────────────────────────────────────────

/**
 * Identify the identifier prefix immediately before the caret. The
 * autocomplete UI passes the resulting `value` to engine.complete()
 * and uses `start` to compute where to anchor the popup. Word chars
 * are `[A-Za-z0-9_]`; non-word chars before the caret stop the scan.
 *
 * Returns { start: caret-N, value: prefixString }. value is '' when
 * the caret is not adjacent to a word char.
 */
export function partialBeforeCaret(text, caret) {
  let i = caret;
  while (i > 0 && /[A-Za-z0-9_]/.test(text[i - 1])) i -= 1;
  return { start: i, value: text.slice(i, caret) };
}

/**
 * Apply an accepted completion. Replaces the identifier-prefix
 * immediately before the caret with `item`, places caret at the
 * end of the inserted text.
 *
 * Returns null when there's nothing to replace (caret not on a word).
 */
export function applyCompletion(text, caret, item) {
  const { start, value } = partialBeforeCaret(text, caret);
  if (!value) return null;
  const newText = text.slice(0, start) + item + text.slice(caret);
  const newCaret = start + item.length;
  return { text: newText, selStart: newCaret, selEnd: newCaret };
}

/**
 * Word-at-position helper — used by hover tooltips. Returns the
 * identifier under (or adjacent to) the given char offset, plus its
 * range. Returns null when not on an identifier.
 */
export function wordAt(text, offset) {
  if (offset < 0 || offset > text.length) return null;
  const isWord = (c) => c && /[A-Za-z0-9_]/.test(c);
  // Caret can be at the boundary — check both sides.
  let here = offset;
  if (!isWord(text[here]) && isWord(text[here - 1])) here -= 1;
  if (!isWord(text[here])) return null;
  let lo = here;
  while (lo > 0 && isWord(text[lo - 1])) lo -= 1;
  let hi = here;
  while (hi < text.length && isWord(text[hi])) hi += 1;
  // Identifiers must start with a letter or underscore (not a digit).
  if (!/[A-Za-z_]/.test(text[lo])) return null;
  return { start: lo, end: hi, word: text.slice(lo, hi) };
}

// ───────────────────────────────────────────────────────────────────
// Go-to-line
// ───────────────────────────────────────────────────────────────────

/** Compute the char offset of the start of the given 1-indexed line.
 *  Clamps to [first line, last line]. Returns the offset and the
 *  resolved (1-indexed) line number so callers can show what they
 *  jumped to. */
export function gotoLineOffset(text, line) {
  const target = Math.max(1, Math.floor(line));
  let pos = 0, ln = 1;
  for (let i = 0; i < text.length && ln < target; i++) {
    if (text[i] === '\n') {
      ln += 1;
      pos = i + 1;     // start of the next line
    }
  }
  return { offset: pos, line: ln };
}

// ───────────────────────────────────────────────────────────────────
// Block helpers — shared by comment-toggle / duplicate / move-line
// ───────────────────────────────────────────────────────────────────

/**
 * Compute the line-aligned block bounds for a selection. blockStart
 * is always at the start of the first affected line; blockEnd is the
 * end of the last affected line (no trailing newline). When selEnd
 * lands right after a `\n` (selection ends at line break), the
 * line AFTER it is NOT included — matches VS Code semantics.
 */
function blockBounds(text, selStart, selEnd) {
  const blockStart = lineStartAt(text, selStart);
  let blockEnd = selEnd;
  if (selEnd === selStart || text[selEnd - 1] !== '\n') {
    blockEnd = lineEndAt(text, selEnd);
  }
  while (blockEnd > blockStart && text[blockEnd - 1] === '\n') {
    blockEnd -= 1;
  }
  return { blockStart, blockEnd };
}

// ───────────────────────────────────────────────────────────────────
// Comment toggle (Ctrl+/)
// ───────────────────────────────────────────────────────────────────

/**
 * MATLAB line-comment prefix. `%` is the comment char; we add an
 * extra space (`% `) for readability when commenting in. When
 * uncommenting we strip `% ` if present, else just `%`.
 */
const COMMENT_PREFIX = '% ';

/**
 * Toggle MATLAB line comments on every line touched by the selection.
 *
 * Smart behaviour:
 *  • If ALL non-blank lines in the block already start with `%`
 *    (ignoring leading whitespace), strip the leading `%` (and one
 *    optional space) from each line.
 *  • Otherwise add `% ` right after the leading whitespace of each
 *    non-blank line. Blank lines are left untouched in either mode
 *    to keep the inserted comment column stable.
 *
 * Selection bounds are adjusted so the new block stays selected —
 * useful when toggling back and forth.
 */
export function applyCommentToggle(text, selStart, selEnd) {
  const { blockStart, blockEnd } = blockBounds(text, selStart, selEnd);
  const block = text.slice(blockStart, blockEnd);
  const lines = block.split('\n');

  // Detect "all non-blank lines already commented" — drives toggle direction.
  const nonBlank = lines.filter((ln) => ln.trim().length > 0);
  const allCommented = nonBlank.length > 0
    && nonBlank.every((ln) => /^[ \t]*%/.test(ln));

  let firstDelta = 0;
  let totalDelta = 0;
  const newLines = lines.map((ln, i) => {
    if (ln.trim().length === 0) return ln;     // skip blank lines
    const indent = leadingWhitespace(ln);
    const rest = ln.slice(indent.length);

    if (allCommented) {
      // Strip leading `%` + optional single space.
      const m = rest.match(/^%[ ]?/);
      const removed = m ? m[0].length : 0;
      if (i === 0) firstDelta -= removed;
      totalDelta -= removed;
      return indent + rest.slice(removed);
    }
    if (i === 0) firstDelta += COMMENT_PREFIX.length;
    totalDelta += COMMENT_PREFIX.length;
    return indent + COMMENT_PREFIX + rest;
  });
  const newText = text.slice(0, blockStart)
                + newLines.join('\n')
                + text.slice(blockEnd);
  const newSelStart = Math.max(blockStart, selStart + firstDelta);
  const newSelEnd   = Math.max(newSelStart, selEnd + totalDelta);
  return { text: newText, selStart: newSelStart, selEnd: newSelEnd };
}

// ───────────────────────────────────────────────────────────────────
// Duplicate line / selection (Ctrl+D)
// ───────────────────────────────────────────────────────────────────

/**
 * Duplicate the current line (when no selection) or the selected
 * text (when a selection is active). The duplicate is inserted
 * immediately after the source — i.e. line-clone goes onto a new
 * line below, selection-clone is concatenated right after the
 * selection end.
 *
 * Caret moves to the END of the duplicated content so successive
 * Ctrl+D calls produce 1, 2, 3, … copies in a tidy column.
 */
export function applyDuplicateLine(text, selStart, selEnd) {
  if (selStart !== selEnd) {
    // Selection: clone the selected text right after selEnd.
    const sel = text.slice(selStart, selEnd);
    const newText = text.slice(0, selEnd) + sel + text.slice(selEnd);
    return {
      text: newText,
      selStart: selEnd,
      selEnd: selEnd + sel.length,
    };
  }
  // No selection: clone the current line. Includes the leading
  // newline so the duplicate ends up on its own line below.
  const lineStart = lineStartAt(text, selStart);
  const lineEnd = lineEndAt(text, selStart);
  const line = text.slice(lineStart, lineEnd);
  const insertAt = lineEnd;
  const newText = text.slice(0, insertAt) + '\n' + line + text.slice(insertAt);
  // Place caret on the new line at the same column the user was at.
  const colOffset = selStart - lineStart;
  const newCaret = insertAt + 1 + colOffset;
  return {
    text: newText,
    selStart: newCaret,
    selEnd: newCaret,
  };
}

// ───────────────────────────────────────────────────────────────────
// Move line up / down (Alt+↑ / Alt+↓)
// ───────────────────────────────────────────────────────────────────

/**
 * Move the line(s) covered by the selection up or down by one line.
 * direction = -1 (up) or +1 (down). No-op when already at the top
 * (direction = -1) or bottom (direction = +1) of the buffer.
 *
 * Selection is preserved relative to the moved content so the user
 * can press Alt+↑ / Alt+↓ repeatedly to "carry" a line through the
 * document.
 */
export function applyMoveLine(text, selStart, selEnd, direction) {
  if (direction !== -1 && direction !== 1) return null;
  const { blockStart, blockEnd } = blockBounds(text, selStart, selEnd);

  if (direction === -1) {
    if (blockStart === 0) return null;          // already at top
    const prevStart = lineStartAt(text, blockStart - 1);
    const prevLine = text.slice(prevStart, blockStart - 1); // exclude its \n
    const block = text.slice(blockStart, blockEnd);
    const trailing = text.slice(blockEnd);
    // New layout: block, then \n, then prevLine, then trailing.
    const newText = text.slice(0, prevStart) + block + '\n' + prevLine + trailing;
    const delta = -(prevLine.length + 1);
    return {
      text: newText,
      selStart: selStart + delta,
      selEnd: selEnd + delta,
    };
  }

  // direction === +1
  if (blockEnd >= text.length) return null;     // already at bottom (no next line)
  // text[blockEnd] must be '\n' — block boundary is end-of-line.
  // The next line starts at blockEnd + 1.
  const nextStart = blockEnd + 1;
  const nextEnd = lineEndAt(text, nextStart);
  const nextLine = text.slice(nextStart, nextEnd);
  const block = text.slice(blockStart, blockEnd);
  const trailing = text.slice(nextEnd);
  // New layout: nextLine, \n, block, then trailing.
  const newText = text.slice(0, blockStart) + nextLine + '\n' + block + trailing;
  const delta = nextLine.length + 1;
  return {
    text: newText,
    selStart: selStart + delta,
    selEnd: selEnd + delta,
  };
}
