import { useState, useEffect, useCallback } from 'react';
import MLabREPL from './components/MLabREPL';
import { createWasmEngine, createFallbackEngine } from './engine';
import tempFS from './temporary';
import { installVfsAdapters, installLocalAdapter } from './fs/vfs-adapter';

/**
 * App — initialises Temporary FS + MLab engine (WASM or fallback).
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
      // ── 1. Init Temporary FS ──
      try {
        setInitMessage('Initialising file system...');
        await tempFS.init();
        if (!cancelled) setFsReady(true);
      } catch (e) {
        console.error('[TemporaryFS] Init failed:', e);
        if (!cancelled) setFsReady(true); // continue anyway
      }

      // ── 2. Init Engine ──
      try {
        const hasWasm = window.__WASM_GLUE_LOADED__ === true
                     && typeof window.createMLabModule === 'function';

        if (hasWasm) {
          setInitMessage('Loading WebAssembly...');
          const eng = await createWasmEngine(window.createMLabModule);
          if (cancelled) return;

          // IMPORTANT: call init() BEFORE registering VFS adapters.
          // repl_init() constructs a fresh ReplSession on the C++ side; if
          // registration happens first, init() replaces the session and
          // silently drops every registered VirtualFS, producing cryptic
          // "filesystem 'temporary' is not available" errors later.
          const greeting = eng.init();

          // Mirror tempFS (and local, if mounted) into sync callbacks the
          // engine can invoke from csvread/csvwrite. Failures here must not
          // block the REPL — user can still run code, just without file I/O
          // routed to the IDE's virtual filesystems.
          try {
            const adapters = await installVfsAdapters(eng);
            if (!cancelled) setVfsAdapters(adapters);
          } catch (e) {
            console.error('[VFS] adapter install failed:', e);
          }

          setEngine(eng);
          setStatus('ready');
          setInitMessage(greeting);
        } else {
          throw new Error('WASM glue not loaded');
        }
      } catch (err) {
        if (cancelled) return;
        console.log('[REPL] Using fallback engine:', err.message);
        const eng = createFallbackEngine();
        setEngine(eng);
        setStatus('fallback');
        setInitMessage('Running in demo mode (no WASM binary detected).');
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
    setVfsAdapters(prev => ({ ...(prev || { temp: null }), local }));
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
    <MLabREPL
      engine={engine}
      status={status}
      initMessage={initMessage}
      vfsAdapters={vfsAdapters}
      onLocalMount={handleLocalMount}
    />
  );
}
