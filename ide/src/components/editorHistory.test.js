// editorHistory.test.js — vitest unit coverage for the editor's
// undo/redo reducer. Pure functions over { text, selStart, selEnd }
// snapshots; the SyntaxEditor wires them to onKeyDown.

import { describe, it, expect } from 'vitest';
import {
  HISTORY_GROUP_MS, MAX_HISTORY,
  createHistory, pushSnapshot, undo, redo,
  canUndo, canRedo, classifyChange,
} from './editorHistory';

const snap = (text, sel = 0) => ({ text, selStart: sel, selEnd: sel });

// ── createHistory ──────────────────────────────────────────────────

describe('createHistory', () => {
  it('seeds with one entry at index 0', () => {
    const h = createHistory(snap('hello'));
    expect(h.entries.length).toBe(1);
    expect(h.index).toBe(0);
    expect(canUndo(h)).toBe(false);
    expect(canRedo(h)).toBe(false);
  });
});

// ── pushSnapshot — basic push ──────────────────────────────────────

describe('pushSnapshot', () => {
  it('pushes a new entry and advances index', () => {
    let h = createHistory(snap(''));
    h = pushSnapshot(h, snap('a'), 'type', 0);
    expect(h.entries.length).toBe(2);
    expect(h.index).toBe(1);
    expect(canUndo(h)).toBe(true);
  });

  it('is a no-op when text matches the current snapshot', () => {
    let h = createHistory(snap('hello'));
    h = pushSnapshot(h, snap('hello'), 'type', 0);   // same text
    expect(h.entries.length).toBe(1);
  });

  it('truncates the redo tail when pushing after an undo', () => {
    let h = createHistory(snap(''));
    h = pushSnapshot(h, snap('a'), 'type', 0);
    h = pushSnapshot(h, snap('ab'), 'type', HISTORY_GROUP_MS + 100);   // different group
    expect(h.entries.length).toBe(3);
    // Undo back to 'a', then push 'c' — 'ab' should be discarded.
    const u = undo(h);
    expect(u.snapshot.text).toBe('a');
    h = u.history;
    h = pushSnapshot(h, snap('ac'), 'type', HISTORY_GROUP_MS * 4);
    expect(h.entries.map((e) => e.text)).toEqual(['', 'a', 'ac']);
  });
});

// ── coalescing ─────────────────────────────────────────────────────

describe('pushSnapshot: grouping', () => {
  it('coalesces consecutive `type` edits within HISTORY_GROUP_MS', () => {
    let h = createHistory(snap(''));
    h = pushSnapshot(h, snap('h'), 'type', 0);
    h = pushSnapshot(h, snap('he'), 'type', 100);
    h = pushSnapshot(h, snap('hel'), 'type', 200);
    h = pushSnapshot(h, snap('hello'), 'type', 400);
    // All four typing strokes form ONE group: history = ['', 'hello'].
    expect(h.entries.length).toBe(2);
    expect(h.entries[1].text).toBe('hello');
  });

  it('starts a new group after a > HISTORY_GROUP_MS pause', () => {
    let h = createHistory(snap(''));
    h = pushSnapshot(h, snap('hello'), 'type', 0);
    h = pushSnapshot(h, snap('hello world'), 'type', HISTORY_GROUP_MS + 1);
    expect(h.entries.length).toBe(3);
  });

  it('does NOT coalesce across different kinds', () => {
    let h = createHistory(snap(''));
    h = pushSnapshot(h, snap('hello'), 'type',   0);
    h = pushSnapshot(h, snap('hello\n'), 'newline', 50);
    h = pushSnapshot(h, snap('hello\nw'), 'type', 100);
    // type → newline → type = three groups.
    expect(h.entries.length).toBe(4);
  });

  it('does NOT coalesce non-continuable kinds (tab, paste, …) even within window', () => {
    let h = createHistory(snap(''));
    h = pushSnapshot(h, snap('    '), 'tab', 0);
    h = pushSnapshot(h, snap('        '), 'tab', 50);
    expect(h.entries.length).toBe(3);
  });

  it('coalesces consecutive deletes the same way', () => {
    let h = createHistory(snap('abc'));
    h = pushSnapshot(h, snap('ab'), 'delete', 0);
    h = pushSnapshot(h, snap('a'), 'delete', 100);
    h = pushSnapshot(h, snap(''), 'delete', 200);
    expect(h.entries.length).toBe(2);
    expect(h.entries[1].text).toBe('');
  });
});

