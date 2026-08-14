/**
 * settings.js — IDE-wide settings store.
 *
 * Stores the paths to the external tools (numkit interpreter and
 * numkit_codegen code-generator) that the IDE can invoke via Electron
 * IPC. Values are persisted in localStorage under a single JSON key.
 *
 * Intentionally separate from ui-state.js so settings survive a
 * "Reset layout" (which calls clearUiState) and are easy to access
 * from any module without pulling in React hooks.
 *
 * Usage:
 *   import { loadSettings, saveSettings, DEFAULT_SETTINGS } from './settings';
 *   const s = loadSettings();
 *   saveSettings({ ...s, codegenPath: '/usr/local/bin/numkit_codegen' });
 */

const SETTINGS_KEY = 'numkit.ide.settings';
const SETTINGS_VERSION = 1;

/**
 * Default values.  An empty string means "look up the executable in
 * PATH at spawn time"; the main process (main.js) treats '' as the
 * bare executable name without a directory prefix.
 */
export const DEFAULT_SETTINGS = Object.freeze({
  interpreterPath: '', // path to numkit_repl / numkit_repl.exe
  codegenPath:     '', // path to numkit_codegen / numkit_codegen.exe
  cxxPath:         '', // NUMKIT_CXX override; '' = leave env var unchanged
  plotAspectRatio: '16:9',
  matlabCompatibility: true, // implicit import compat.*
});

/**
 * Read persisted settings from localStorage.  Returns DEFAULT_SETTINGS
 * on missing key, parse error, or version mismatch so callers never
 * have to null-check.
 */
export function loadSettings() {
  try {
    const raw = localStorage.getItem(SETTINGS_KEY);
    if (!raw) return { ...DEFAULT_SETTINGS };
    const parsed = JSON.parse(raw);
    if (!parsed || parsed.version !== SETTINGS_VERSION) return { ...DEFAULT_SETTINGS };
    // Merge with defaults so new fields added in later versions get
    // their defaults instead of being silently undefined.
    return { ...DEFAULT_SETTINGS, ...parsed };
  } catch (_) {
    return { ...DEFAULT_SETTINGS };
  }
}

/**
 * Persist settings to localStorage.  The `version` field is injected
 * automatically; do not pass it in `settings`.
 */
export function saveSettings(settings) {
  try {
    const nextSettings = { ...DEFAULT_SETTINGS, ...settings, version: SETTINGS_VERSION };
    localStorage.setItem(
      SETTINGS_KEY,
      JSON.stringify(nextSettings),
    );
    if (typeof window !== 'undefined') {
      window.dispatchEvent(new CustomEvent('numkitSettingsChanged', { detail: nextSettings }));
    }
  } catch (_) {
    // Quota exceeded or storage disabled — silent; we'd rather skip
    // persistence than crash the UI.
  }
}
