/**
 * ui-state.js — debounced localStorage persistence for the IDE's
 * UI state.
 *
 * One key, one JSON blob:
 *
 *   {
 *     version: 1,
 *     layout: { showLeft, showCenter, showRight, showBottom,
 *               figuresWidth, bottomHeight },
 *     tabs:   [{ id, name, code, modified, vfsPath, source }],
 *     activeTab,
 *     // Keyed by vfsPath so breakpoints survive close/reopen and
 *     // reloads. Untitled tabs (no vfsPath) don't persist their
 *     // breakpoints — acceptable for scratch work.
 *     breakpointsByPath: { [vfsPath]: [lineNumbers] },
 *   }
 *
 * Writes are debounced (~250 ms) so chatty state churn (typing,
 * panel resize) collapses to a single flush.
 *
 * On version mismatch or corruption we return null and callers
 * fall through to default state — old blobs don't crash new code.
 */

const KEY = 'mlab.uiState';
const VERSION = 1;
const SAVE_DEBOUNCE_MS = 250;

/** Read persisted state. Returns null if missing, corrupt, or stale. */
export function loadUiState() {
    try {
        const raw = localStorage.getItem(KEY);
        if (!raw) return null;
        const parsed = JSON.parse(raw);
        if (!parsed || parsed.version !== VERSION) return null;
        return parsed;
    } catch (_) {
        return null;
    }
}

let saveTimer = null;
let pendingState = null;

/**
 * Schedule a persist. Subsequent calls within the debounce window
 * overwrite the pending state, so only the latest snapshot is
 * flushed. Flush is synchronous inside a microtask, but writing to
 * localStorage is the only expensive part and it still happens in
 * the main thread — kept small.
 */
export function saveUiState(state) {
    pendingState = state;
    if (saveTimer) return;
    saveTimer = setTimeout(() => {
        const s = pendingState;
        saveTimer = null;
        pendingState = null;
        try {
            localStorage.setItem(
                KEY,
                JSON.stringify({ ...s, version: VERSION })
            );
        } catch (_) {
            // Quota exceeded or storage disabled — silent, we'd rather
            // lose state than crash the UI.
        }
    }, SAVE_DEBOUNCE_MS);
}

/** Wipe persisted state — handy for tests / "reset layout". */
export function clearUiState() {
    try { localStorage.removeItem(KEY); } catch (_) {}
    if (saveTimer) { clearTimeout(saveTimer); saveTimer = null; }
    pendingState = null;
}
