import { useRef, useState, useMemo, useCallback, useEffect, forwardRef, useImperativeHandle } from 'react';
import { useTheme, FONT, FONT_UI } from '../../theme';
import {
  applyTab, applyEnter,
  applyAutoClose, applyAutoSkip, applyAutoDeletePair,
  applySmartDedent,
  applyCommentToggle, applyDuplicateLine, applyMoveLine,
  trimTrailingWhitespace,
  findAllMatches, findNextMatch, findPrevMatch,
  applyReplace, applyReplaceAll, gotoLineOffset,
  findMatchingBracket, findWordOccurrences,
  partialBeforeCaret, applyCompletion,
  computeIndentGuides,
} from './editorOps';
import {
  createHistory, pushSnapshot, undo as historyUndo, redo as historyRedo,
  classifyChange,
} from './editorHistory';
import { buildHighlightHtml } from './editorHighlight';
import CompletionPopup from './CompletionPopup';
import { EditorGutter, EditorMinimap } from './editorChrome';

// ── Search-bar style helpers ───────────────────────────────────────
// Pulled out of the forwardRef body so the closures aren't recreated
// every render (the search-bar lives in a conditional branch and
// would otherwise rebuild its style objects each keystroke).
const searchBtnStyle = (C) => ({
  padding: '1px 6px',
  fontFamily: FONT_UI,
  fontSize: 11,
  background: C.bg1,
  color: C.text,
  border: `1px solid ${C.border}`,
  borderRadius: 2,
  cursor: 'pointer',
});
const searchToggleStyle = (C, active) => ({
  padding: '1px 6px',
  fontFamily: FONT_UI,
  fontSize: 11,
  background: active ? C.accent : C.bg1,
  color: active ? C.bg0 : C.text,
  border: `1px solid ${active ? C.accent : C.border}`,
  borderRadius: 2,
  cursor: 'pointer',
});

