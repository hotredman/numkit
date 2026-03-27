import { useState, useEffect } from 'react';
import MLabREPL from './components/MLabREPL';
import { createWasmEngine, createFallbackEngine } from './engine';
import vfs from './vfs';

/**
 * App — initialises VFS + MLab engine (WASM or fallback)
 * On first run, populates VFS with example files from public/examples/
 */
export default function App() {
  const [engine, setEngine] = useState(null);
  const [status, setStatus] = useState('loading');
  const [initMessage, setInitMessage] = useState('');
  const [vfsReady, setVfsReady] = useState(false);

  useEffect(() => {
    let cancelled = false;

    async function init() {
      // ── 1. Init VFS ──
      try {
        setInitMessage('Initialising file system...');
        await vfs.init();

        const empty = await vfs.isEmpty();
        if (empty) {
          setInitMessage('Setting up workspace...');
          // Create default folder
          await vfs.mkdir('/My Scripts');
          await vfs.writeFile('/My Scripts/untitled.m', '% My first script\ndisp(\'Hello, MLab!\')\n');

          // Load examples from manifest and populate VFS
          try {
            const base = import.meta.env.BASE_URL || '/';
            const res = await fetch(`${base}examples/manifest.json`);
            if (res.ok) {
              const manifest = await res.json();
              for (const folder of manifest.folders) {
                const folderPath = `/Examples/${folder.name.replace(/_/g, ' ')}`;
                await vfs.mkdir(folderPath);
                for (const file of folder.files) {
                  try {
                    const fRes = await fetch(`${base}examples/${folder.name}/${file}`);
                    if (fRes.ok) {
                      const content = await fRes.text();
                      await vfs.writeFile(`${folderPath}/${file}`, content);
                    }
                  } catch (e) {
                    console.warn(`[VFS] Failed to fetch example: ${file}`, e);
                  }
                }
              }
              console.log('[VFS] Examples loaded into virtual FS');
            }
          } catch (e) {
            console.warn('[VFS] Could not load examples manifest:', e);
          }
        }

        if (!cancelled) setVfsReady(true);
      } catch (e) {
        console.error('[VFS] Init failed:', e);
        if (!cancelled) setVfsReady(true); // continue anyway
      }

      // ── 2. Init Engine ──
      try {
        const hasWasm = window.__WASM_GLUE_LOADED__ === true
                     && typeof window.createMLabModule === 'function';

        if (hasWasm) {
          setInitMessage('Loading WebAssembly...');
          const eng = await createWasmEngine(window.createMLabModule);
          if (cancelled) return;
          setEngine(eng);
          setStatus('ready');
          setInitMessage(eng.init());
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

  if (!engine || !vfsReady) {
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

  return <MLabREPL engine={engine} status={status} initMessage={initMessage} />;
}
