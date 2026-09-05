// ide/desktop/protocol-integration.test.js
//
// Integration tests for the native REPL pipe protocol.
// These tests spawn the real numkit_repl binary and verify protocol
// correctness end-to-end.
//
// Skip these if the binary is not found (CI without native build).

import { describe, it, expect, beforeAll, afterAll, afterEach } from 'vitest';
import { spawn } from 'node:child_process';
import path from 'node:path';
import fs from 'node:fs';
import { extractResponse } from './repl-protocol.js';

// ── Locate the binary ─────────────────────────────────────────────────────────

const REPO_ROOT = path.resolve(import.meta.dirname, '../..');

function findBinary() {
  const candidates = [
    path.join(REPO_ROOT, 'build/windows/release/apps/numkit/Release/numkit_repl.exe'),
    path.join(REPO_ROOT, 'build/windows/release/apps/numkit/numkit_repl.exe'),
    path.join(REPO_ROOT, 'build/linux/release/apps/numkit/numkit_repl'),
    path.join(REPO_ROOT, 'build/desktop-fast/apps/numkit/Release/numkit_repl.exe'),
    path.join(REPO_ROOT, 'build/desktop-fast/apps/numkit/numkit_repl'),
    path.join(REPO_ROOT, 'build/desktop/apps/numkit/Release/numkit_repl.exe'),
    path.join(REPO_ROOT, 'build/desktop/apps/numkit/numkit_repl'),
  ];
  return candidates.find(p => fs.existsSync(p)) ?? null;
}

const BINARY = findBinary();
const HAS_BINARY = BINARY !== null;

// ── Helper: one-shot protocol session ────────────────────────────────────────

function runSession(commands) {
  // Sends `commands` array to a fresh ide-session process.
  // Each command is a string (without trailing \n).
  // Returns a Promise<string[]> — one response chunk per command.
  return new Promise((resolve, reject) => {
    const proc = spawn(BINARY, ['--ide-session'], { stdio: ['pipe', 'pipe', 'pipe'] });
    const results = [];
    let buf = '';

    proc.stdout.on('data', d => {
      buf += d.toString();
      // Drain as many complete responses as possible.
      while (true) {
        const { result, remainder } = extractResponse(buf);
        if (!result) break;
        results.push(result);
        buf = remainder;
      }
    });

    let cmdIdx = 0;
    function sendNext() {
      if (cmdIdx >= commands.length) {
        proc.stdin.write('__QUIT__\n', 'utf8');
        return;
      }
      const cmd = commands[cmdIdx++];
      proc.stdin.write(cmd + '\n', 'utf8');
    }

    // Watch for complete responses and send the next command.
    const originalLength = results.length;
    const checkAndAdvance = setInterval(() => {
      if (results.length > originalLength + cmdIdx - 1) {
        sendNext();
      }
    }, 10);

    proc.on('close', () => {
      clearInterval(checkAndAdvance);
      resolve(results);
    });
    proc.on('error', reject);

    // Send first command immediately.
    sendNext();
  });
}

// Simpler helper: run all commands sequentially via promise chain.
async function runAll(commands) {
  return new Promise((resolve, reject) => {
    if (!HAS_BINARY) { resolve([]); return; }
    const proc = spawn(BINARY, ['--ide-session'], { stdio: ['pipe', 'pipe', 'pipe'] });
    const results = [];
    let buf = '';
    let pending = null;

    proc.stdout.on('data', d => {
      buf += d.toString();
      const { result, remainder } = extractResponse(buf);
      if (result && pending) {
        buf = remainder;
        const res = pending;
        pending = null;
        res.resolve(result);
      }
    });

    proc.on('error', reject);
    proc.on('close', () => resolve(results));

    async function execute() {
      for (const cmd of commands) {
        const r = await new Promise(res => {
          pending = { resolve: res };
          proc.stdin.write(cmd + '\n', 'utf8');
        });
        results.push(r);
      }
      proc.stdin.write('__QUIT__\n', 'utf8');
    }

    execute().catch(reject);
  });
}

// ── Tests ─────────────────────────────────────────────────────────────────────

