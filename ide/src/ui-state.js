/**
 * ui-state.js — localStorage persistence for the IDE's UI state.
 *
 * Two flavors:
 *
 *  1) `loadUiState` / `saveUiState` — the big session blob
 *     (tabs, active tab, panel layout, breakpoints). Debounced so
 *     chatty churn (typing, resize) collapses to one flush. One
 *     JSON value under one key, versioned — on version mismatch or
 *     corruption we return null and callers fall through to
 *     defaults.
 *
 *  2) `usePersistedState(key, default, { validate? })` — a small
 *     React hook for any standalone piece of UI state (dropdown
 *     selection, expanded-folder map) that just wants to mirror
 *     itself into localStorage. Writes on every change (no debounce
 *     — these are small, rare mutations).
 *
 * The blob shape:
 *
 *   {
 *     version: 1,
 *     layout: { showLeft, showCenter, showRight, showBottom,
 *               figuresWidth, bottomHeight },
 *     // Breakpoints live on each tab so any tab type (untitled,
 *     // Examples, GitHub, FS-backed) keeps its red dots across
 *     // reloads — the tab is the single source of truth.
 *     tabs:   [{ id, name, code, modified, vfsPath, source, breakpoints: [lineNumbers] }],
 *     activeTab,
 *   }
 */

import { useState, useEffect } from 'react';

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

/**
 * Small useState-alike that mirrors a JSON-serializable value into
 * localStorage under `key`. `validate(parsed)` lets the caller reject
 * stale or out-of-whitelist values; failed validation falls back to
 * `defaultValue`. Storage errors (quota, disabled storage) are
 * swallowed — we'd rather skip persistence than crash the UI.
 *
 * Not debounced on purpose: intended for sparse user interactions
 * (dropdown pick, folder expand), not for continuous streams. If
 * you need to persist a high-churn value, use `saveUiState`.
 */
export function usePersistedState(key, defaultValue, { validate } = {}) {
    const [value, setValue] = useState(() => {
        try {
            const raw = localStorage.getItem(key);
            if (raw == null) return defaultValue;
            const parsed = JSON.parse(raw);
            if (validate && !validate(parsed)) return defaultValue;
            return parsed;
        } catch (_) {
            return defaultValue;
        }
    });

    useEffect(() => {
        try { localStorage.setItem(key, JSON.stringify(value)); } catch (_) {}
    }, [key, value]);

    return [value, setValue];
}
