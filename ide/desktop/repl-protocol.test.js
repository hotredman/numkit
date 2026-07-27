// ide/desktop/repl-protocol.test.js
//
// Unit tests for repl-protocol.js — figure extraction, debug sub-protocol,
// and the streaming extractResponse() splitter.
//
// Run with:  npx --yes vitest run ide/desktop/repl-protocol.test.js
// or:        npx --yes jest    ide/desktop/repl-protocol.test.js

import { describe, it, expect } from 'vitest';
import { parseReplChunk, extractResponse, extractFigureMarkers } from './repl-protocol.js';

// ─── extractFigureMarkers ────────────────────────────────────────────────────

describe('extractFigureMarkers', () => {
  it('returns clean output unchanged when no markers present', () => {
    const { cleanOutput, figures, closedFigureIds, closeAllFigures }
      = extractFigureMarkers('x = 42\nans = 1');
    expect(cleanOutput).toBe('x = 42\nans = 1');
    expect(figures).toEqual([]);
    expect(closedFigureIds).toEqual([]);
    expect(closeAllFigures).toBe(false);
  });

  it('strips __FIGURE_DATA__: and returns parsed figure', () => {
    const output = 'before\n__FIGURE_DATA__:{"id":1,"type":"line","x":[1,2,3]}\nafter';
    const { cleanOutput, figures } = extractFigureMarkers(output);
    expect(cleanOutput).toBe('before\nafter');
    expect(figures).toHaveLength(1);
    expect(figures[0]).toMatchObject({ id: 1, type: 'line' });
  });

  it('strips __FIGURE_CLOSE__: and returns closedFigureIds', () => {
    const output = '__FIGURE_CLOSE__:3\nsome text';
    const { cleanOutput, figures, closedFigureIds } = extractFigureMarkers(output);
    expect(cleanOutput).toBe('some text');
    expect(figures).toEqual([]);
    expect(closedFigureIds).toEqual([3]);
  });

  it('handles __FIGURE_CLOSE_ALL__', () => {
    const output = 'text\n__FIGURE_CLOSE_ALL__\nmore';
    const { cleanOutput, closeAllFigures } = extractFigureMarkers(output);
    expect(cleanOutput).toBe('text\nmore');
    expect(closeAllFigures).toBe(true);
  });

  it('handles multiple figures', () => {
    const f1 = '{"id":1,"type":"bar"}';
    const f2 = '{"id":2,"type":"scatter"}';
    const output = `__FIGURE_DATA__:${f1}\n__FIGURE_DATA__:${f2}`;
    const { figures } = extractFigureMarkers(output);
    expect(figures).toHaveLength(2);
    expect(figures[0].id).toBe(1);
    expect(figures[1].id).toBe(2);
  });

  it('handles text before __FIGURE_DATA__: on the same line', () => {
    const output = 'ans = 1\n prefix text __FIGURE_DATA__:{"id":5,"type":"line"}\nafter';
    const { cleanOutput, figures } = extractFigureMarkers(output);
    expect(cleanOutput).toContain('prefix text');
    expect(cleanOutput).toContain('after');
    expect(figures).toHaveLength(1);
    expect(figures[0].id).toBe(5);
  });

  it('handles __ERROR_LINE__: marker', () => {
    const output = '__ERROR_LINE__:7\nError: undefined variable';
    const { cleanOutput, errorLine } = extractFigureMarkers(output);
    expect(errorLine).toBe(7);
    expect(cleanOutput).toBe('Error: undefined variable');
  });

  it('returns empty string for empty input', () => {
    const { cleanOutput, figures } = extractFigureMarkers('');
    expect(cleanOutput).toBe('');
    expect(figures).toEqual([]);
  });
});

// ─── parseReplChunk — regular run ─────────────────────────────────────────────

