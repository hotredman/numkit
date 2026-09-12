import { mount } from 'svelte';
import App from './App.svelte';
import { injectCssVars } from './lib/tokens.js';
import { gallery } from './gallery.generated.js';

injectCssVars();
const app = mount(App, {
  target: document.getElementById('app'),
  props: { gallery },
});

export default app;
