import { useRef, useCallback, forwardRef, useImperativeHandle } from 'react';
import { useTheme, FONT } from '../theme';

const KEYWORDS = new Set([
  'for','end','while','if','else','elseif','switch','case','otherwise',
  'try','catch','function','return','break','continue','global','persistent',
  'classdef','properties','methods','events','enumeration',
]);
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

function tokenize(code) {
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
    if (code[i] === '\n') { tokens.push({ text: '\n', type: 'plain' }); i++; continue; }
    if (/\s/.test(code[i])) { let j = i; while (j < n && /\s/.test(code[j]) && code[j] !== '\n') j++; tokens.push({ text: code.slice(i, j), type: 'plain' }); i = j; continue; }
    tokens.push({ text: code[i], type: 'plain' }); i++;
  }
  return tokens;
}

const SyntaxEditor = forwardRef(function SyntaxEditor({ value, onChange, onScroll, errorLine, debugLine }, ref) {
  const C = useTheme();
  const textareaRef = useRef(null);
  const highlightRef = useRef(null);
  useImperativeHandle(ref, () => ({ get scrollTop() { return textareaRef.current?.scrollTop || 0; }, focus: () => textareaRef.current?.focus() }));

  const colorMap = { keyword: C.synKeyword, builtin: C.synBuiltin, number: C.synNumber, string: C.synString, comment: C.synComment, operator: C.synOperator, constant: C.synConstant, param: C.synParam, plain: C.text };

  const syncScroll = useCallback(() => {
    if (highlightRef.current && textareaRef.current) { highlightRef.current.scrollTop = textareaRef.current.scrollTop; highlightRef.current.scrollLeft = textareaRef.current.scrollLeft; }
    if (onScroll && textareaRef.current) onScroll(textareaRef.current.scrollTop);
  }, [onScroll]);

  const tokens = tokenize(value || '');
  const html = tokens.map(t => {
    const e = t.text.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
    if (t.type === 'plain') return e;
    const s = `color:${colorMap[t.type]||C.text};${t.type==='keyword'?'font-weight:600;':''}${t.type==='comment'?'font-style:italic;':''}`;
    return `<span style="${s}">${e}</span>`;
  }).join('');

  return (
    <div style={{ position:'relative',width:'100%',height:'100%',overflow:'hidden' }}>
      {errorLine&&<div style={{position:'absolute',left:0,right:0,top:(errorLine-1)*20+8,height:20,background:`${C.red}18`,borderLeft:`2px solid ${C.red}`,pointerEvents:'none',zIndex:1}}/>}
      {debugLine&&<div style={{position:'absolute',left:0,right:0,top:(debugLine-1)*20+8,height:20,background:`${C.orange}22`,borderLeft:`2px solid ${C.orange}`,pointerEvents:'none',zIndex:1}}/>}
      <pre ref={highlightRef} aria-hidden="true" style={{position:'absolute',top:0,left:0,right:0,bottom:0,margin:0,padding:8,fontFamily:FONT,fontSize:13,lineHeight:'20px',color:C.text,background:'transparent',border:'none',overflow:'auto',whiteSpace:'pre-wrap',wordBreak:'break-word',pointerEvents:'none',zIndex:2}} dangerouslySetInnerHTML={{__html:html+'\n'}}/>
      <textarea ref={textareaRef} value={value} onChange={e=>onChange(e.target.value)} onScroll={syncScroll} spellCheck={false}
        style={{position:'relative',width:'100%',height:'100%',margin:0,padding:8,fontFamily:FONT,fontSize:13,lineHeight:'20px',color:'transparent',caretColor:C.accent,background:'transparent',border:'none',outline:'none',resize:'none',overflow:'auto',whiteSpace:'pre-wrap',wordBreak:'break-word',zIndex:3}}/>
    </div>
  );
});

export default SyntaxEditor;