describe.skipIf(!HAS_BINARY)('native REPL protocol — run', () => {
  it('executes code and returns stdout + vars', async () => {
    const results = await runAll([`x = 42;\ndisp(x)\n__END_OF_INPUT__`]);
    expect(results).toHaveLength(1);
    const r = results[0];
    expect(r.type).toBe('run');
    expect(r.stdout).toContain('42');
    expect(r.vars).toBeTruthy();
    expect(r.vars.x).toBeTruthy();
    expect(r.vars.x.type).toBe('double');
  });

  it('preserves workspace between runs', async () => {
    const results = await runAll([
      `x = 100;\n__END_OF_INPUT__`,
      `disp(x + 1)\n__END_OF_INPUT__`,
    ]);
    expect(results).toHaveLength(2);
    expect(results[1].stdout.trim()).toBe('101');
  });

  it('reports syntax errors without crashing', async () => {
    const results = await runAll([
      `x = ;\n__END_OF_INPUT__`,
      `x = 5;\n__END_OF_INPUT__`,
    ]);
    // First run should have an error in stderr (not crash)
    // Second run should still work (session persists).
    expect(results).toHaveLength(2);
    expect(results[1].type).toBe('run');
    expect(results[1].vars?.x).toBeTruthy();
  });
});

describe.skipIf(!HAS_BINARY)('native REPL protocol — __RESET__', () => {
  it('clears workspace and emits __RESET_OK__', async () => {
    const results = await runAll([
      `x = 7;\n__END_OF_INPUT__`,
      `__RESET__`,
      `__GET_SHAPE__:x`,
    ]);
    expect(results[1].type).toBe('reset');
    expect(results[1].ok).toBe(true);
    // After reset, x should not exist.
    expect(results[2].type).toBe('data');
    expect(results[2].data.error).toMatch(/not found/);
  });
});

describe.skipIf(!HAS_BINARY)('native REPL protocol — __GET_SHAPE__', () => {
  it('returns correct shape for a scalar', async () => {
    const results = await runAll([
      `s = 3.14;\n__END_OF_INPUT__`,
      `__GET_SHAPE__:s`,
    ]);
    const shape = results[1].data;
    expect(shape.type).toBe('double');
    expect(shape.rows).toBe(1);
    expect(shape.cols).toBe(1);
    expect(shape.numel).toBe(1);
    expect(shape.pages).toBe(1);
  });

  it('returns correct shape for a matrix', async () => {
    const results = await runAll([
      `M = zeros(3, 4);\n__END_OF_INPUT__`,
      `__GET_SHAPE__:M`,
    ]);
    const shape = results[1].data;
    expect(shape.rows).toBe(3);
    expect(shape.cols).toBe(4);
    expect(shape.numel).toBe(12);
  });

  it('returns error for unknown variable', async () => {
    const results = await runAll([`__GET_SHAPE__:notexist`]);
    expect(results[0].type).toBe('data');
    expect(results[0].data.error).toMatch(/not found/);
  });
});

describe.skipIf(!HAS_BINARY)('native REPL protocol — __INSPECT__', () => {
  it('returns full data for a small matrix', async () => {
    const results = await runAll([
      `A = [1 2; 3 4];\n__END_OF_INPUT__`,
      `__INSPECT__:A`,
    ]);
    const d = results[1].data;
    expect(d.type).toBe('double');
    expect(d.rows).toBe(2);
    expect(d.cols).toBe(2);
    // row-major: data[0][0]=1, data[0][1]=2, data[1][0]=3, data[1][1]=4
    expect(d.data[0][0]).toBe(1);
    expect(d.data[0][1]).toBe(2);
    expect(d.data[1][0]).toBe(3);
  });

  it('returns full data for a vector', async () => {
    const results = await runAll([
      `v = [10 20 30];\n__END_OF_INPUT__`,
      `__INSPECT__:v`,
    ]);
    const d = results[1].data;
    expect(d.rows).toBe(1);
    expect(d.cols).toBe(3);
    expect(d.data[0]).toEqual([10, 20, 30]);
  });
});

describe.skipIf(!HAS_BINARY)('native REPL protocol — __GET_STATS__', () => {
  it('returns min/max/mean for a numeric matrix', async () => {
    const results = await runAll([
      `M = [1 2 3; 4 5 6];\n__END_OF_INPUT__`,
      `__GET_STATS__:M\t-1`,
    ]);
    const s = results[1].data;
    expect(s.min).toBe(1);
    expect(s.max).toBe(6);
    expect(s.mean).toBeCloseTo(3.5, 5);
    expect(s.n).toBe(6);
    expect(s.hasNaN).toBe(false);
  });
});

