/**
 * editorHistory.js — immutable history reducer for the editor's
 * Ctrl+Z / Ctrl+Y stack.
 *
 * Native textarea undo records every keystroke as a separate step,
 * which is unusable for a code editor. This module groups
 * consecutive typing into one undo step (debounce-based + change-
 * kind-aware), and exposes Word/Selection-aware undo.
 *
 * Snapshot shape: { text, selStart, selEnd }
 *
 * Public API:
 *   createHistory(initial)              → History
 *   pushSnapshot(history, snap, kind, t)→ History  (immutable)
 *   undo(history)                       → { history, snapshot } | null
 *   redo(history)                       → { history, snapshot } | null
 *   canUndo(history) / canRedo(history) → bool
 *
 * `kind` is a string tagging the edit type. Two consecutive edits
 * with the same continuable kind (`type` / `delete`) within
 * HISTORY_GROUP_MS get coalesced into one undo step; everything
 * else creates a fresh step.
 */

/** Max ms between two same-kind edits for them to merge into one
 *  undo group. 500 ms is the VS Code default and feels natural —
 *  long enough to capture a word burst, short enough that a pause
 *  starts a new group. */
export const HISTORY_GROUP_MS = 500;

/** Hard cap on history depth. Keeps memory bounded on long
 *  editing sessions. Oldest entries get dropped first (FIFO). */
export const MAX_HISTORY = 500;

/** Kinds whose consecutive same-kind edits merge into one undo step
 *  (when within the time window). Everything else is always its
 *  own step — operations like Tab / Enter / paste / auto-close are
 *  semantically distinct events. */
const CONTINUABLE_KINDS = new Set(['type', 'delete']);

/** Construct an empty history seeded with `initial` as the first
 *  (zero-index) snapshot. The current state is at index 0 — undo
 *  immediately is a no-op. */
export function createHistory(initial) {
  return {
    entries: [initial],
    index: 0,
    lastKind: null,
    lastTime: 0,
  };
}

/** Push a new snapshot, possibly coalescing with the previous entry
 *  if both are same-kind continuable edits within HISTORY_GROUP_MS.
 *
 *  When NOT coalescing: drops anything past the current index (the
 *  redo branch is forfeited the moment the user types after undo —
 *  same semantics as every editor).
 *
 *  Returns a NEW history object; the input is not mutated. */
export function pushSnapshot(history, snapshot, kind, time) {
  // No-op when text is identical to the current snapshot (caret-only
  // changes don't deserve a history entry).
  const cur = history.entries[history.index];
  if (cur && cur.text === snapshot.text) return history;

  const sameGroup = kind === history.lastKind
                 && CONTINUABLE_KINDS.has(kind)
                 && (time - history.lastTime) < HISTORY_GROUP_MS;

  let entries;
  if (sameGroup) {
    // Replace the current entry with the new one — the burst-start
    // entry (history[index-1]) is what we'll undo to.
    entries = history.entries.slice(0, history.index);
    entries.push(snapshot);
  } else {
    // Push as a new entry; truncate the redo tail.
    entries = history.entries.slice(0, history.index + 1);
    entries.push(snapshot);
  }
  // Cap depth from the OLD side so the most-recent activity is
  // always preserved.
  let index = entries.length - 1;
  while (entries.length > MAX_HISTORY) {
    entries.shift();
    index -= 1;
  }
  return { entries, index, lastKind: kind, lastTime: time };
}

/** Move one step back. Returns null when already at index 0. */
export function undo(history) {
  if (history.index === 0) return null;
  const nextIndex = history.index - 1;
  return {
    history: {
      ...history,
      index: nextIndex,
      // Reset group state so the next edit starts a fresh group.
      lastKind: null,
      lastTime: 0,
    },
    snapshot: history.entries[nextIndex],
  };
}

/** Move one step forward. Returns null when no forward entry exists. */
export function redo(history) {
  if (history.index >= history.entries.length - 1) return null;
  const nextIndex = history.index + 1;
  return {
    history: {
      ...history,
      index: nextIndex,
      lastKind: null,
      lastTime: 0,
    },
    snapshot: history.entries[nextIndex],
  };
}

export function canUndo(history) {
  return history.index > 0;
}

export function canRedo(history) {
  return history.index < history.entries.length - 1;
}

/** Classify a single onChange diff into one of:
 *    'type'   — single-char insertion at caret
 *    'delete' — single-char deletion
 *    'edit'   — anything else (paste, cut, multi-char replace)
 *
 *  Used to drive grouping. Cheap heuristic; doesn't have to be perfect
 *  because misclassification only affects how aggressively edits
 *  coalesce, never correctness. */
export function classifyChange(oldText, newText) {
  if (newText.length === oldText.length + 1) return 'type';
  if (newText.length === oldText.length - 1) return 'delete';
  return 'edit';
}
