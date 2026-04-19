/**
 * fs/run-origin.js — pure helpers for deciding which VirtualFS a Run
 * should push onto the engine's script-origin stack.
 *
 * Separated from MLabREPL so the decision logic can be unit-tested
 * without mounting the component.
 */

// Tab `source` → preferred VFS name the engine knows about.
// Anything that isn't explicitly a real-disk mount (Local Folder) routes
// through Temporary by default — that covers REPL input, bundled examples,
// GitHub imports, and fresh unsaved tabs.
export function originForSource(source) {
  return source === 'localFolder' ? 'local' : 'temporary';
}

/**
 * Decide which adapter to invoke for a given tab. Policy:
 *   1. If the preferred FS is registered, use it.
 *   2. Otherwise, if Temporary is registered, use it — but flag that we
 *      fell back so the caller can surface a one-time warning. Keeps
 *      writes visible in File Browser instead of vanishing into MEMFS.
 *   3. Otherwise, return null/null — the caller should skip push so
 *      the engine's resolver falls through to NativeFS (MEMFS on WASM,
 *      real disk on native CLI/tests).
 *
 * @param {string | null | undefined} tabSource   The active tab's `.source`.
 * @param {{ temp?: object, local?: object } | null | undefined} adapters
 *        VFS adapters from App state.
 * @returns {{ adapter: object|null, origin: string|null, fallbackUsed: boolean }}
 */
export function pickRunOrigin(tabSource, adapters) {
  const wantOrigin = originForSource(tabSource);
  const wanted = wantOrigin === 'local' ? adapters?.local : adapters?.temp;
  if (wanted) return { adapter: wanted, origin: wantOrigin, fallbackUsed: false };
  if (adapters?.temp)
    return { adapter: adapters.temp, origin: 'temporary', fallbackUsed: wantOrigin === 'local' };
  return { adapter: null, origin: null, fallbackUsed: false };
}
