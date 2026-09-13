// MATLAB stream tokenizer for CodeMirror 6 (rule R5 — pure module).
//
// Adapted from the CodeMirror legacy Octave mode (MIT, © Marijn
// Haverbeke et al., codemirror/legacy-modes mode/octave.js) and tuned
// to the MATLAB dialect per dev-docs/handbook/svelte_architecture.md:
//   * `...` line continuation consumes the rest of the line (MATLAB
//     comment semantics; the Octave mode lacks it)
//   * `function` is a KEYWORD (Octave mode files it under builtins)
//   * Octave-only words dropped (endif/endfunction/endfor/endwhile/
//     do/until/pkg); MATLAB set added (parfor, spmd …)
//   * `%{ … %}` block comments kept — @play manifests rely on them

import { StreamLanguage } from '@codemirror/language';
import type { StreamParser } from '@codemirror/language';

function wordRegexp(words: string[]): RegExp {
  return new RegExp('^((' + words.join(')|(') + '))\\b');
}

const singleOperators = /^[\+\-\*/&|\^~<>!@'\\]/;
const singleDelimiters = /^[\(\[\{\},:=;\.]/;
const doubleOperators = /^((==)|(~=)|(<=)|(>=)|(<<)|(>>)|(\.[\+\-\*/\^\\]))/;
const doubleDelimiters = /^((!=)|(\+=)|(\-=)|(\*=)|(\/=)|(&=)|(\|=)|(\^=))/;
const tripleDelimiters = /^((>>=)|(<<=))/;
const expressionEnd = /^[\]\)]/;
const identifiers = /^[_A-Za-z\xa1-\uffff][_A-Za-z0-9\xa1-\uffff]*/;

const builtins = wordRegexp([
  'abs', 'acos', 'asin', 'atan', 'atan2', 'atan2d', 'asind', 'acosd', 'arrayfun',
  'besselj', 'butter', 'bode', 'ceil', 'chebwin', 'cheby1', 'cheby2', 'chirp',
  'clear', 'colormap', 'conj', 'contour', 'conv', 'cos', 'cosd', 'cosh',
  'cov', 'cumsum', 'det', 'diag', 'diff', 'disp', 'eig', 'error', 'eval',
  'exp', 'expm', 'eye', 'fft', 'fftshift', 'filter', 'filtfilt', 'find',
  'fir1', 'fliplr', 'flipud', 'floor', 'freqz', 'grid', 'hamming', 'hann',
  'hilbert', 'histogram', 'histcounts', 'ifft', 'imag', 'imagesc', 'input',
  'interp1', 'inv', 'ismember', 'kaiser', 'linspace', 'log', 'log10', 'logm',
  'margin', 'max', 'mean', 'mesh', 'meshgrid', 'min', 'nextpow2', 'norm',
  'num2str', 'ode45', 'ode23', 'ones', 'parula', 'plot', 'plot3', 'polyval',
  'prod', 'pwelch', 'rand', 'randn', 'rank', 'real', 'reshape', 'roots',
  'round', 'sign', 'sin', 'sind', 'sinh', 'size', 'sqrt', 'sqrtm', 'std',
  'step', 'subplot', 'sum', 'surf', 'tan', 'tand', 'tanh', 'taylorwin',
  'text', 'tf', 'title', 'trace', 'zeros', 'spectrogram', 'xlabel',
  'ylabel', 'legend',
]);

const keywords = wordRegexp([
  'arguments', 'break', 'case', 'catch', 'classdef', 'continue', 'else',
  'elseif', 'end', 'events', 'for', 'function', 'global', 'if', 'methods',
  'otherwise', 'parfor', 'persistent', 'properties', 'return', 'spmd',
  'switch', 'try', 'while',
]);

interface MatlabState { tokenize: (s: any, st: MatlabState) => string | null; }

function tokenTranspose(stream: any, state: MatlabState): string | null {
  if (!stream.sol() && stream.peek() === "'") {
    stream.next();
    state.tokenize = tokenBase;
    return 'operator';
  }
  state.tokenize = tokenBase;
  return tokenBase(stream, state);
}

function tokenComment(stream: any, state: MatlabState): string {
  if (stream.match(/^.*%}/)) {
    state.tokenize = tokenBase;
    return 'comment';
  }
  stream.skipToEnd();
  return 'comment';
}

function tokenBase(stream: any, state: MatlabState): string | null {
  if (stream.eatSpace()) return null;

  // Block comments %{ … %} (the @play manifest carrier).
  if (stream.match('%{')) {
    state.tokenize = tokenComment;
    stream.skipToEnd();
    return 'comment';
  }

  // Line comments: % … and (Octave legacy) # …
  if (stream.match(/^[%#]/)) {
    stream.skipToEnd();
    return 'comment';
  }

  // `...` continuation: the rest of the line is a comment (MATLAB-only,
  // missing from the Octave base — the fix this vendoring adds).
  if (stream.match(/^\.\.\./)) {
    stream.skipToEnd();
    return 'comment';
  }

  // Numbers.
  if (stream.match(/^[0-9.+-]/, false)) {
    if (stream.match(/^[+-]?0x[0-9a-fA-F]+[ij]?/)) return 'number';
    if (stream.match(/^[+-]?\d*\.\d+([EeDd][+-]?\d+)?[ij]?/)) return 'number';
    if (stream.match(/^[+-]?\d+([EeDd][+-]?\d+)?[ij]?/)) return 'number';
  }
  if (stream.match(wordRegexp(['nan', 'NaN', 'inf', 'Inf']))) return 'number';

  // Strings: "…" and '...' with '' escaping.
  const m = stream.match(/^"(?:[^"]|"")*("|$)/)
        ?? stream.match(/^'(?:[^']|'')*('|$)/);
  if (m) return m[1] ? 'string' : 'error';

  if (stream.match(keywords)) return 'keyword';
  if (stream.match(builtins)) return 'builtin';
  if (stream.match(identifiers)) return 'variable';

  if (stream.match(singleOperators) || stream.match(doubleOperators))
    return 'operator';
  if (stream.match(singleDelimiters) || stream.match(doubleDelimiters)
      || stream.match(tripleDelimiters))
    return null;

  if (stream.match(expressionEnd)) {
    state.tokenize = tokenTranspose;
    return null;
  }

  stream.next();
  return 'error';
}

const matlabParser: StreamParser<MatlabState> = {
  name: 'matlab',
  startState: () => ({ tokenize: tokenBase }),
  token: (stream, state) => {
    const style = state.tokenize(stream, state);
    if (style === 'number' || style === 'variable')
      state.tokenize = tokenTranspose;
    return style;
  },
  languageData: { commentTokens: { line: '%' } },
};

export const matlab = StreamLanguage.define(matlabParser);