// ── undo / redo ────────────────────────────────────────────────────

describe('undo / redo', () => {
  it('moves the cursor back/forward through entries', () => {
    let h = createHistory(snap(''));
    h = pushSnapshot(h, snap('a'),  'newline', 0);
    h = pushSnapshot(h, snap('ab'), 'newline', 100);
    const u1 = undo(h);
    expect(u1.snapshot.text).toBe('a');
    const u2 = undo(u1.history);
    expect(u2.snapshot.text).toBe('');
    expect(undo(u2.history)).toBeNull();    // already at start
    const r1 = redo(u2.history);
    expect(r1.snapshot.text).toBe('a');
  });

  it('redo is null when at the front', () => {
    let h = createHistory(snap('a'));
    h = pushSnapshot(h, snap('ab'), 'newline', 0);
    expect(redo(h)).toBeNull();
  });

  it('resets the group lock after undo (next edit starts a new group)', () => {
    let h = createHistory(snap(''));
    h = pushSnapshot(h, snap('a'), 'type', 0);
    h = pushSnapshot(h, snap('ab'), 'type', 100);   // same group → coalesces
    expect(h.entries.length).toBe(2);
    const u = undo(h);
    h = u.history;
    // First push after undo MUST be a new entry (not merged with the
    // pre-undo 'ab' burst), even if the time delta is small. Later
    // pushes in the same fresh burst merge normally.
    h = pushSnapshot(h, snap('ax'), 'type', 150);  // 50 ms after the now-undone 'ab'
    expect(h.entries.map((e) => e.text)).toEqual(['', 'ax']);
    expect(canRedo(h)).toBe(false);   // redo branch was discarded
  });
});

// ── canUndo / canRedo ──────────────────────────────────────────────

describe('canUndo / canRedo', () => {
  it('canUndo is false at index 0', () => {
    expect(canUndo(createHistory(snap('')))).toBe(false);
  });
  it('canRedo is false at end', () => {
    let h = createHistory(snap(''));
    h = pushSnapshot(h, snap('a'), 'type', 0);
    expect(canRedo(h)).toBe(false);
  });
  it('canRedo is true after an undo', () => {
    let h = createHistory(snap(''));
    h = pushSnapshot(h, snap('a'), 'type', 0);
    const u = undo(h);
    expect(canRedo(u.history)).toBe(true);
  });
});

// ── MAX_HISTORY cap ────────────────────────────────────────────────

describe('MAX_HISTORY cap', () => {
  it('drops the oldest entries when over the cap', () => {
    let h = createHistory(snap('start'));
    // Push MAX_HISTORY + 100 distinct entries with non-coalescing kind.
    for (let i = 1; i <= MAX_HISTORY + 100; i++) {
      h = pushSnapshot(h, snap(String(i)), 'newline', i);
    }
    expect(h.entries.length).toBe(MAX_HISTORY);
    expect(h.entries[h.entries.length - 1].text).toBe(String(MAX_HISTORY + 100));
    // 'start' is long gone.
    expect(h.entries[0].text).not.toBe('start');
  });
});

// ── classifyChange ─────────────────────────────────────────────────

describe('classifyChange', () => {
  it('detects single-char insert as `type`', () => {
    expect(classifyChange('foo', 'foox')).toBe('type');
  });
  it('detects single-char delete as `delete`', () => {
    expect(classifyChange('foox', 'foo')).toBe('delete');
  });
  it('multi-char diff is `edit`', () => {
    expect(classifyChange('foo', 'foo bar')).toBe('edit');
    expect(classifyChange('foo', 'baz')).toBe('edit');
  });
});
