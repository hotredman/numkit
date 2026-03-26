import { useState, useEffect } from 'react';
import MLabREPL from './components/MLabREPL';
import { createWasmEngine, createFallbackEngine } from './engine';

/**
 * App — initialises the MLab engine (WASM or fallback)
 * and passes it down to the REPL component.
 */
export default function App() {
  const [engine, setEngine] = useState(null);
  const [status, setStatus] = useState('loading'); // loading | ready | fallback | error
  const [initMessage, setInitMessage] = useState('');

  useEffect(() => {
    let cancelled = false;

    async function init() {
      try {
        // Check if Emscripten glue loaded (set in index.html)
        const hasWasm = window.__WASM_GLUE_LOADED__ === true
                     && typeof window.createMLabModule === 'function';

        console.log('[REPL] WASM glue detected:', hasWasm);

        if (hasWasm) {
          setInitMessage('Loading WebAssembly...');
          const eng = await createWasmEngine(window.createMLabModule);
          if (cancelled) return;
          setEngine(eng);
          setStatus('ready');
          setInitMessage(eng.init());
          console.log('[REPL] WASM engine initialised');
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

  if (!engine) {
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
