import { describe, it, expect } from 'vitest';
import { tokenize, buildHighlightHtml, BUILTIN_INFO } from './editorHighlight';

// The tokenizer + HTML builder were inline + untested inside SyntaxEditor.
// As a pure module they can be asserted directly: code string -> tokens,
// and (tokens + theme) -> styled HTML.

describe('tokenize — MATLAB token classification', () => {
  it('classifies keywords', () => {
    expect(tokenize('for')[0].type).toBe('keyword');
    expect(tokenize('end')[0].type).toBe('keyword');
  });
  it('classifies builtins vs plain identifiers', () => {
    expect(tokenize('fft')[0].type).toBe('builtin');   // fft is in BUILTINS
    expect(tokenize('myvar')[0].type).toBe('plain');
  });
  it('classifies constants', () => {
    expect(tokenize('pi')[0].type).toBe('constant');
  });
  it('classifies numbers incl. scientific + imaginary', () => {
    expect(tokenize('3.14')[0]).toMatchObject({ type: 'number', text: '3.14' });
    expect(tokenize('1e-9')[0].type).toBe('number');
    expect(tokenize('2i')[0].type).toBe('number');
  });
  it('classifies % comments to end of line', () => {
    const c = tokenize('x = 1 % hi\ny').find((t) => t.type === 'comment');
    expect(c.text).toBe('% hi');
  });
  it('classifies strings but treats post-identifier quote as transpose', () => {
    expect(tokenize("'abc'")[0]).toMatchObject({ type: 'string', text: "'abc'" });
    expect(tokenize("a'")[1]).toMatchObject({ type: 'operator', text: "'" });
  });
  it('emits newline tokens', () => {
    expect(tokenize('a\nb').some((t) => t.type === 'newline')).toBe(true);
  });
  it('classifies one- and two-char operators', () => {
    expect(tokenize('a == b').some((t) => t.type === 'operator' && t.text === '==')).toBe(true);
    expect(tokenize('a .* b').some((t) => t.text === '.*')).toBe(true);
    expect(tokenize('a + b').some((t) => t.type === 'operator' && t.text === '+')).toBe(true);
  });
});

describe('buildHighlightHtml', () => {
  const C = { text: '#eee', red: '#f00', orange: '#fa0' };
  const colorMap = {
    keyword: '#00f', comment: '#888', plain: '#eee', number: '#0f0',
    string: '#fa0', operator: '#aaa', constant: '#0ff', builtin: '#f0f', param: '#ff0',
  };
  const base = { colorMap, C, errorLine: null, debugLine: null, showCurrentLine: false, caretLine: 0 };

  it('wraps each line in its own display:block span', () => {
    const html = buildHighlightHtml('a\nb', base);
    expect((html.match(/display:block/g) || []).length).toBe(2);
  });
  it('bolds keywords and italicises comments', () => {
    expect(buildHighlightHtml('for', base)).toContain('font-weight:600');
    expect(buildHighlightHtml('% c', base)).toContain('font-style:italic');
  });
  it('escapes &, <, >', () => {
    const html = buildHighlightHtml('a < b & c > d', base);
    expect(html).toContain('&lt;');
    expect(html).toContain('&amp;');
    expect(html).toContain('&gt;');
  });
  it('tints the error line with the red background', () => {
    expect(buildHighlightHtml('a\nb\nc', { ...base, errorLine: 2 })).toContain(`${C.red}18`);
  });
  it('tints the debug line with the orange background', () => {
    expect(buildHighlightHtml('a\nb', { ...base, debugLine: 1 })).toContain(`${C.orange}22`);
  });
  it('bands the current line only when showCurrentLine is on', () => {
    expect(buildHighlightHtml('a\nb', { ...base, showCurrentLine: true, caretLine: 1 }))
      .toContain(`${C.text}0c`);
    expect(buildHighlightHtml('a\nb', { ...base, showCurrentLine: false, caretLine: 1 }))
      .not.toContain(`${C.text}0c`);
  });
  it('renders an empty buffer as a single placeholder line', () => {
    expect(buildHighlightHtml('', base)).toContain('> </span>');
  });
});

describe('BUILTIN_INFO', () => {
  it('is a non-empty description map', () => {
    expect(typeof BUILTIN_INFO).toBe('object');
    expect(Object.keys(BUILTIN_INFO).length).toBeGreaterThan(0);
  });
});
