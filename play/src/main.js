import { mount } from 'svelte';
import App from './App.svelte';
import { gallery } from './gallery.generated.js';

const app = mount(App, {
  target: document.getElementById('app'),
  props: { gallery },
});

export default app;
