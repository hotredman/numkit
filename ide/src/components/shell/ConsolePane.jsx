/**
 * v3 Console — mockup-styled REPL with full functionality from the legacy
 * Console: history (Up/Down), Tab autocomplete, inline help cards, Ctrl+L
 * to clear.
 *
 * Uses mockup CSS classes (.console, .console-out, .console-input,
 * .con-line, .con-cmd, .con-log, .con-ok, .con-err, .con-prompt) so the
 * dock body matches the design without inline-style overrides.
 */
import {
  forwardRef, useCallback, useEffect, useImperativeHandle,
  useRef, useState,
} from 'react';
import HELP_DB from '../../data/help';

const ConsolePane = forwardRef(function ConsolePane(
  { engine, output, onAddOutput, onRunCode, helpTopic, onSetHelpTopic },
  ref,
) {
  const [inputVal, setInputVal] = useState('');
  const [history, setHistory] = useState([]);
  const [histIdx, setHistIdx] = useState(-1);
  const [savedInput, setSavedInput] = useState('');
  const [acItems, setAcItems] = useState([]);
  const [acIdx, setAcIdx] = useState(-1);
  const [acPartial, setAcPartial] = useState('');

  const outRef = useRef(null);
  const inputRef = useRef(null);

  useImperativeHandle(ref, () => ({ focus: () => inputRef.current?.focus() }));

  useEffect(() => {
    requestAnimationFrame(() => {
      if (outRef.current) outRef.current.scrollTop = outRef.current.scrollHeight;
    });
  }, [output]);

  const submit = useCallback(() => {
    const val = inputVal.trim();
    if (!val) return;
    onAddOutput([{ type: 'input', text: val }]);
    setHistory((p) => {
      const h = [...p, val];
      return h.length > 200 ? h.slice(-200) : h;
    });
    setHistIdx(-1);
    setInputVal('');
    setAcItems([]);
    const hm = val.match(/^help\s+(\w+)$/);
    if (hm && HELP_DB[hm[1]]) { onSetHelpTopic(hm[1]); }
    if (val === 'help') { onSetHelpTopic(null); }
    onRunCode(val);
  }, [inputVal, onAddOutput, onRunCode, onSetHelpTopic]);

  const onKeyDown = useCallback((e) => {
    // Autocomplete navigation
    if (acItems.length > 0) {
      if (e.key === 'ArrowDown') { e.preventDefault(); setAcIdx((i) => (i + 1) % acItems.length); return; }
      if (e.key === 'ArrowUp')   { e.preventDefault(); setAcIdx((i) => (i - 1 + acItems.length) % acItems.length); return; }
      if ((e.key === 'Enter' || e.key === 'Tab') && acIdx >= 0) {
        e.preventDefault();
        const item = acItems[acIdx];
        const val = inputVal;
        const cur = inputRef.current?.selectionStart || val.length;
        let ws = cur - 1;
        while (ws >= 0 && /[a-zA-Z0-9_]/.test(val[ws])) ws--;
        ws++;
        setInputVal(val.substring(0, ws) + item + val.substring(cur));
        setAcItems([]);
        return;
      }
      if (e.key === 'Escape') { setAcItems([]); return; }
    }
    // Submit
    if (e.key === 'Enter' && !e.shiftKey) { e.preventDefault(); submit(); return; }
    // History
    if (e.key === 'ArrowUp' && !e.shiftKey && !inputVal.includes('\n')) {
      e.preventDefault();
      if (!history.length) return;
      const ni = histIdx === -1 ? history.length - 1 : Math.max(0, histIdx - 1);
      if (histIdx === -1) setSavedInput(inputVal);
      setHistIdx(ni);
      setInputVal(history[ni]);
      return;
    }
    if (e.key === 'ArrowDown' && !e.shiftKey && !inputVal.includes('\n')) {
      e.preventDefault();
      if (histIdx === -1) return;
      if (histIdx < history.length - 1) {
        setHistIdx(histIdx + 1);
        setInputVal(history[histIdx + 1]);
      } else {
        setHistIdx(-1);
        setInputVal(savedInput);
      }
      return;
    }
    // Tab autocomplete
    if (e.key === 'Tab') {
      e.preventDefault();
      const val = inputVal;
      const cur = inputRef.current?.selectionStart || val.length;
      let ws = cur - 1;
      while (ws >= 0 && /[a-zA-Z0-9_]/.test(val[ws])) ws--;
      ws++;
      const partial = val.substring(ws, cur);
      if (partial) {
        const items = engine.complete(partial);
        if (items.length === 1) {
          setInputVal(val.substring(0, ws) + items[0] + val.substring(cur));
          setAcItems([]);
        } else if (items.length > 1) {
          setAcItems(items);
          setAcIdx(0);
          setAcPartial(partial);
        }
      }
      return;
    }
    if (e.key === 'l' && e.ctrlKey) {
      e.preventDefault();
      onAddOutput([{ text: '__CLEAR__' }]);
    }
  }, [inputVal, submit, history, histIdx, savedInput, acItems, acIdx, engine, onAddOutput]);

  const lineClass = (t) => {
    if (t === 'input')   return 'con-cmd';
    if (t === 'error')   return 'con-err';
    if (t === 'warning') return 'con-warn';
    if (t === 'system')  return 'con-log';
    if (t === 'result')  return 'con-log';
    return 'con-log';
  };

  return (
    <div className="console">
      <div ref={outRef} className="console-out">
        {output.map((item, i) => {
          if (item.type === 'input') {
            return <div key={i} className="con-line con-cmd">{`>> ${item.text}`}</div>;
          }
          return <div key={i} className={`con-line ${lineClass(item.type)}`}>{item.text}</div>;
        })}
        {helpTopic && HELP_DB[helpTopic] && (
          <div style={{
            background: 'var(--bg-2)', border: '1px solid var(--line)',
            borderRadius: 6, padding: '8px 12px', margin: '4px 0',
            position: 'relative',
          }}>
            <button onClick={() => onSetHelpTopic(null)}
              style={{
                position: 'absolute', top: 4, right: 6,
                background: 'transparent', border: 'none',
                color: 'var(--fg-3)', cursor: 'pointer', fontSize: 14,
              }}>×</button>
            <div style={{ fontSize: 13, fontWeight: 700, color: 'var(--accent)', marginBottom: 3 }}>
              {HELP_DB[helpTopic].sig}
            </div>
            <div style={{ fontSize: 11, color: 'var(--fg-1)', marginBottom: 3 }}>
              {HELP_DB[helpTopic].desc}
            </div>
            <div style={{ fontSize: 10, color: 'var(--fg-3)' }}>
              Category: {HELP_DB[helpTopic].cat}
            </div>
            <div style={{
              fontSize: 11, color: 'var(--accent)', marginTop: 3,
              fontFamily: 'var(--font-mono)',
            }}>{HELP_DB[helpTopic].ex}</div>
          </div>
        )}
      </div>

      <div className="console-input" style={{ position: 'relative' }}>
        <span className="con-prompt">&gt;&gt;</span>
        {acItems.length > 1 && (
          <div style={{
            position: 'absolute', bottom: 'calc(100% + 4px)', left: 24,
            minWidth: 160, maxWidth: 320, maxHeight: 160, overflowY: 'auto',
            background: 'var(--bg-3)', border: '1px solid var(--line)',
            borderRadius: 5, boxShadow: '0 -4px 16px rgba(0,0,0,0.3)', zIndex: 100,
          }}>
            {acItems.map((item, i) => (
              <div key={item}
                onClick={() => {
                  const val = inputVal;
                  const cur = inputRef.current?.selectionStart || val.length;
                  let ws = cur - 1;
                  while (ws >= 0 && /[a-zA-Z0-9_]/.test(val[ws])) ws--;
                  ws++;
                  setInputVal(val.substring(0, ws) + item + val.substring(cur));
                  setAcItems([]);
                  inputRef.current?.focus();
                }}
                style={{
                  padding: '4px 8px', cursor: 'pointer', fontSize: 11,
                  color: i === acIdx ? 'var(--fg-0)' : 'var(--fg-2)',
                  background: i === acIdx ? 'var(--bg-4)' : 'transparent',
                  fontFamily: 'var(--font-mono)',
                }}>
                <span style={{ color: 'var(--accent)', fontWeight: 600 }}>
                  {item.substring(0, acPartial.length)}
                </span>
                {item.substring(acPartial.length)}
              </div>
            ))}
          </div>
        )}
        <input
          ref={inputRef}
          value={inputVal}
          onChange={(e) => { setInputVal(e.target.value); setAcItems([]); }}
          onKeyDown={onKeyDown}
          spellCheck={false}
          autoComplete="off"
          placeholder="enter numkit command…"
        />
      </div>
    </div>
  );
});

export default ConsolePane;
