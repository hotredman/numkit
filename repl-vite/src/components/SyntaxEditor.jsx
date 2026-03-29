import { useRef, useEffect, useCallback, forwardRef, useImperativeHandle } from 'react';
import { getTheme, FONT } from '../theme';

/**
 * MATLAB syntax tokenizer.
 * Returns array of {text, type} where type is:
 *   'keyword' | 'function' | 'number' | 'string' | 'comment' | 'operator' | 'plain'
 */
const KEYWORDS = new Set([
  'for','end','while','if','else','elseif','switch','case','otherwise',
  'try','catch','function','return','break','continue','global','persistent',
  'true','false','clear','close','hold','grid','axis',
]);

const BUILTINS = new Set([
  'disp','fprintf','sprintf','plot','bar','scatter','hist','stem','stairs',
  'polarplot','semilogx','semilogy','loglog','figure','subplot','title',
  'xlabel','ylabel','legend','xlim','ylim','rlim','clf','who','whos','which',
  'zeros','ones','eye','rand','randn','linspace','logspace','reshape','size',
  'length','numel','sum','prod','mean','min','max','cumsum','sort',
  'sin','cos','tan','asin','acos','atan','atan2','sqrt','abs','exp','log',
  'log2','log10','floor','ceil','round','mod','rem','sign','real','imag','conj',
  'upper','lower','strcmp','strcmpi','strcat','strsplit','num2str',
  'thetadir','thetazero','thetalim','exist','isempty','isnumeric','ischar',
]);

function tokenize(code) {
  const tokens = [];
  let i = 0;
  const n = code.length;

  while (i < n) {
    // Comment: % to end of line
    if (code[i] === '%') {
      let j = i;
      while (j < n && code[j] !== '\n') j++;
      tokens.push({ text: code.slice(i, j), type: 'comment' });
      i = j;
      continue;
    }

    // String: 'text' (but not transpose after identifier/number/bracket)
    if (code[i] === "'") {
      // Check if this is transpose (preceded by alphanum, ), ], ., ')
      if (i > 0) {
        const prev = code[i - 1];
        if (/[a-zA-Z0-9_)\].]/.test(prev)) {
          tokens.push({ text: "'", type: 'operator' });
          i++;
          continue;
        }
      }
      let j = i + 1;
      while (j < n && code[j] !== "'" && code[j] !== '\n') j++;
      if (j < n && code[j] === "'") j++;
      tokens.push({ text: code.slice(i, j), type: 'string' });
      i = j;
      continue;
    }

    // Number: digits, optional dot, optional exponent, optional imaginary
    if (/[0-9]/.test(code[i]) || (code[i] === '.' && i + 1 < n && /[0-9]/.test(code[i + 1]))) {
      let j = i;
      while (j < n && /[0-9]/.test(code[j])) j++;
      if (j < n && code[j] === '.') { j++; while (j < n && /[0-9]/.test(code[j])) j++; }
      if (j < n && (code[j] === 'e' || code[j] === 'E')) {
        j++;
        if (j < n && (code[j] === '+' || code[j] === '-')) j++;
        while (j < n && /[0-9]/.test(code[j])) j++;
      }
      if (j < n && (code[j] === 'i' || code[j] === 'j')) j++; // imaginary
      tokens.push({ text: code.slice(i, j), type: 'number' });
      i = j;
      continue;
    }

    // Identifier or keyword
    if (/[a-zA-Z_]/.test(code[i])) {
      let j = i;
      while (j < n && /[a-zA-Z0-9_]/.test(code[j])) j++;
      const word = code.slice(i, j);
      let type = 'plain';
      if (KEYWORDS.has(word)) type = 'keyword';
      else if (BUILTINS.has(word)) type = 'function';
      tokens.push({ text: word, type });
      i = j;
      continue;
    }

    // Multi-char operators
    if (i + 1 < n) {
      const two = code.slice(i, i + 2);
      if (['==','~=','<=','>=','&&','||','.*','./','.*','.^',".'",'.\\','--'].includes(two)) {
        tokens.push({ text: two, type: 'operator' });
        i += 2;
        continue;
      }
    }

    // Single-char operators
    if ('+-*/\\^~<>=&|@:;,()[]{}!'.includes(code[i])) {
      tokens.push({ text: code[i], type: 'operator' });
      i++;
      continue;
    }

    // Newline / whitespace / other — plain
    if (code[i] === '\n') {
      tokens.push({ text: '\n', type: 'plain' });
      i++;
      continue;
    }

    // Whitespace
    if (/\s/.test(code[i])) {
      let j = i;
      while (j < n && /\s/.test(code[j]) && code[j] !== '\n') j++;
      tokens.push({ text: code.slice(i, j), type: 'plain' });
      i = j;
      continue;
    }

    // Anything else
    tokens.push({ text: code[i], type: 'plain' });
    i++;
  }

  return tokens;
}