describe('parseReplChunk — regular run', () => {
  it('parses a plain run with no figures', () => {
    const chunk = 'x = 5\n__VARS__:{"x":{"type":"double","size":"1x1","preview":"5"}}';
    const r = parseReplChunk(chunk);
    expect(r.type).toBe('run');
    expect(r.stdout).toBe('x = 5');
    expect(r.vars).toMatchObject({ x: { type: 'double' } });
    expect(r.figures).toEqual([]);
    expect(r.closedFigureIds).toEqual([]);
    expect(r.closeAll).toBe(false);
  });

  it('extracts figures from run output', () => {
    const figJson = '{"id":1,"type":"line","x":[1,2],"y":[3,4]}';
    const chunk = `x = 1\n__FIGURE_DATA__:${figJson}\n__VARS__:{}`;
    const r = parseReplChunk(chunk);
    expect(r.type).toBe('run');
    expect(r.stdout).toBe('x = 1');
    expect(r.figures).toHaveLength(1);
    expect(r.figures[0].id).toBe(1);
  });

  it('handles __FIGURE_CLOSE_ALL__ in run', () => {
    const chunk = '__FIGURE_CLOSE_ALL__\n__VARS__:{}';
    const r = parseReplChunk(chunk);
    expect(r.closeAll).toBe(true);
    expect(r.stdout).toBe('');
  });

  it('handles __FIGURE_CLOSE__: in run', () => {
    const chunk = '__FIGURE_CLOSE__:7\n__VARS__:{}';
    const r = parseReplChunk(chunk);
    expect(r.closedFigureIds).toContain(7);
  });

  it('parses introspection responses (not affected by figure changes)', () => {
    const chunk = '__SHAPE_DATA__:{"rows":3,"cols":4}';
    const r = parseReplChunk(chunk);
    expect(r.type).toBe('data');
    expect(r.data).toMatchObject({ rows: 3, cols: 4 });
  });

  it('parses __RESET_OK__', () => {
    const r = parseReplChunk('__RESET_OK__\n__VARS__:{}');
    expect(r.type).toBe('reset');
    expect(r.ok).toBe(true);
  });

  it('parses __CMD_ERROR__:', () => {
    const r = parseReplChunk('__CMD_ERROR__:unknown command');
    expect(r.type).toBe('error');
    expect(r.error).toBe('unknown command');
  });

  it('extracts __ERROR_LINE__: and sets errorLine in run result', () => {
    const chunk = '__ERROR_LINE__:5\nError: undefined variable\n__VARS__:{}';
    const r = parseReplChunk(chunk);
    expect(r.type).toBe('run');
    expect(r.errorLine).toBe(5);
    expect(r.stdout).toBe('Error: undefined variable');
    expect(r.figures).toEqual([]);
  });

  it('errorLine is null when no __ERROR_LINE__: marker present', () => {
    const r = parseReplChunk('x = 5\n__VARS__:{}');
    expect(r.errorLine).toBeNull();
  });
});

// ─── parseReplChunk — debug step ─────────────────────────────────────────────

describe('parseReplChunk — debug step (isStep=true)', () => {
  const bpBase = {
    status: 'paused',
    pauseState: {
      line: 3, col: 0, function: 'main',
      reason: 'breakpoint', selectedFrame: 0, frameCount: 1,
      variables: { x: { type: 'double', size: '1x1', preview: '5' } },
      callStack: [{ function: 'main', line: 3 }],
    },
  };

  it('parses a plain breakpoint pause', () => {
    const bpJson = JSON.stringify(bpBase);
    const chunk = `__BREAKPOINT__:${bpJson}`;
    const r = parseReplChunk(chunk, '', true);
    expect(r.type).toBe('dbstep');
    expect(r.status).toBe('paused');
    expect(r.pauseState).toMatchObject({ line: 3, reason: 'breakpoint' });
    expect(r.figures).toEqual([]);
    expect(r.output).toBe('');
  });

  it('extracts figures from breakpoint output field', () => {
    const figJson = '{"id":2,"type":"bar"}';
    const bpWithOutput = {
      ...bpBase,
      output: `print done\n__FIGURE_DATA__:${figJson}`,
    };
    const chunk = `__BREAKPOINT__:${JSON.stringify(bpWithOutput)}`;
    const r = parseReplChunk(chunk, '', true);
    expect(r.type).toBe('dbstep');
    expect(r.output).toBe('print done');
    expect(r.figures).toHaveLength(1);
    expect(r.figures[0].id).toBe(2);
  });

  it('returns error type if no __BREAKPOINT__: line in step chunk', () => {
    const r = parseReplChunk('unexpected line', '', true);
    expect(r.type).toBe('error');
    expect(r.error).toMatch(/No __BREAKPOINT__/);
  });
});