describe.skipIf(!HAS_BINARY)('native REPL protocol — __GET_TILE__', () => {
  it('returns the requested tile', async () => {
    const results = await runAll([
      `B = reshape(1:9, 3, 3);\n__END_OF_INPUT__`,
      `__GET_TILE__:B\t0\t0\t2\t2\t0`,
    ]);
    const t = results[1].data;
    expect(t.r0).toBe(0);
    expect(t.c0).toBe(0);
    expect(t.rows).toBe(2);
    expect(t.cols).toBe(2);
    expect(t.data).toHaveLength(2);
    expect(t.data[0]).toHaveLength(2);
  });

  it('supports pipelined concurrent requests without losing any responses', async () => {
    const results = await runAll([
      `y = linspace(0, 10, 500000);\n__END_OF_INPUT__`,
      `__GET_SHAPE__:y`,
      `__GET_STATS__:y\t-1`,
      `__GET_TILE__:y\t0\t0\t64\t64\t0`,
      `__GET_TILE__:y\t0\t64\t64\t64\t0`,
    ]);
    expect(results).toHaveLength(5);
    expect(results[1].data.numel).toBe(500000);
    expect(results[2].data.n).toBe(500000);
    expect(results[3].data.r0).toBe(0);
    expect(results[3].data.c0).toBe(0);
    expect(results[4].data.r0).toBe(0);
    expect(results[4].data.c0).toBe(64);
  });

  it('builds AST via __BUILD_AST__ protocol command', async () => {
    const results = await runAll([
      `__BUILD_AST__\nx = 10 + 20;\n__END_OF_INPUT__`,
    ]);
    expect(results[0].data).toBeDefined();
    expect(results[0].data.type).toBeDefined();
  });

  it('builds script graph via __BUILD_GRAPH__ protocol command', async () => {
    const results = await runAll([
      `__BUILD_GRAPH__\nx = 10;\ny = x * 2;\n__END_OF_INPUT__`,
    ]);
    expect(results[0].data).toBeDefined();
    expect(results[0].data.nodes).toBeDefined();
    expect(results[0].data.edges).toBeDefined();
  });
});

describe.skipIf(!HAS_BINARY)('native REPL protocol — __INSPECT_PATH__', () => {
  it('inspects a struct at root (empty path)', async () => {
    const results = await runAll([
      `s.x = 5; s.name = 'hello';\n__END_OF_INPUT__`,
      `__INSPECT_PATH__:s\t`,
    ]);
    const d = results[1].data;
    expect(d.kind).toBe('struct');
    expect(d.fields).toContain('x');
    expect(d.fields).toContain('name');
  });

  it('drills into a struct field', async () => {
    const results = await runAll([
      `s.x = 42;\n__END_OF_INPUT__`,
      `__INSPECT_PATH__:s\tf:x`,
    ]);
    const d = results[1].data;
    // Field x=42 is a 1x1 double matrix
    expect(d.kind).toBe('matrix');
    expect(d.type).toBe('double');
    expect(d.data[0][0]).toBe(42);
  });
});

describe.skipIf(!HAS_BINARY)('native REPL protocol — __GET_PAGE__', () => {
  it('returns page 0 for a 2D matrix', async () => {
    const results = await runAll([
      `P = [1 2; 3 4];\n__END_OF_INPUT__`,
      `__GET_PAGE__:P\t0`,
    ]);
    const d = results[1].data;
    expect(d.type).toBe('double');
    expect(d.rows).toBe(2);
    expect(d.cols).toBe(2);
    expect(d.data[0]).toEqual([1, 2]);
  });
});

// ─── Debug sub-protocol integration tests ────────────────────────────────────
// These tests use the __DEBUG_START__: / __DEBUG_CMD__: / __END_OF_STEP__ protocol
// directly via the runSession helper.