/**
 * SyntaxEditor — textarea with syntax highlighting overlay.
 *
 * Props:
 *   value        — code string
 *   onChange     — (newValue) => void
 *   onScroll     — (scrollTop) => void  (for gutter sync)
 *   errorLine    — number | null
 *   theme        — theme object from getTheme()
 */
const SyntaxEditor = forwardRef(function SyntaxEditor({ value, onChange, onScroll, errorLine, theme }, ref) {
  const textareaRef = useRef(null);
  const highlightRef = useRef(null);

  useImperativeHandle(ref, () => ({
    get scrollTop() { return textareaRef.current?.scrollTop || 0; },
    focus: () => textareaRef.current?.focus(),
  }));

  const C = theme;

  const colorMap = {
    keyword: C.synKeyword,
    function: C.synFunction,
    number: C.synNumber,
    string: C.synString,
    comment: C.synComment,
    operator: C.synOperator,
    plain: C.text,
  };

  // Sync scroll between textarea and highlight overlay
  const syncScroll = useCallback(() => {
    if (highlightRef.current && textareaRef.current) {
      highlightRef.current.scrollTop = textareaRef.current.scrollTop;
      highlightRef.current.scrollLeft = textareaRef.current.scrollLeft;
    }
    if (onScroll && textareaRef.current) onScroll(textareaRef.current.scrollTop);
  }, [onScroll]);

  // Build highlighted HTML
  const tokens = tokenize(value || '');
  const highlightedHTML = tokens.map(t => {
    const escaped = t.text
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;');
    if (t.type === 'plain') return escaped;
    return `<span style="color:${colorMap[t.type] || C.text}">${escaped}</span>`;
  }).join('');

  return (
    <div style={{ position: 'relative', width: '100%', height: '100%', overflow: 'hidden' }}>
      {/* Error line highlight */}
      {errorLine && (
        <div style={{
          position: 'absolute',
          left: 0, right: 0,
          top: (errorLine - 1) * 20 + 8,
          height: 20,
          background: `${C.red}12`,
          borderLeft: `2px solid ${C.red}`,
          pointerEvents: 'none',
          zIndex: 1,
        }} />
      )}
      {/* Syntax highlight layer (behind textarea) */}
      <pre
        ref={highlightRef}
        aria-hidden="true"
        style={{
          position: 'absolute',
          top: 0, left: 0, right: 0, bottom: 0,
          margin: 0,
          padding: 8,
          fontFamily: FONT,
          fontSize: 13,
          lineHeight: '20px',
          color: C.text,
          background: 'transparent',
          border: 'none',
          overflow: 'auto',
          whiteSpace: 'pre-wrap',
          wordBreak: 'break-word',
          pointerEvents: 'none',
          zIndex: 2,
        }}
        dangerouslySetInnerHTML={{ __html: highlightedHTML + '\n' }}
      />
      {/* Actual textarea (transparent text, receives input) */}
      <textarea
        ref={textareaRef}
        value={value}
        onChange={e => onChange(e.target.value)}
        onScroll={syncScroll}
        spellCheck={false}
        style={{
          position: 'relative',
          width: '100%',
          height: '100%',
          margin: 0,
          padding: 8,
          fontFamily: FONT,
          fontSize: 13,
          lineHeight: '20px',
          color: 'transparent',
          caretColor: C.accent,
          background: 'transparent',
          border: 'none',
          outline: 'none',
          resize: 'none',
          overflow: 'auto',
          whiteSpace: 'pre-wrap',
          wordBreak: 'break-word',
          zIndex: 3,
        }}
      />
    </div>
  );
});

export default SyntaxEditor;
