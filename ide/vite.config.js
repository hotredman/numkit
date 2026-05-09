import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

export default defineConfig({
  plugins: [react()],
  base: '/numkit-m/',      // ← add this line
  build: {
    outDir: 'dist',
    // Sourcemaps inflate the production bundle by ~1.4 MB (and Electron
    // keeps them in renderer memory). They're useful in dev (vite serves
    // them inline anyway) — for production builds, the cost outweighs
    // the benefit. Set NUMKIT_BUILD_SOURCEMAP=1 to opt in if a prod crash
    // needs source-mapped traces.
    sourcemap: process.env.NUMKIT_BUILD_SOURCEMAP === '1',
  },
  server: {
    port: 3000,
    host: '127.0.0.1',
    headers: {
      'Cross-Origin-Opener-Policy': 'same-origin',
      'Cross-Origin-Embedder-Policy': 'require-corp',
    },
  },
  assetsInclude: ['**/*.wasm'],
  // Worker bundling format. Default is IIFE which doesn't support
  // ES `import` — our temporary-worker.js imports sab-protocol.js,
  // so we need module-format workers (Chromium / Electron 33 support
  // them natively).
  worker: { format: 'es' },
  optimizeDeps: {
    exclude: ['numkit_ide'],
  },
});