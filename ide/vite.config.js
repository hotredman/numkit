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
  optimizeDeps: {
    exclude: ['numkit_ide'],
  },
});