// Helper: run a debug session and collect ALL responses (step + run markers).
function runDebugSession(bpLines, code, actions = []) {
  // actions is an array of 'continue'|'step_over'|'step_into'|'step_out'|'stop'
  return new Promise((resolve, reject) => {
    const proc = spawn(BINARY, ['--ide-session'], { stdio: ['pipe', 'pipe', 'pipe'] });
    const results = [];
    let buf = '';
    let done = false;

    function finish() {
      if (done) return;
      done = true;
      try { proc.kill(); } catch { /* ignore */ }
      resolve(results);
    }

    proc.stdout.on('data', d => {
      buf += d.toString();
      let changed = false;
      while (true) {
        const { result, remainder } = extractResponse(buf);
        if (!result) break;
        results.push(result);
        buf = remainder;
        changed = true;
      }
      if (!changed) return;

      // Send next pending action if we just got a dbstep
      let actionIdx = results.filter(r => r.type === 'dbstep').length - 1;
      if (actionIdx >= 0 && actionIdx < actions.length) {
        const last = results[results.length - 1];
        if (last?.type === 'dbstep') {
          proc.stdin.write(`__DEBUG_CMD__:${actions[actionIdx]}\n`, 'utf8');
        }
      }

      // Done when we see dbend or dbstop
      if (results.some(r => r.type === 'dbend' || r.type === 'dbstop')) {
        try { proc.stdin.write('__QUIT__\n', 'utf8'); } catch { /* ignore */ }
        // Give process a moment to flush then resolve
        setTimeout(finish, 200);
      }
    });

    proc.on('close', finish);
    proc.on('error', (err) => { done = true; reject(err); });

    const bpJson = JSON.stringify(bpLines);
    proc.stdin.write(`__DEBUG_START__:${bpJson}\n${code}\n__END_OF_INPUT__\n`, 'utf8');
  });
}

describe.skipIf(!HAS_BINARY)('native REPL protocol — debug', () => {
  // NOTE: DebugSession always pauses at the FIRST line on entry (reason='step'),
  // even if no breakpoints are set. You must send at least one 'continue' action
  // for the session to run to completion.

  it('completes after initial entry pause when no breakpoints set', async () => {
    // Entry → line 1 pause → continue → completion
    const results = await runDebugSession([], 'x = 42;\n', ['continue']);
    const stepResult = results.find(r => r.type === 'dbstep');
    expect(stepResult).toBeDefined();
    expect(stepResult.pauseState.line).toBe(1);
    expect(stepResult.pauseState.reason).toBe('step');

    const completion = results.find(r => r.type === 'dbend');
    expect(completion).toBeDefined();
    expect(completion.status).toBe('completed');
    expect(completion.vars).toHaveProperty('x');
  }, 15000);

  it('pauses at a breakpoint and resumes to completion', async () => {
    const code = 'a = 1;\nb = 2;\nc = a + b;\n';
    // Breakpoint on line 2: runs directly to line 2 (no initial entry step),
    // then continue → completion.
    const results = await runDebugSession([2], code, ['continue']);

    const steps = results.filter(r => r.type === 'dbstep');
    expect(steps.length).toBeGreaterThanOrEqual(1);

    // Must find a pause with reason='breakpoint' on line 2
    const bpStep = steps.find(s => s.pauseState?.reason === 'breakpoint');
    expect(bpStep).toBeDefined();
    expect(bpStep.pauseState.line).toBe(2);

    const completion = results.find(r => r.type === 'dbend');
    expect(completion).toBeDefined();
    expect(completion.status).toBe('completed');
    expect(completion.vars).toHaveProperty('c');
  }, 15000);


  it('stops when __DEBUG_CMD__:stop is sent after initial pause', async () => {
    const code = 'x = 10;\ny = 20;\n';
    // Entry pause at line 1, then immediately stop
    const results = await runDebugSession([], code, ['stop']);
    const stopResult = results.find(r => r.type === 'dbstop');
    expect(stopResult).toBeDefined();
    // No completion should follow a stop
    const completion = results.find(r => r.type === 'dbend');
    expect(completion).toBeUndefined();
  }, 15000);

  it('returns vars with correct value after completion', async () => {
    const results = await runDebugSession([], 'result = 99;\n', ['continue']);
    const completion = results.find(r => r.type === 'dbend');
    expect(completion).toBeDefined();
    expect(completion.vars?.result).toBeDefined();
  }, 15000);
});


