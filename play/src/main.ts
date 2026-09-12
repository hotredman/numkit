import { mount } from 'svelte';
import App from './App.svelte';
import { injectCssVars } from './lib/tokens.js';
import { gallery } from './gallery.generated.js';

injectCssVars();
const target = document.getElementById('app');
if (!target) throw new Error('#app mount point missing');
const app = mount(App, {
  target,
  props: { gallery },
});

export default app;
