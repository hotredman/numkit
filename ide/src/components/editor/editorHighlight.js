// Syntax-highlight engine for the editor: the MATLAB token tables, the
// tokenizer, and the per-line HTML builder. Extracted from SyntaxEditor
// so the pure (no-React) highlight pipeline can be unit-tested directly
// — code string in, classified tokens / styled HTML out.
//
// BUILTIN_INFO is exported too: the completion popup shows its one-line
// descriptions next to each suggestion.

const KEYWORDS = new Set([
  'for','end','while','if','else','elseif','switch','case','otherwise',
  'try','catch','function','return','break','continue','global','persistent',
  'classdef','properties','methods','events','enumeration',
]);

// Brief one-line descriptions for the most common MATLAB builtins.
// Used to populate the native `title` tooltip on hover. Not exhaustive —
// unknown builtins get the generic "builtin function" label. Extend
// as needed; per-function help text would ideally come from the engine
// (no public API yet), so this is a curated map covering the basics.
export const BUILTIN_INFO = {
  disp:     'disp — display value',
  fprintf:  'fprintf — formatted print to stdout/file',
  sprintf:  'sprintf — formatted string',
  plot:     'plot — 2-D line plot',
  bar:      'bar — bar chart',
  scatter:  'scatter — 2-D scatter plot',
  hist:     'hist — histogram (legacy; prefer histogram)',
  stem:     'stem — discrete-sequence plot',
  stairs:   'stairs — stair-step plot',
  polarplot:'polarplot — plot in polar coordinates',
  figure:   'figure — create / select a figure window',
  subplot:  'subplot — create axes in tiled positions',
  title:    'title — set axes title',
  xlabel:   'xlabel — set X-axis label',
  ylabel:   'ylabel — set Y-axis label',
  legend:   'legend — add legend',
  xlim:     'xlim — get/set X-axis limits',
  ylim:     'ylim — get/set Y-axis limits',
  zeros:    'zeros(N) / zeros(M,N) — array of zeros',
  ones:     'ones(N) / ones(M,N) — array of ones',
  eye:      'eye(N) — identity matrix',
  rand:     'rand(N) — uniform random in [0,1)',
  randn:    'randn(N) — standard normal random',
  linspace: 'linspace(a,b,N) — N points evenly between a and b',
  logspace: 'logspace(a,b,N) — N log-spaced points 10^a to 10^b',
  reshape:  'reshape(A, ...) — change array shape',
  size:     'size(A) — array dimensions',
  length:   'length(A) — longest dimension',
  numel:    'numel(A) — total number of elements',
  sum:      'sum(A) — sum along first non-singleton dim',
  prod:     'prod(A) — product along first non-singleton dim',
  mean:     'mean(A) — arithmetic mean',
  min:      'min(A) — minimum value(s)',
  max:      'max(A) — maximum value(s)',
  sort:     'sort(A) — sort ascending',
  find:     'find(A) — indices of nonzero elements',
  sin:      'sin(x) — sine (radians)',
  cos:      'cos(x) — cosine (radians)',
  tan:      'tan(x) — tangent (radians)',
  sqrt:     'sqrt(x) — square root',
  abs:      'abs(x) — absolute value / complex magnitude',
  exp:      'exp(x) — natural exponential',
  log:      'log(x) — natural logarithm',
  log2:     'log2(x) — base-2 logarithm',
  log10:    'log10(x) — base-10 logarithm',
  fft:      'fft(x) — Fast Fourier Transform',
  ifft:     'ifft(X) — inverse FFT',
  conv:     'conv(a,b) — convolution',
  close:    'close — close figure window(s)',
  clear:    'clear — remove variables from workspace',
  hold:     'hold on/off — retain existing plots',
  grid:     'grid on/off — show grid',
  axis:     'axis — set axis behaviour (equal, tight, ...)',
  clc:      'clc — clear command window',
  imshow:   'imshow(I) — display image',
  imagesc:  'imagesc(C) — scale data to colormap and display',
};
const BUILTINS = new Set([
  'disp','fprintf','sprintf','plot','bar','scatter','hist','stem','stairs',
  'polarplot','semilogx','semilogy','loglog','figure','subplot','title',
  'xlabel','ylabel','legend','xlim','ylim','rlim','clf','cla','who','whos','which',
  'zeros','ones','eye','rand','randn','linspace','logspace','reshape','size',
  'length','numel','sum','prod','mean','min','max','cumsum','sort','find',
  'sin','cos','tan','asin','acos','atan','atan2','sqrt','abs','exp','log',
  'log2','log10','floor','ceil','round','mod','rem','sign','real','imag','conj',
  'upper','lower','strcmp','strcmpi','strcat','strsplit','num2str',
  'thetadir','thetazero','thetalim','exist','isempty','isnumeric','ischar',
  'close','clear','hold','grid','axis','clc',
  'input','error','warning','class','fieldnames','struct','cell',
  'cat','horzcat','vertcat','repmat','cross','dot','norm','det','inv','eig',
  'fft','ifft','conv','deconv','poly','roots','interp1',
]);
const CONSTANTS = new Set(['pi','eps','inf','Inf','nan','NaN','true','false','i','j','end']);
const PARAMS = new Set(['on','off','all','minor','equal','tight','auto','ij','xy','clockwise','counterclockwise','top','bottom','left','right']);

