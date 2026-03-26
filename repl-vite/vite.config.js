import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

export default defineConfig({
  plugins: [react()],
  build: {
    outDir: 'dist',
    sourcemap: true,
  },
  server: {
    port: 3000,
    // Headers needed for SharedArrayBuffer (if you use threads later)
    headers: {
      'Cross-Origin-Opener-Policy': 'same-origin',
      'Cross-Origin-Embedder-Policy': 'require-corp',
    },
  },
  // Treat WASM glue code as external asset
  assetsInclude: ['**/*.wasm'],
  optimizeDeps: {
    // Don't try to bundle the Emscripten glue
    exclude: ['mlab_repl'],
  },
});
