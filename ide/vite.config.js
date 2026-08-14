import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

export default defineConfig({
  plugins: [react()],
  test: {
    environment: 'jsdom',
    globals: true,
    include: ['src/**/*.{test,spec}.{js,jsx}'],
  },
  // Relative base so the built site works from any location — a web host at
  // root OR a sub-path, and the Electron desktop shell over file:// (which
  // desktop-build also forces via `vite build --base ./`). Keeps the bundled
  // Examples (fetched as `${BASE_URL}examples/...`) resolving in every target.
  base: './',
  build: {
    outDir: 'dist',
    // Sourcemaps default ON. Cost: +1.4 MB to the dist (Electron renderer
    // keeps them in memory at startup). Benefit: every uncaught crash in
    // the renderer console comes back with a real file:line trace,
    // which is non-negotiable now that 3-D figures + three.js are in the
    // hot path. Set NUMKIT_BUILD_SOURCEMAP=0 to opt out for slim builds.
    sourcemap: process.env.NUMKIT_BUILD_SOURCEMAP !== '0',
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