<script lang="ts">
  // MatlabEditor (R3): the CodeMirror 6 surface with MATLAB
  // highlighting (vendored tokenizer + token-driven theme). Editable;
  // reports changes via oninput, accepts external value resets.
  import { onMount } from 'svelte';
  import { EditorView, lineNumbers } from '@codemirror/view';
  import { EditorState } from '@codemirror/state';
  import { defaultKeymap, history, historyKeymap } from '@codemirror/commands';
  import { keymap } from '@codemirror/view';
  import { matlab } from './matlabLanguage.js';
  import { matlabTheme, matlabHighlighting } from './matlabTheme.js';

  interface Props {
    value?: string;
    oninput?: (v: string) => void;
  }
  let { value = '', oninput }: Props = $props();

  let host = $state<HTMLElement | null>(null);
  let view: EditorView | null = null;

  onMount(() => {
    if (!host) return;
    view = new EditorView({
      parent: host,
      state: EditorState.create({
        doc: value,
        extensions: [
          lineNumbers(),
          history(),
          keymap.of([...defaultKeymap, ...historyKeymap]),
          matlab,
          matlabTheme,
          matlabHighlighting,
          EditorView.updateListener.of((u) => {
            if (u.docChanged)
              oninput?.(u.state.doc.toString());
          }),
        ],
      }),
    });
    return () => { view?.destroy(); view = null; };
  });

  // External resets (Run/Reset reload the source): sync only when the
  // prop diverges from the document to avoid echoing our own edits.
  $effect(() => {
    if (view && value !== view.state.doc.toString())
      view.dispatch({
        changes: { from: 0, to: view.state.doc.length, insert: value },
      });
  });
</script>

<div class="editor" bind:this={host}></div>

<style>
  .editor { flex: 1; min-height: 0; overflow: hidden; }
  .editor :global(.cm-editor) { height: 100%; }
</style>
