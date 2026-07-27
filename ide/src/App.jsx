import { useState, useEffect, useCallback } from 'react';
import IDE from './components/shell/IDE';
import ErrorBoundary from './components/ui/ErrorBoundary';
import { createWasmEngine, createNativeEngine, createFallbackEngine } from './engine';
import { loadSettings } from './settings';
import tempFS from './temporary';
import { installVfsAdapters, installLocalAdapter } from './fs/vfs-adapter';
import './styles/numkit-ide.css';

// ── Global async-error tap ─────────────────────────────────────────
// React's ErrorBoundary catches sync render errors, not unhandled
// promise rejections or window.onerror events. In Electron those
// previously went silently to the devtools console while the user saw
// a frozen UI; route them through the same console + a one-line
// in-app banner so we get diagnostic data when scripts misbehave.
if (typeof window !== 'undefined' && !window.__numkitGlobalErrorsBound) {
  window.__numkitGlobalErrorsBound = true;
  window.addEventListener('error', (e) => {
    // eslint-disable-next-line no-console
    console.error('[IDE window.error]', e.message, e.filename, e.lineno, e.error);
  });
  window.addEventListener('unhandledrejection', (e) => {
    // eslint-disable-next-line no-console
    console.error('[IDE unhandledrejection]', e.reason);
  });
}

/**
 * App — initialises the numkit engine.
 *
 * Two distinct paths, no overlap:
 *   Electron  →  createNativeEngine(nativeFS)   — instant, no WASM / tempFS
 *   Browser   →  createWasmEngine() + tempFS    — full browser stack
 */
export default function App() {
  const [engine, setEngine] = useState(null);
  const [status, setStatus] = useState('loading');
  const [initMessage, setInitMessage] = useState('');
  const [fsReady, setFsReady] = useState(false);
  const [vfsAdapters, setVfsAdapters] = useState(null);

  useEffect(() => {
    let cancelled = false;

    async function init() {
      const isElectron = typeof window.nativeFS !== 'undefined';

      if (isElectron) {
        // ─────────────────────────────────────────────────────────────
        // Electron: native REPL binary handles everything.
        // No WASM, no IndexedDB tempFS, no VFS adapters needed.
        // ─────────────────────────────────────────────────────────────
        console.log('[App] Electron mode — using native engine');

        // Push persisted settings (localStorage) to main.js so
        // resolveExe() finds the user-configured interpreter path
        // from the very first REPL spawn.
        const nfs = window.nativeFS;
        if (typeof nfs.updateSettings === 'function') {
          const saved = loadSettings();
          nfs.updateSettings(saved);
        }

        const eng = createNativeEngine(nfs);
        const greeting = eng.init();
        if (cancelled) return;
        setEngine(eng);
        setFsReady(true);
        setStatus('ready');
        setInitMessage(greeting);
      } else {
        // ─────────────────────────────────────────────────────────────
        // Browser: WASM engine + IndexedDB tempFS + VFS adapters.
        // ─────────────────────────────────────────────────────────────

        // 1. Init Temporary FS (IndexedDB-backed scratch storage)
        try {
          setInitMessage('Initialising file system...');
          await tempFS.init();
          if (!cancelled) setFsReady(true);
        } catch (e) {
          console.error('[TemporaryFS] Init failed:', e);
          if (!cancelled) setFsReady(true); // continue anyway
        }

        // 2. Init Engine
        try {
          const hasWasm = window.__WASM_GLUE_LOADED__ === true
                       && typeof window.createNumkitIdeModule === 'function';
          if (!hasWasm) throw new Error('WASM glue not loaded');

          setInitMessage('Loading WebAssembly...');
          const eng = await createWasmEngine(window.createNumkitIdeModule);
          if (cancelled) return;

          // IMPORTANT: call init() BEFORE registering VFS adapters.
          // repl_init() constructs a fresh ReplSession on the C++ side; if
          // registration happens first, init() replaces the session and
          // silently drops every registered VirtualFS, producing cryptic
          // "filesystem 'temporary' is not available" errors later.
          const greeting = eng.init();

          // Mirror tempFS (and local, if mounted) into sync callbacks the
          // engine can invoke from csvread/csvwrite.
          try {
            const adapters = await installVfsAdapters(eng);
            if (!cancelled) setVfsAdapters(adapters);
          } catch (e) {
            console.error('[VFS] adapter install failed:', e);
          }

          setEngine(eng);
          setStatus('ready');
          setInitMessage(greeting);
        } catch (err) {
          if (cancelled) return;
          console.log('[REPL] Using fallback engine:', err.message);
          const eng = createFallbackEngine();
          setEngine(eng);
          setStatus('fallback');
          setInitMessage('Running in demo mode (no WASM binary detected).');
        }
      }
    }

    init();
    return () => { cancelled = true; };
  }, []);

  // Registration helper handed to FileBrowser so the 'local' adapter can
  // be installed after the user (re)mounts a folder — that happens after
  // page load, well after the initial installVfsAdapters() call. Memoised
  // by `engine` so prop identity is stable across rerenders; otherwise
  // FileBrowser's useEffect (which depends on this callback) would fire
  // in a loop as setVfsAdapters triggers rerenders here.
  const handleLocalMount = useCallback(async () => {
    if (!engine) return;
    const local = await installLocalAdapter(engine);
    setVfsAdapters(prev => ({ temp: prev?.temp ?? null, local }));
  }, [engine]);

  if (!engine || !fsReady) {
    return (
      <div style={{
        display: 'flex', alignItems: 'center', justifyContent: 'center',
        height: '100vh', color: '#8888b0', fontSize: 14,
        flexDirection: 'column', gap: 12,
      }}>
        <div style={{
          width: 32, height: 32,
          border: '3px solid #363658', borderTop: '3px solid #7c6ff0',
          borderRadius: '50%', animation: 'spin 0.8s linear infinite',
        }} />
        <span>{initMessage || 'Initialising...'}</span>
      </div>
    );
  }

  return (
    <ErrorBoundary>
      <IDE
        engine={engine}
        status={status}
        initMessage={initMessage}
        vfsAdapters={vfsAdapters}
        onLocalMount={handleLocalMount}
      />
    </ErrorBoundary>
  );
}