const SyntaxEditor = forwardRef(function SyntaxEditor({
  value, onChange, onScroll, onCursor,
  errorLine, debugLine,
  // Phase-4 visual chrome — all default-on so the editor looks
  // production-grade out of the box; callers (e.g. compact preview
  // panes) can opt out per-flag.
  showGutter = true,
  showMinimap = true,
  showIndentGuides = true,
  showCurrentLine = true,
  // Phase-8 IntelliSense. Pass the engine to enable autocomplete;
  // without it the popup never opens.
  engine = null,
}, ref) {
  const C = useTheme();
  const textareaRef = useRef(null);
  const highlightRef = useRef(null);
  const gutterRef = useRef(null);
  const minimapRef = useRef(null);
  const editorAreaRef = useRef(null);

  // Caret-line tracker drives both the current-line highlight band
  // and the minimap viewport marker. Starts at line 1 — empty buffer
  // has line 1 too.
  const [caretLine, setCaretLine] = useState(1);

  // Char-offset selection bounds — drive bracket matching and the
  // selection-match overlay. Updated by reportCursor on every caret
  // move / click / keyup.
  const [selBounds, setSelBounds] = useState({ start: 0, end: 0 });

  // Convert a char offset to (line, col) for overlay positioning.
  // Cheap O(N) scan — N is buffer-bounded, no caching needed.
  // Declared up here (not next to its bracket/word-occurrence
  // consumers) because tryAutocomplete's deps array reads it.
  const offsetToLineCol = useCallback((text, off) => {
    let line = 0, col = 0;
    const lim = Math.min(off, text.length);
    for (let i = 0; i < lim; i++) {
      if (text[i] === '\n') { line += 1; col = 0; }
      else col += 1;
    }
    return { line, col };
  }, []);

  // ── Autocomplete (IntelliSense) state ───────────────────────────
  // The popup re-uses Console autocomplete via engine.complete().
  // `acItems` empty == popup hidden.
  const [acItems, setAcItems] = useState([]);
  const [acIdx, setAcIdx] = useState(0);
  const [acAnchor, setAcAnchor] = useState({ line: 0, col: 0 });
  // Track caret offset when popup opened so we can re-anchor while
  // the user keeps typing (filtering down the list).
  const acAnchorOffsetRef = useRef(0);

  // ── Find / Replace / Go-to-line UI state ────────────────────────
  // All three bars share the same overlay slot at the top of the
  // editor area. `mode` enumerates which one is open ('' = closed).
  const [searchMode, setSearchMode] = useState(''); // '' | 'find' | 'replace' | 'goto'
  const [searchQuery, setSearchQuery] = useState('');
  const [searchReplace, setSearchReplace] = useState('');
  const [searchOpts, setSearchOpts] = useState({
    caseSensitive: false, wholeWord: false, regex: false,
  });
  const [gotoValue, setGotoValue] = useState('');
  const searchInputRef = useRef(null);

  // Search action callbacks (jumpToMatch / findNext / findPrev /
  // replaceCurrent / replaceAll / performGoto) are defined below,
  // AFTER applyResult — they reference it and would TDZ-throw if
  // declared higher. The order is: state up here, applyResult mid,
  // search actions below.

  // ── Undo/redo history ───────────────────────────────────────────
  // Lives in a ref (not state) — we don't render anything off of it
  // directly, and avoiding re-renders per keystroke matters. Group
  // semantics + cap are in editorHistory.js.
  const historyRef = useRef(createHistory({ text: value || '', selStart: 0, selEnd: 0 }));

  // External value changes (file load, programmatic setValue from the
  // parent) should reset history. Detect by comparing the incoming
  // value to whatever we last stored as the current history entry —
  // our own edits would have already pushed the new value, so the
  // current entry matches; external changes won't.
  useEffect(() => {
    const cur = historyRef.current.entries[historyRef.current.index];
    if (cur.text !== (value || '')) {
      historyRef.current = createHistory({ text: value || '', selStart: 0, selEnd: 0 });
    }
  }, [value]);

  /** Push a snapshot into the history ref. Wrapper so call sites
   *  don't have to construct the History object manually. */
  const recordHistory = useCallback((snap, kind) => {
    historyRef.current = pushSnapshot(historyRef.current, snap, kind, Date.now());
  }, []);
  useImperativeHandle(ref, () => ({
    get scrollTop() { return textareaRef.current?.scrollTop || 0; },
    focus: () => textareaRef.current?.focus(),
    /** Strip trailing whitespace from every line — exposed for a
     *  future toolbar/menu command. No-op for buffers without any
     *  trailing whitespace. Preserves caret on its current line. */
    trimTrailingWhitespace() {
      const ta = textareaRef.current;
      if (!ta) return;
      const cleaned = trimTrailingWhitespace(ta.value);
      if (cleaned === ta.value) return;
      const caret = ta.selectionStart;
      onChange(cleaned);
      requestAnimationFrame(() => {
        if (!textareaRef.current) return;
        // Caret may have landed in stripped whitespace — clamp to
        // the new line end.
        const pos = Math.min(caret, cleaned.length);
        textareaRef.current.selectionStart = pos;
        textareaRef.current.selectionEnd = pos;
      });
    },
    /** Move the caret to the given 1-indexed (line, col). Counts
     *  newlines in the current value to find the char offset, then
     *  uses setSelectionRange + focus so the user sees the cursor
     *  blink at the target. Centers the line vertically by setting
     *  scrollTop. Used by the AST → editor click-to-navigate
     *  handoff so jumping isn't just a highlight, it's a real
     *  caret move ready for typing/editing. */
    setCaret(line, col) {
      const ta = textareaRef.current;
      if (!ta) return;
      const v = ta.value || '';
      const targetLine = Math.max(1, line | 0);
      const targetCol  = Math.max(1, col  | 0);
      let pos = 0, ln = 1;
      while (pos < v.length && ln < targetLine) {
        if (v[pos] === '\n') ln += 1;
        pos += 1;
      }
      // pos is now at start of targetLine; advance col-1 chars,
      // clamped to end-of-line so we don't overshoot into the next.
      const eol = v.indexOf('\n', pos);
      const lineEnd = eol === -1 ? v.length : eol;
      pos = Math.min(pos + (targetCol - 1), lineEnd);
      ta.focus();
      ta.setSelectionRange(pos, pos);
      // Center the target line in the viewport (line-height matches
      // the inline CSS in the textarea below: 20px).
      const lineHeight = 20;
      const targetTop = (targetLine - 1) * lineHeight - ta.clientHeight / 2;
      ta.scrollTop = Math.max(0, targetTop);
    },
  }));

  /** Apply an editorOps result — push history, onChange, then
   *  restore selection in the next animation frame (the React
   *  commit needs to land first or the caret snaps back).
   *
   *  `kind` tags the change for history grouping. Defaults to 'op'
   *  so each call from an editorOps handler is its own undo step. */
  const applyResult = useCallback((r, kind = 'op') => {
    if (!r) return;
    recordHistory(r, kind);
    onChange(r.text);
    requestAnimationFrame(() => {
      const ta = textareaRef.current;
      if (!ta) return;
      ta.selectionStart = r.selStart;
      ta.selectionEnd   = r.selEnd;
    });
  }, [onChange, recordHistory]);

  // ── Search action callbacks — depend on applyResult ─────────────
  const jumpToMatch = useCallback((match) => {
    if (!match) return;
    const ta = textareaRef.current;
    if (!ta) return;
    ta.focus();
    ta.selectionStart = match.start;
    ta.selectionEnd = match.end;
    // Scroll the match into view by computing its line index and
    // centering the textarea's viewport on it.
    const before = (value || '').slice(0, match.start);
    const lineIdx = (before.match(/\n/g) || []).length;
    const editorLineH = 20;
    ta.scrollTop = Math.max(0, lineIdx * editorLineH - ta.clientHeight / 2);
  }, [value]);

  const findNext = useCallback(() => {
    const ta = textareaRef.current;
    if (!ta) return;
    const from = ta.selectionEnd;
    jumpToMatch(findNextMatch(value || '', from, searchQuery, searchOpts));
  }, [value, searchQuery, searchOpts, jumpToMatch]);

  const findPrev = useCallback(() => {
    const ta = textareaRef.current;
    if (!ta) return;
    const from = ta.selectionStart;
    jumpToMatch(findPrevMatch(value || '', from, searchQuery, searchOpts));
  }, [value, searchQuery, searchOpts, jumpToMatch]);

  const replaceCurrent = useCallback(() => {
    const ta = textareaRef.current;
    if (!ta) return;
    const selS = ta.selectionStart;
    const selE = ta.selectionEnd;
    // If the current selection IS the next match, replace it; else
    // jump to next so the user can confirm before clicking Replace.
    const m = findNextMatch(value || '', selS, searchQuery, searchOpts);
    if (!m) return;
    if (m.start !== selS || m.end !== selE) {
      jumpToMatch(m);
      return;
    }
    const r = applyReplace(value || '', m, searchReplace);
    applyResult(r, 'replace');
    requestAnimationFrame(() => {
      const nx = findNextMatch(r.text, r.selEnd, searchQuery, searchOpts);
      if (nx) jumpToMatch(nx);
    });
  }, [value, searchQuery, searchReplace, searchOpts, jumpToMatch, applyResult]);

  const replaceAll = useCallback(() => {
    const r = applyReplaceAll(value || '', searchQuery, searchReplace, searchOpts);
    if (r.count === 0) return;
    applyResult({ text: r.text, selStart: 0, selEnd: 0 }, 'replace-all');
  }, [value, searchQuery, searchReplace, searchOpts, applyResult]);

  const performGoto = useCallback(() => {
    const ln = parseInt(gotoValue, 10);
    if (!Number.isFinite(ln) || ln <= 0) return;
    const { offset } = gotoLineOffset(value || '', ln);
    const ta = textareaRef.current;
    if (!ta) return;
    ta.focus();
    ta.selectionStart = offset;
    ta.selectionEnd = offset;
    const editorLineH = 20;
    ta.scrollTop = Math.max(0, (ln - 1) * editorLineH - ta.clientHeight / 2);
    setSearchMode('');
  }, [gotoValue, value]);

  // ── Autocomplete helpers ────────────────────────────────────────
  /** Try to open / refresh the autocomplete popup based on the
   *  current caret position. Closes it when the partial is shorter
   *  than 2 chars or yields no items. */
  const tryAutocomplete = useCallback(() => {
    const ta = textareaRef.current;
    if (!ta || !engine || typeof engine.complete !== 'function') return;
    const text = ta.value;
    const caret = ta.selectionStart;
    const { start, value: partial } = partialBeforeCaret(text, caret);
    if (partial.length < 2) { setAcItems([]); return; }
    const raw = engine.complete(partial) || [];
    // engine.complete() returns prefix-matching results. Filter case-
    // insensitive just in case + dedupe.
    const items = Array.from(new Set(raw));
    if (items.length === 0) { setAcItems([]); return; }
    setAcItems(items);
    setAcIdx(0);
    acAnchorOffsetRef.current = start;
    const { line, col } = offsetToLineCol(text, start);
    setAcAnchor({ line, col });
  }, [engine, offsetToLineCol]);

  const closeAutocomplete = useCallback(() => setAcItems([]), []);

  const acceptCompletion = useCallback((item) => {
    const ta = textareaRef.current;
    if (!ta) return;
    const r = applyCompletion(ta.value, ta.selectionStart, item);
    if (r) applyResult(r, 'autocomplete');
    setAcItems([]);
  }, [applyResult]);

  // All matches of the open find/replace query — drives both the
  // "n / m" counter and the highlight-all overlay (VS Code-style).
  const searchMatches = useMemo(() => {
    if ((searchMode !== 'find' && searchMode !== 'replace') || !searchQuery) return [];
    return findAllMatches(value || '', searchQuery, searchOpts);
  }, [searchMode, searchQuery, searchOpts, value]);

  // Counter "n / m" — active index = the match under the caret.
  const matchInfo = useMemo(() => {
    if (searchMatches.length === 0) return { count: 0, activeIdx: -1 };
    const ta = textareaRef.current;
    const pos = ta ? ta.selectionStart : 0;
    let idx = -1;
    for (let i = 0; i < searchMatches.length; i++) {
      if (searchMatches[i].start <= pos && searchMatches[i].end >= pos) { idx = i; break; }
    }
    return { count: searchMatches.length, activeIdx: idx };
  }, [searchMatches]);

  /** Master keydown dispatcher.
   *
   *  Tab / Shift+Tab     → applyTab        (indent / outdent)
   *  Enter               → applyEnter      (auto-indent + block opener)
   *  Backspace           → applyAutoDeletePair (kill empty bracket pair)
   *  ( [ { ' "           → applyAutoClose  (insert pair)
   *  ) ] } ' "           → applyAutoSkip   (advance past matching close)
   *
   *  Each handler is a pure function in editorOps.js; this layer is
   *  just dispatch + side-effects. Smart-dedent (typing `end` etc. on
   *  an indented line) runs in onChangeInternal because it needs to
   *  observe the AFTER-text state, which only exists once the native
   *  input has applied. */
  const onKeyDown = useCallback((e) => {
    const ta = textareaRef.current;
    if (!ta) return;
    const text = ta.value;
    const selStart = ta.selectionStart;
    const selEnd   = ta.selectionEnd;

    // ── Autocomplete popup — intercept nav keys when open ───────
    if (acItems.length > 0) {
      if (e.key === 'ArrowDown') {
        e.preventDefault();
        setAcIdx((i) => (i + 1) % acItems.length);
        return;
      }
      if (e.key === 'ArrowUp') {
        e.preventDefault();
        setAcIdx((i) => (i - 1 + acItems.length) % acItems.length);
        return;
      }
      if (e.key === 'Enter' || e.key === 'Tab') {
        e.preventDefault();
        acceptCompletion(acItems[acIdx]);
        return;
      }
      if (e.key === 'Escape') {
        e.preventDefault();
        closeAutocomplete();
        return;
      }
      // Any other key falls through to native typing; the next
      // onChangeInternal will refresh the popup.
    }

    // Ctrl+Space — manually trigger autocomplete.
    if ((e.ctrlKey || e.metaKey) && !e.altKey && !e.shiftKey && e.key === ' ') {
      e.preventDefault();
      tryAutocomplete();
      return;
    }

    // Ctrl+F — open find bar.
    if ((e.ctrlKey || e.metaKey) && !e.altKey && !e.shiftKey
        && (e.key === 'f' || e.key === 'F')) {
      e.preventDefault();
      setSearchMode('find');
      // Pre-fill query with current selection (VS Code does this) so
      // hitting Ctrl+F on a selected word opens the bar ready to F3.
      const ta = textareaRef.current;
      if (ta && ta.selectionStart !== ta.selectionEnd) {
        setSearchQuery((value || '').slice(ta.selectionStart, ta.selectionEnd));
      }
      requestAnimationFrame(() => searchInputRef.current?.focus());
      return;
    }
    // Ctrl+H — open find + replace.
    if ((e.ctrlKey || e.metaKey) && !e.altKey && !e.shiftKey
        && (e.key === 'h' || e.key === 'H')) {
      e.preventDefault();
      setSearchMode('replace');
      const ta = textareaRef.current;
      if (ta && ta.selectionStart !== ta.selectionEnd) {
        setSearchQuery((value || '').slice(ta.selectionStart, ta.selectionEnd));
      }
      requestAnimationFrame(() => searchInputRef.current?.focus());
      return;
    }
    // Ctrl+G — go to line.
    if ((e.ctrlKey || e.metaKey) && !e.altKey && !e.shiftKey
        && (e.key === 'g' || e.key === 'G')) {
      e.preventDefault();
      setSearchMode('goto');
      setGotoValue('');
      requestAnimationFrame(() => searchInputRef.current?.focus());
      return;
    }
    // F3 / Shift+F3 — next / previous match (works from inside the
    // textarea even when the find bar isn't focused).
    if (e.key === 'F3') {
      e.preventDefault();
      if (searchQuery) (e.shiftKey ? findPrev : findNext)();
      return;
    }
    // Esc — close any open search bar and return focus to the
    // editor. Only when a bar is open (otherwise let native handle it).
    if (e.key === 'Escape' && searchMode) {
      e.preventDefault();
      setSearchMode('');
      textareaRef.current?.focus();
      return;
    }

    // Ctrl+Z — undo (Ctrl+Shift+Z and Ctrl+Y both redo, matching
    // every editor we've ever used).
    if ((e.ctrlKey || e.metaKey) && !e.altKey
        && (e.key === 'z' || e.key === 'Z' || e.key === 'y' || e.key === 'Y')) {
      const wantRedo = (e.key === 'y' || e.key === 'Y') || e.shiftKey;
      e.preventDefault();
      const action = wantRedo ? historyRedo : historyUndo;
      const out = action(historyRef.current);
      if (!out) return;     // already at the end
      historyRef.current = out.history;
      const snap = out.snapshot;
      onChange(snap.text);
      requestAnimationFrame(() => {
        const ta2 = textareaRef.current;
        if (!ta2) return;
        ta2.selectionStart = snap.selStart;
        ta2.selectionEnd   = snap.selEnd;
      });
      return;
    }

    // Tab — handled regardless of modifier (Shift+Tab = outdent).
    if (e.key === 'Tab') {
      e.preventDefault();
      applyResult(applyTab(text, selStart, selEnd, e.shiftKey), 'tab');
      return;
    }

    // Ctrl+/ — toggle MATLAB line comments on the selected lines.
    // `e.key === '/'` covers both US layout and the Cmd+/ Mac variant.
    if ((e.ctrlKey || e.metaKey) && !e.altKey && !e.shiftKey && e.key === '/') {
      e.preventDefault();
      applyResult(applyCommentToggle(text, selStart, selEnd), 'comment');
      return;
    }

    // Ctrl+D — duplicate current line or selection.
    if ((e.ctrlKey || e.metaKey) && !e.altKey && !e.shiftKey
        && (e.key === 'd' || e.key === 'D')) {
      e.preventDefault();
      applyResult(applyDuplicateLine(text, selStart, selEnd), 'duplicate');
      return;
    }

    // Alt+↑ / Alt+↓ — move line(s) up / down.
    if (e.altKey && !e.ctrlKey && !e.metaKey && !e.shiftKey
        && (e.key === 'ArrowUp' || e.key === 'ArrowDown')) {
      const dir = e.key === 'ArrowUp' ? -1 : 1;
      const r = applyMoveLine(text, selStart, selEnd, dir);
      if (r) {
        e.preventDefault();
        applyResult(r, 'move');
      }
      // No-op at top/bottom — let native behavior pass (will move
      // caret but content stays put).
      return;
    }

    // Enter — auto-indent. Skip when Ctrl/Shift/Alt held (those have
    // special meanings: Ctrl+Enter = "run", Shift+Enter = soft break).
    if (e.key === 'Enter' && !e.ctrlKey && !e.altKey && !e.metaKey && !e.shiftKey) {
      e.preventDefault();
      applyResult(applyEnter(text, selStart, selEnd), 'newline');
      return;
    }

    // Backspace — collapse empty bracket pairs in one stroke. Falls
    // through to native backspace when the caret isn't between an
    // open+close pair.
    if (e.key === 'Backspace' && !e.ctrlKey && !e.altKey && !e.metaKey) {
      const r = applyAutoDeletePair(text, selStart, selEnd);
      if (r) {
        e.preventDefault();
        applyResult(r, 'delete-pair');
        return;
      }
    }

    // Auto-close openers. Single-char keys only — modifiers like
    // Ctrl+( aren't valid input.
    if (!e.ctrlKey && !e.altKey && !e.metaKey
        && ['(', '[', '{', '"', "'"].includes(e.key)) {
      const r = applyAutoClose(text, selStart, selEnd, e.key);
      if (r) {
        e.preventDefault();
        applyResult(r, 'auto-close');
        return;
      }
      // null → transpose-context for `'`, let native typing through.
    }

    // Auto-skip closers — when the next char in the buffer matches
    // what the user typed, just advance past it. No history push:
    // text didn't change, only caret moved.
    if (!e.ctrlKey && !e.altKey && !e.metaKey
        && [')', ']', '}', '"', "'"].includes(e.key)) {
      const r = applyAutoSkip(text, selStart, selEnd, e.key);
      if (r) {
        e.preventDefault();
        const ta2 = textareaRef.current;
        if (ta2) {
          ta2.selectionStart = r.selStart;
          ta2.selectionEnd   = r.selEnd;
        }
      }
    }
  }, [applyResult, onChange, value, searchMode, searchQuery,
      findNext, findPrev,
      acItems, acIdx, acceptCompletion, closeAutocomplete, tryAutocomplete]);

  /** Wraps the parent's onChange so smart-dedent can fire on every
   *  text mutation. Smart-dedent triggers when the user finishes
   *  typing a keyword like `end` on an otherwise-blank indented line;
   *  the line gets pulled one indent level back automatically.
   *
   *  Only checked when the change appears to come from a single-char
   *  insert at the caret (cheap heuristic: new length = old length + 1).
   *  Avoids running on paste / multi-char inserts where dedent rules
   *  would mis-fire. */
  const onChangeInternal = useCallback((e) => {
    const newValue = e.target.value;
    const ta = e.target;
    const oldText = value || '';
    const kind = classifyChange(oldText, newValue);
    const typedSnapshot = {
      text: newValue,
      selStart: ta.selectionStart,
      selEnd: ta.selectionEnd,
    };
    // Always record the typed state first — preserves a stop on the
    // undo stack between "raw typing" and any auto-fix that follows.
    recordHistory(typedSnapshot, kind);

    const oneCharInsert = newValue.length === oldText.length + 1
                       && ta.selectionStart === ta.selectionEnd;
    if (oneCharInsert) {
      const r = applySmartDedent(newValue, ta.selectionStart, ta.selectionEnd);
      if (r) {
        // Smart-dedent layers a SECOND history entry so Ctrl+Z first
        // un-dedents, then the next Ctrl+Z un-types the keyword.
        applyResult(r, 'auto-fix');
        return;
      }
      // Autocomplete trigger — once the typed char extends an
      // identifier to ≥ 2 chars, re-query the engine. Schedule for
      // the next frame so the textarea's selection has settled.
      const typedChar = newValue[ta.selectionStart - 1];
      if (typedChar && /[A-Za-z0-9_]/.test(typedChar)) {
        requestAnimationFrame(() => tryAutocomplete());
      } else {
        setAcItems([]);   // non-word char ends the completion context
      }
    } else if (newValue.length < oldText.length) {
      // Backspace inside an active popup → refresh; one-char delete
      // outside → close.
      if (acItems.length > 0) {
        requestAnimationFrame(() => tryAutocomplete());
      }
    }
    onChange(newValue);
  }, [onChange, value, applyResult, recordHistory, tryAutocomplete, acItems.length]);

  /** Convert the textarea's caret char-offset to a 1-indexed
   *  (line, col) pair and fire onCursor. Cheap O(N) scan of the
   *  value up to the offset — N is bounded by script size which
   *  is small in practice. */
  const reportCursor = useCallback(() => {
    if (!textareaRef.current) return;
    const ta = textareaRef.current;
    const pos = ta.selectionStart || 0;
    const v = value || '';
    let line = 1, col = 1;
    for (let i = 0; i < pos && i < v.length; ++i) {
      if (v[i] === '\n') { line += 1; col = 1; }
      else                col += 1;
    }
    setCaretLine(line);
    setSelBounds({ start: ta.selectionStart, end: ta.selectionEnd });
    if (onCursor) onCursor(line, col);
  }, [value, onCursor]);

  // ── Live highlight overlays ─────────────────────────────────────
  // Bracket pair: caret-adjacent bracket + its match (or red flag
  // when unmatched). Word occurrences: every whole-word match of
  // the current selection (when it's an identifier).
  // offsetToLineCol moved up to the state block — it's a dep of
  // tryAutocomplete and would TDZ-throw if declared here.
  const bracketMatch = useMemo(
    () => findMatchingBracket(value || '', selBounds.start),
    [value, selBounds.start]
  );
  const wordOccurrences = useMemo(
    () => findWordOccurrences(value || '', selBounds.start, selBounds.end),
    [value, selBounds.start, selBounds.end]
  );

  const colorMap = { keyword: C.synKeyword, builtin: C.synBuiltin, number: C.synNumber, string: C.synString, comment: C.synComment, operator: C.synOperator, constant: C.synConstant, param: C.synParam, plain: C.text };

  // Track the textarea's scrollTop in state so the minimap viewport
  // indicator re-renders when the user scrolls. (Re-renders aren't
  // free, but the rate is bounded by scroll events, which are already
  // throttled by the browser.)
  const [scrollTop, setScrollTop] = useState(0);

  // Minimap container size — kept in state so the canvas redraws when
  // the editor pane is resized (e.g. another pane squeezes it). The
  // canvas backing-store must match the container's pixel size; without
  // this the canvas held its old dimensions until an unrelated redraw
  // (a click → scroll) re-measured it. A ResizeObserver (below) writes
  // here, and the draw effect lists mapSize in its deps.
  const [mapSize, setMapSize] = useState({ w: 0, h: 0 });

  const syncScroll = useCallback(() => {
    const ta = textareaRef.current;
    if (!ta) return;
    if (highlightRef.current) {
      highlightRef.current.scrollTop = ta.scrollTop;
      highlightRef.current.scrollLeft = ta.scrollLeft;
    }
    if (gutterRef.current) {
      // Gutter only scrolls vertically — line numbers don't extend
      // off the right edge.
      gutterRef.current.scrollTop = ta.scrollTop;
    }
    setScrollTop(ta.scrollTop);
    if (onScroll) onScroll(ta.scrollTop);
  }, [onScroll]);

  // Build HTML with per-line <span style="display:block"> for line highlighting.
  // This keeps everything inside a single <pre> so scroll sync works perfectly.
  // Line count is just newline count + 1 — cheap + independent of the
  // highlight pipeline (which now lives in editorHighlight.js).
  const lineCount = (value || '').split('\n').length;
  const html = buildHighlightHtml(value, { colorMap, C, errorLine, debugLine, showCurrentLine, caretLine });

  // Line-number gutter content — one number per line, newline-joined.
  // Right-aligned in the gutter via CSS textAlign.
  const lineNumbers = useMemo(
    () => Array.from({ length: lineCount }, (_, i) => String(i + 1)).join('\n'),
    [lineCount]
  );

  // ── Minimap size tracking ────────────────────────────────────────
  // Observe the minimap container so the canvas redraws when the pane
  // is resized. ResizeObserver fires once on observe() with the current
  // size, so mount is covered too. The change-guard skips redundant
  // setState when the observer reports the same size (it sometimes
  // does), avoiding pointless re-renders.
  useEffect(() => {
    if (!showMinimap) return undefined;
    const cv = minimapRef.current;
    const parent = cv && cv.parentElement;
    if (!parent) return undefined;
    const ro = new ResizeObserver((entries) => {
      const cr = entries[0]?.contentRect;
      if (!cr) return;
      const w = Math.round(cr.width);
      const h = Math.round(cr.height);
      setMapSize((prev) => (prev.w === w && prev.h === h) ? prev : { w, h });
    });
    ro.observe(parent);
    return () => ro.disconnect();
  }, [showMinimap]);

  // ── Minimap rendering ────────────────────────────────────────────
  // Renders all lines onto the right-edge canvas at a scaled-down
  // density. Each line shows as a slim bar covering the column range
  // [leading-whitespace, line-end]. The current viewport is overlaid
  // as a translucent rectangle so the user knows where they are in
  // a long file. Re-renders on value / scroll / caret / theme / resize
  // (mapSize is written by the ResizeObserver above and listed in deps).
  useEffect(() => {
    if (!showMinimap) return;
    const cv = minimapRef.current;
    const ta = textareaRef.current;
    if (!cv || !ta) return;

    // Size comes from the ResizeObserver-tracked state — the single
    // source of truth for the canvas dimensions. {0,0} before the
    // observer's first fire; the guard below waits for a real size.
    const W = mapSize.w;
    const H = mapSize.h;
    if (W <= 0 || H <= 0) return;

    // Match canvas backing-store size to its CSS size so we get
    // crisp 1-device-pixel lines. Respect dpr for HiDPI displays.
    const dpr = window.devicePixelRatio || 1;
    cv.width = W * dpr;
    cv.height = H * dpr;
    cv.style.width = W + 'px';
    cv.style.height = H + 'px';
    const g = cv.getContext('2d');
    g.scale(dpr, dpr);
    g.clearRect(0, 0, W, H);

    const totalLines = Math.max(1, lineCount);
    const lineH = Math.max(1, H / totalLines);   // px per line on the minimap
    // Cap visible lines = how many editor lines fit on the canvas if
    // we render at 1 px per line. Used to find max line length for
    // scaling bar widths.
    let maxLen = 1;
    const v = value || '';
    const lineTexts = v.split('\n');
    for (const ln of lineTexts) {
      if (ln.length > maxLen) maxLen = ln.length;
    }
    g.fillStyle = C.textDim;
    for (let i = 0; i < lineTexts.length; i++) {
      const ln = lineTexts[i];
      const lead = ln.match(/^[ \t]*/)[0].length;
      const rest = ln.length - lead;
      if (rest <= 0) continue;
      const x0 = (lead / maxLen) * W;
      const x1 = (ln.length / maxLen) * W;
      const y  = i * lineH;
      g.fillRect(x0, y, Math.max(1, x1 - x0), Math.max(1, lineH - 0.5));
    }

    // Viewport overlay. textarea's contentHeight ≈ totalLines * 20px
    // (line-height set in the CSS below).
    const editorLineH = 20;
    const contentH = totalLines * editorLineH;
    if (contentH > 0) {
      const vpTop = (scrollTop / contentH) * H;
      const vpH = (ta.clientHeight / contentH) * H;
      g.fillStyle = `${C.accent}33`;
      g.fillRect(0, vpTop, W, Math.max(2, vpH));
      g.strokeStyle = `${C.accent}66`;
      g.lineWidth = 1;
      g.strokeRect(0.5, vpTop + 0.5, W - 1, Math.max(2, vpH - 1));
    }

    // Caret indicator — thin bar at the caret's line.
    const caretY = (caretLine - 1) * lineH;
    g.fillStyle = C.accent;
    g.fillRect(0, caretY, 2, Math.max(1, lineH));
  }, [showMinimap, value, lineCount, scrollTop, caretLine,
      C.textDim, C.accent, mapSize.w, mapSize.h]);

  // VS Code-style minimap interaction:
  //   • Mousedown OUTSIDE the viewport-rect → absolute jump to that
  //     line; subsequent drag follows the cursor absolutely.
  //   • Mousedown INSIDE the viewport-rect → no jump; enter grab
  //     mode, drag preserves the cursor's offset within the rect
  //     (so the rect "follows the grip", not the cursor centre).
  //
  // dragRef.mode tracks which behaviour is in flight; grabOffset is
  // the cursor's pixel distance from the rect's top edge at grab time.
  const minimapDragRef = useRef({ active: false, mode: 'absolute', grabOffset: 0 });

  // Absolute-scroll helper — centre the viewport at the cursor's
  // Y position on the minimap. Used by jump-on-click and
  // absolute-mode drag.
  const scrollEditorToMinimapY = useCallback((clientY) => {
    const cv = minimapRef.current;
    const ta = textareaRef.current;
    if (!cv || !ta) return;
    const rect = cv.getBoundingClientRect();
    const y = clientY - rect.top;
    const editorLineH = 20;
    const contentH = Math.max(1, lineCount * editorLineH);
    const target = (y / rect.height) * contentH - ta.clientHeight / 2;
    ta.scrollTop = Math.max(0, Math.min(target, contentH - ta.clientHeight));
  }, [lineCount]);

  // Grab-scroll helper — keep the cursor pinned at its original
  // offset inside the viewport-rect so the rect tracks the grip.
  const scrollEditorToMinimapGrab = useCallback((clientY, grabOffset) => {
    const cv = minimapRef.current;
    const ta = textareaRef.current;
    if (!cv || !ta) return;
    const rect = cv.getBoundingClientRect();
    const y = clientY - rect.top;
    const editorLineH = 20;
    const contentH = Math.max(1, lineCount * editorLineH);
    // Solve: (scrollTop / contentH) * H == newVpTop, where
    //   newVpTop = y - grabOffset
    const newVpTop = y - grabOffset;
    const target = (newVpTop / rect.height) * contentH;
    ta.scrollTop = Math.max(0, Math.min(target, contentH - ta.clientHeight));
  }, [lineCount]);

  const onMinimapMouseDown = useCallback((e) => {
    e.preventDefault();
    const cv = minimapRef.current;
    const ta = textareaRef.current;
    if (!cv || !ta) return;
    const rect = cv.getBoundingClientRect();
    const y = e.clientY - rect.top;
    const editorLineH = 20;
    const contentH = Math.max(1, lineCount * editorLineH);
    const vpTop = (ta.scrollTop / contentH) * rect.height;
    const vpH = (ta.clientHeight / contentH) * rect.height;
    const insideVp = y >= vpTop && y <= vpTop + vpH;
    if (insideVp) {
      // Grab — no jump, just remember the grip offset.
      minimapDragRef.current = {
        active: true, mode: 'grab', grabOffset: y - vpTop,
      };
    } else {
      // Click outside viewport → absolute jump now, drag continues
      // absolutely.
      scrollEditorToMinimapY(e.clientY);
      minimapDragRef.current = {
        active: true, mode: 'absolute', grabOffset: 0,
      };
    }
  }, [lineCount, scrollEditorToMinimapY]);

  useEffect(() => {
    function onMove(e) {
      const state = minimapDragRef.current;
      if (!state.active) return;
      if (state.mode === 'grab') {
        scrollEditorToMinimapGrab(e.clientY, state.grabOffset);
      } else {
        scrollEditorToMinimapY(e.clientY);
      }
    }
    function onUp() {
      minimapDragRef.current = { ...minimapDragRef.current, active: false };
    }
    window.addEventListener('mousemove', onMove);
    window.addEventListener('mouseup', onUp);
    return () => {
      window.removeEventListener('mousemove', onMove);
      window.removeEventListener('mouseup', onUp);
    };
  }, [scrollEditorToMinimapY, scrollEditorToMinimapGrab]);

  // VS Code-style indent guides — per-line segments, NOT a global
  // gradient. computeIndentGuides() returns one entry per contiguous
  // vertical run at a given indent level. Each renders as a single
  // 1-px-wide div inside the highlight pre so it scrolls with text.
  // Blank lines inside an indented block inherit the surrounding
  // level; blank lines between unrelated blocks don't drag guides.
  const indentSegments = useMemo(
    () => (showIndentGuides ? computeIndentGuides(value || '') : []),
    [showIndentGuides, value]
  );

  return (
    <div style={{ position:'relative', width:'100%', height:'100%',
                  display:'flex', overflow:'hidden', background: C.bg1 }}>
      {/* Line-number gutter — see EditorGutter (scroll-synced via gutterRef). */}
      <EditorGutter show={showGutter} gutterRef={gutterRef} lineNumbers={lineNumbers} C={C} />

      {/* Editing area — the original two-layer stack (highlight pre +
          transparent textarea), now with optional indent-guide
          background on the highlight layer. */}
      <div
        ref={editorAreaRef}
        onClick={(e) => {
          if (e.target !== textareaRef.current && !searchMode) {
            textareaRef.current?.focus();
          }
        }}
        style={{ position:'relative', flex:1, overflow:'hidden', cursor: 'text' }}
      >
        <pre ref={highlightRef} aria-hidden="true" style={{
          position:'absolute', top:0, left:0, right:0, bottom:0,
          margin:0, padding:8,
          fontFamily:FONT, fontSize:13, lineHeight:'20px',
          color: C.text, background:'transparent',
          // overflow:hidden (not auto) — this layer scrolls ONLY
          // programmatically via syncScroll (scrollTop/scrollLeft are
          // set directly, which still works under overflow:hidden).
          // Using `auto` here drew a SECOND scrollbar on top of the
          // textarea's. The textarea (zIndex 3) owns the visible
          // scrollbar + all user scroll interaction.
          border:'none', overflow:'hidden', whiteSpace:'pre',
          pointerEvents:'none', zIndex:2,
        }}>
          {/* Syntax-highlighted content (replaces children). */}
          <span dangerouslySetInnerHTML={{__html:html}} />

          {/* Indent guides — VS Code-style per-line segments. One div
              per contiguous vertical run at a single indent level.
              `left` uses ch unit so the position scales with the
              monospace font width. Sits BELOW syntax-highlight text
              (rendered earlier in the same z-stack) but the colour
              is subtle so overlap is fine. */}
          {indentSegments.map((seg, i) => (
            <span key={`ig-${i}`} style={{
              position: 'absolute',
              left: `calc(8px + ${(seg.level - 1) * 4}ch)`,
              top: seg.startLine * 20 + 8,
              width: 1,
              height: (seg.endLine - seg.startLine + 1) * 20,
              background: `${C.border}77`,
              pointerEvents: 'none',
            }} />
          ))}

          {/* Bracket-match overlay — two faint boxes around the
              paired brackets. Red when one half is unmatched. Uses
              `ch` so column math matches the monospace font width. */}
          {bracketMatch && (() => {
            const v = value || '';
            const cells = [];
            const matched = bracketMatch.open !== -1 && bracketMatch.close !== -1;
            const bg = matched ? `${C.accent}33` : `${C.red}33`;
            const bd = matched ? `${C.accent}` : `${C.red}`;
            for (const off of [bracketMatch.open, bracketMatch.close]) {
              if (off === -1) continue;
              const { line, col } = offsetToLineCol(v, off);
              cells.push(
                <span key={`bm-${off}`} style={{
                  position:'absolute',
                  left: `calc(8px + ${col}ch)`,
                  top: line * 20 + 8,
                  width: '1ch', height: 20,
                  background: bg,
                  border: `1px solid ${bd}`,
                  boxSizing: 'border-box',
                  pointerEvents: 'none',
                }} />
              );
            }
            return cells;
          })()}

          {/* Find overlay — a box on EVERY match while the find/replace
              bar is open (the active match, under the caret, gets a
              stronger tint). Distinct colour (warn) from the accent-tinted
              selection-match overlay below. */}
          {searchMatches.map((m, i) => {
            const { line, col } = offsetToLineCol(value || '', m.start);
            const len = m.end - m.start;
            const active = i === matchInfo.activeIdx;
            return (
              <span key={`fm-${i}`} style={{
                position: 'absolute',
                left: `calc(8px + ${col}ch)`,
                top: line * 20 + 8,
                width: `${len}ch`,
                height: 20,
                background: active ? `${C.orange}55` : `${C.orange}22`,
                border: `1px solid ${C.orange}${active ? 'aa' : '55'}`,
                boxSizing: 'border-box',
                pointerEvents: 'none',
              }} />
            );
          })}

          {/* Selection-match overlay — every other whole-word
              occurrence of the currently-selected identifier. */}
          {wordOccurrences.length > 0 && wordOccurrences.map((m, i) => {
            const { line, col } = offsetToLineCol(value || '', m.start);
            const len = m.end - m.start;
            return (
              <span key={`wo-${i}`} style={{
                position:'absolute',
                left: `calc(8px + ${col}ch)`,
                top: line * 20 + 8,
                width: `${len}ch`,
                height: 20,
                background: `${C.accent}22`,
                border: `1px solid ${C.accent}55`,
                boxSizing: 'border-box',
                pointerEvents: 'none',
              }} />
            );
          })}
        </pre>
        {/* Native textareas already support drag-drop of their own
            selection (move; Ctrl+drag = copy). The browser mutates
            value and fires `input`, which React routes to onChange →
            our onChangeInternal classifies the multi-char diff as
            kind='edit' and pushes one history entry. applyDragDrop
            in editorOps is exported for programmatic callers; the
            textarea itself doesn't need a custom drag handler. */}
        <textarea ref={textareaRef} value={value} onChange={onChangeInternal} onScroll={syncScroll}
          onKeyDown={onKeyDown}
          onSelect={reportCursor} onKeyUp={reportCursor} onClick={reportCursor}
          spellCheck={false} wrap="off"
          // nk-editor-textarea hides the native scrollbar (CSS in
          // numkit-ide.css). The minimap is the scroll affordance now —
          // wheel + minimap drag both scroll; a native scrollbar
          // sitting right next to the minimap read as a duplicate.
          className="nk-editor-textarea"
          style={{position:'relative',width:'100%',height:'100%',margin:0,padding:8,fontFamily:FONT,fontSize:13,lineHeight:'20px',color:'transparent',caretColor:C?.accent || 'var(--accent, #7c6ff0)',cursor:'text',background:'transparent',border:'none',outline:'none',resize:'none',overflow:'auto',whiteSpace:'pre',zIndex:3}}/>

        {/* Autocomplete popup — see CompletionPopup. */}
        <CompletionPopup items={acItems} anchor={acAnchor} activeIdx={acIdx}
                         onAccept={acceptCompletion} onHover={setAcIdx} C={C} />

        {/* Find / Replace / Go-to-line bar — overlays the top-right
            corner of the editor area, à la VS Code. Same input gets
            re-used across modes; render shape changes based on mode. */}
        {searchMode && (
          <div style={{
            position:'absolute', top:6, right:14,
            zIndex:10, display:'flex', flexDirection:'column', gap:4,
            background: C.bg2, border:`1px solid ${C.border}`, borderRadius:4,
            padding:'6px 8px', fontFamily: FONT_UI, fontSize:12,
            boxShadow: `0 2px 10px ${C.bg0}aa`,
          }}>
            {searchMode === 'goto' ? (
              <div style={{ display:'flex', alignItems:'center', gap:6 }}>
                <span style={{ color: C.textDim }}>line:</span>
                <input
                  ref={searchInputRef}
                  type="text"
                  value={gotoValue}
                  onChange={(e) => setGotoValue(e.target.value)}
                  onKeyDown={(e) => {
                    if (e.key === 'Enter') { e.preventDefault(); performGoto(); }
                    else if (e.key === 'Escape') { e.preventDefault(); setSearchMode(''); textareaRef.current?.focus(); }
                  }}
                  style={{ width:80, padding:'2px 4px', fontFamily:FONT, fontSize:12,
                           background: C.bg1, color: C.text,
                           border:`1px solid ${C.border}`, borderRadius:2, outline:'none' }} />
                <button onClick={performGoto} style={searchBtnStyle(C)}>go</button>
                <button onClick={() => { setSearchMode(''); textareaRef.current?.focus(); }} style={searchBtnStyle(C)}>×</button>
              </div>
            ) : (
              <>
                {/* Find row */}
                <div style={{ display:'flex', alignItems:'center', gap:6 }}>
                  <input
                    ref={searchInputRef}
                    type="text"
                    placeholder="find"
                    value={searchQuery}
                    onChange={(e) => setSearchQuery(e.target.value)}
                    onKeyDown={(e) => {
                      if (e.key === 'Enter') { e.preventDefault(); (e.shiftKey ? findPrev : findNext)(); }
                      else if (e.key === 'Escape') { e.preventDefault(); setSearchMode(''); textareaRef.current?.focus(); }
                    }}
                    style={{ width:180, padding:'2px 4px', fontFamily:FONT, fontSize:12,
                             background: C.bg1, color: C.text,
                             border:`1px solid ${C.border}`, borderRadius:2, outline:'none' }} />
                  <span style={{ color: C.textMuted, minWidth:60, fontSize:11 }}>
                    {searchQuery && matchInfo.count > 0
                      ? `${matchInfo.activeIdx >= 0 ? matchInfo.activeIdx + 1 : '–'} / ${matchInfo.count}`
                      : (searchQuery ? '0 / 0' : '')}
                  </span>
                  <button title="case sensitive (Aa)"
                          onClick={() => setSearchOpts((o) => ({ ...o, caseSensitive: !o.caseSensitive }))}
                          style={searchToggleStyle(C, searchOpts.caseSensitive)}>Aa</button>
                  <button title="whole word"
                          onClick={() => setSearchOpts((o) => ({ ...o, wholeWord: !o.wholeWord }))}
                          style={searchToggleStyle(C, searchOpts.wholeWord)}>W</button>
                  <button title="regex"
                          onClick={() => setSearchOpts((o) => ({ ...o, regex: !o.regex }))}
                          style={searchToggleStyle(C, searchOpts.regex)}>.*</button>
                  <button title="prev (Shift+F3)" onClick={findPrev} style={searchBtnStyle(C)}>↑</button>
                  <button title="next (F3)"      onClick={findNext} style={searchBtnStyle(C)}>↓</button>
                  <button title="close (Esc)"    onClick={() => { setSearchMode(''); textareaRef.current?.focus(); }} style={searchBtnStyle(C)}>×</button>
                </div>
                {/* Replace row */}
                {searchMode === 'replace' && (
                  <div style={{ display:'flex', alignItems:'center', gap:6 }}>
                    <input
                      type="text"
                      placeholder="replace"
                      value={searchReplace}
                      onChange={(e) => setSearchReplace(e.target.value)}
                      onKeyDown={(e) => {
                        if (e.key === 'Enter') { e.preventDefault(); replaceCurrent(); }
                        else if (e.key === 'Escape') { e.preventDefault(); setSearchMode(''); textareaRef.current?.focus(); }
                      }}
                      style={{ width:180, padding:'2px 4px', fontFamily:FONT, fontSize:12,
                               background: C.bg1, color: C.text,
                               border:`1px solid ${C.border}`, borderRadius:2, outline:'none' }} />
                    <span style={{ minWidth:60 }} />
                    <button onClick={replaceCurrent} style={searchBtnStyle(C)} title="replace">↻</button>
                    <button onClick={replaceAll}    style={searchBtnStyle(C)} title="replace all">↻↻</button>
                  </div>
                )}
              </>
            )}
          </div>
        )}
      </div>

      {/* Minimap — see EditorMinimap. The canvas is painted by the effect
          above; click-to-jump is owned by onMinimapMouseDown. */}
      <EditorMinimap show={showMinimap} minimapRef={minimapRef}
                     onMouseDown={onMinimapMouseDown} C={C} />
    </div>
  );
});

export default SyntaxEditor;