export function tokenize(code) {
  const tokens = []; let i = 0; const n = code.length;
  while (i < n) {
    if (code[i] === '%') { let j = i; while (j < n && code[j] !== '\n') j++; tokens.push({ text: code.slice(i, j), type: 'comment' }); i = j; continue; }
    if (code[i] === "'") {
      if (i > 0 && /[a-zA-Z0-9_)\].]/.test(code[i - 1])) { tokens.push({ text: "'", type: 'operator' }); i++; continue; }
      let j = i + 1; while (j < n && code[j] !== "'" && code[j] !== '\n') j++; if (j < n && code[j] === "'") j++;
      tokens.push({ text: code.slice(i, j), type: 'string' }); i = j; continue;
    }
    if (/[0-9]/.test(code[i]) || (code[i] === '.' && i + 1 < n && /[0-9]/.test(code[i + 1]))) {
      let j = i; while (j < n && /[0-9]/.test(code[j])) j++;
      if (j < n && code[j] === '.') { j++; while (j < n && /[0-9]/.test(code[j])) j++; }
      if (j < n && (code[j] === 'e' || code[j] === 'E')) { j++; if (j < n && (code[j] === '+' || code[j] === '-')) j++; while (j < n && /[0-9]/.test(code[j])) j++; }
      if (j < n && (code[j] === 'i' || code[j] === 'j') && (j + 1 >= n || !/[a-zA-Z0-9_]/.test(code[j + 1]))) j++;
      tokens.push({ text: code.slice(i, j), type: 'number' }); i = j; continue;
    }
    if (/[a-zA-Z_]/.test(code[i])) {
      let j = i; while (j < n && /[a-zA-Z0-9_]/.test(code[j])) j++; const w = code.slice(i, j);
      let type = 'plain';
      if (KEYWORDS.has(w)) type = 'keyword'; else if (CONSTANTS.has(w)) type = 'constant'; else if (BUILTINS.has(w)) type = 'builtin'; else if (PARAMS.has(w)) type = 'param';
      tokens.push({ text: w, type }); i = j; continue;
    }
    if (i + 1 < n) { const two = code.slice(i, i + 2); if (['==','~=','<=','>=','&&','||','.*','./','.*','.^',".'",'.\\'].includes(two)) { tokens.push({ text: two, type: 'operator' }); i += 2; continue; } }
    if ('+-*/\\^~<>=&|@;,'.includes(code[i])) { tokens.push({ text: code[i], type: 'operator' }); i++; continue; }
    if (code[i] === '\n') { tokens.push({ text: '\n', type: 'newline' }); i++; continue; }
    if (/\s/.test(code[i])) { let j = i; while (j < n && /\s/.test(code[j]) && code[j] !== '\n') j++; tokens.push({ text: code.slice(i, j), type: 'plain' }); i = j; continue; }
    tokens.push({ text: code[i], type: 'plain' }); i++;
  }
  return tokens;
}

// Build the syntax-highlighted HTML for the editor's highlight layer.
// One <span style="display:block"> per line so per-line backgrounds
// (error / debug / current-line) and scroll-sync work inside a single
// <pre>. Pure: every theme-dependent input arrives via `opts`:
//   colorMap                 token-type -> CSS colour (incl. `plain`)
//   C                        theme (fallback colour + line-bg tints)
//   errorLine / debugLine    1-indexed lines to flag (or null)
//   showCurrentLine + caretLine   current-line band
export function buildHighlightHtml(value, { colorMap, C, errorLine, debugLine, showCurrentLine, caretLine }) {
  const tokens = tokenize(value || '');
  const lines = [[]]; // array of arrays of tokens per line
  for (const t of tokens) {
    if (t.type === 'newline') lines.push([]);
    else lines[lines.length - 1].push(t);
  }
  return lines.map((toks, i) => {
    const ln = i + 1;
    const inner = toks.map((t) => {
      const e = t.text.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
      if (t.type === 'plain') return e;
      const s = `color:${colorMap[t.type]||C.text};${t.type==='keyword'?'font-weight:600;':''}${t.type==='comment'?'font-style:italic;':''}`;
      return `<span style="${s}">${e}</span>`;
    }).join('');
    let style = 'display:block;height:20px;line-height:20px;padding-right:16px;';
    if (ln === errorLine) style += `background:${C.red}18;border-left:2px solid ${C.red};margin-left:-2px;`;
    else if (ln === debugLine) style += `background:${C.orange}22;border-left:2px solid ${C.orange};margin-left:-2px;`;
    else if (showCurrentLine && ln === caretLine) style += `background:${C.text}0c;`;
    return `<span style="${style}">${inner || ' '}</span>`;
  }).join('');
}
