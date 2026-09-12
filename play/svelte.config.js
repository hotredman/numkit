import { vitePreprocess } from '@sveltejs/vite-plugin-svelte';

// Svelte 5 handles TS in <script lang="ts"> natively; vitePreprocess
// keeps svelte-check + the plugin aligned on one pipeline.
export default {
  preprocess: vitePreprocess(),
};
