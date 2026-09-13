// CodeMirror 6 theme + highlight style built from the Play design
// tokens (R4 — the editor never sees a hex literal of its own). The
// token constants are applied as THEME CSS here; Svelte <style> blocks
// are not involved (CM6 owns its DOM).

import { EditorView } from '@codemirror/view';
import { HighlightStyle, syntaxHighlighting } from '@codemirror/language';
import { tags as t } from '@lezer/highlight';
import { T } from '../tokens.js';

export const matlabTheme = EditorView.theme({
  '&': {
    height: '100%',
    backgroundColor: T.bg,
    color: T.text,
    fontSize: '12.5px',
  },
  '.cm-scroller': {
    fontFamily: T.fontMono,
    lineHeight: '1.55',
    overflow: 'auto',
  },
  '.cm-gutters': {
    backgroundColor: T.bgPanel,
    color: T.textFaint,
    border: 'none',
    borderRight: `1px solid ${T.border}`,
  },
  '.cm-activeLine': { backgroundColor: T.bgHover },
  '.cm-activeLineGutter': { backgroundColor: T.bgHover, color: T.textDim },
  '.cm-selectionBackground, &.cm-focused .cm-selectionBackground': {
    backgroundColor: T.bgActive,
  },
  '.cm-cursor': { borderLeftColor: T.accent },
}, { dark: true });

export const matlabHighlighting = syntaxHighlighting(
  HighlightStyle.define([
    { tag: t.comment, color: T.textFaint, fontStyle: 'italic' },
    { tag: t.keyword, color: T.accentSoft },
    { tag: t.string, color: T.series[2] },
    { tag: t.number, color: T.series[1] },
    { tag: t.bool, color: T.series[1] },
    { tag: t.operator, color: T.series[4] },
    { tag: t.variableName, color: T.text },
    { tag: t.function(t.variableName), color: T.series[3] },
    { tag: t.standard(t.variableName), color: T.series[3] },
    { tag: t.definition(t.variableName), color: T.text },
    { tag: t.invalid, color: T.danger },
  ]));
