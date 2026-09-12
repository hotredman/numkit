import { defineConfig } from 'vite';
import { svelte } from '@sveltejs/vite-plugin-svelte';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

const here = path.dirname(fileURLToPath(import.meta.url));

// Pure Svelte 5 app (user decision: no React islands). The plot
// renderer is the SUCCESSOR of the IDE's React one — it consumes the
// identical engine wire JSON and shares the framework-free helpers
// from ide/src/components/plot (plotTicks, colormaps, adapters).
export default defineConfig({
  plugins: [svelte()],
  resolve: {
    alias: {
      // Shared framework-free modules (MATLAB-parity math: tick
      // ladders, colormap tables, wire adapters).
      '@ide-plot': path.resolve(here, '../ide/src/components/plot'),
      // Engine bindings loader (wasm glue loader, same shape the IDE
      // uses; imports only the pure parts — no React).
      '@ide-src': path.resolve(here, '../ide/src'),
    },
  },
  build: {
    outDir: 'dist',
    // The wasm glue + gallery scripts ride along as static assets.
    assetsInlineLimit: 0,
  },
});