// ─── parseReplChunk — debug completion ───────────────────────────────────────

describe('parseReplChunk — debug completion', () => {
  it('parses __DEBUG_END__ with completed status', () => {
    const chunk = [
      'x = 10',
      '__VARS__:{"x":{"type":"double","size":"1x1","preview":"10"}}',
      '__DEBUG_RESULT__:{"status":"completed"}',
      '__DEBUG_END__',
    ].join('\n');
    const r = parseReplChunk(chunk);
    expect(r.type).toBe('dbend');
    expect(r.status).toBe('completed');
    expect(r.output).toBe('x = 10');
    expect(r.vars).toMatchObject({ x: { type: 'double' } });
    expect(r.figures).toEqual([]);
  });

  it('parses __DEBUG_END__ with error status', () => {
    const chunk = [
      '__VARS__:{}',
      '__DEBUG_RESULT__:{"status":"error","message":"undefined var","line":5}',
      '__DEBUG_END__',
    ].join('\n');
    const r = parseReplChunk(chunk);
    expect(r.type).toBe('dbend');
    expect(r.status).toBe('error');
    expect(r.message).toBe('undefined var');
    expect(r.line).toBe(5);
  });

  it('extracts figures from debug completion output', () => {
    const figJson = '{"id":3,"type":"scatter"}';
    const chunk = [
      `__FIGURE_DATA__:${figJson}`,
      '__VARS__:{}',
      '__DEBUG_RESULT__:{"status":"completed"}',
      '__DEBUG_END__',
    ].join('\n');
    const r = parseReplChunk(chunk);
    expect(r.type).toBe('dbend');
    expect(r.figures).toHaveLength(1);
    expect(r.figures[0].id).toBe(3);
  });

  it('parses __DEBUG_STOPPED__', () => {
    const r = parseReplChunk('__DEBUG_STOPPED__');
    expect(r.type).toBe('dbstop');
  });
});

// ─── extractResponse ──────────────────────────────────────────────────────────

describe('extractResponse', () => {
  it('returns null result when buffer is incomplete', () => {
    const { result } = extractResponse('partial output\n');
    expect(result).toBeNull();
  });

  it('splits on __END_OF_RUN__ and parses run result', () => {
    const buf = 'hello\n__VARS__:{}\n__END_OF_RUN__\n';
    const { result, remainder, isStep } = extractResponse(buf);
    expect(result).not.toBeNull();
    expect(result.type).toBe('run');
    expect(result.stdout).toBe('hello');
    expect(isStep).toBe(false);
    expect(remainder).toBe('');
  });

  it('splits on __END_OF_STEP__ and sets isStep=true', () => {
    const bpJson = JSON.stringify({
      status: 'paused',
      pauseState: { line: 1, col: 0, function: 'f', reason: 'step',
                    selectedFrame: 0, frameCount: 1, variables: {}, callStack: [] },
    });
    const buf = `__BREAKPOINT__:${bpJson}\n__END_OF_STEP__\n`;
    const { result, isStep } = extractResponse(buf);
    expect(result).not.toBeNull();
    expect(result.type).toBe('dbstep');
    expect(isStep).toBe(true);
  });

  it('picks the earliest of __END_OF_RUN__ and __END_OF_STEP__', () => {
    // __END_OF_STEP__ comes first
    const bpJson = JSON.stringify({
      status: 'paused',
      pauseState: { line: 2, col: 0, function: 'g', reason: 'breakpoint',
                    selectedFrame: 0, frameCount: 1, variables: {}, callStack: [] },
    });
    const buf = `__BREAKPOINT__:${bpJson}\n__END_OF_STEP__\nextra\n__END_OF_RUN__\n`;
    const { result, isStep, remainder } = extractResponse(buf);
    expect(isStep).toBe(true);
    expect(result.type).toBe('dbstep');
    expect(remainder).toBe('extra\n__END_OF_RUN__\n');
  });

  it('leaves remaining data after end marker in remainder', () => {
    const buf = 'first\n__VARS__:{}\n__END_OF_RUN__\nsecond\n__END_OF_RUN__\n';
    const { result, remainder } = extractResponse(buf);
    expect(result.type).toBe('run');
    expect(remainder).toBe('second\n__END_OF_RUN__\n');
  });
});
