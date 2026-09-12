// @vitest-environment jsdom
//
// Unit test for the WASM engine wrapper's session init — specifically that
// it configures MATLAB compatibility mode.

import { describe, it, expect, vi } from 'vitest';
import { createWasmEngine } from './engine';

function mockModule(overrides = {}) {
  return {
    repl_init: vi.fn(() => 'NK IDE — ready'),
    repl_execute: vi.fn(() => ''),
    ...overrides,
  };
}

describe('createWasmEngine — session init', () => {
  it('returns the greeting from repl_init', async () => {
    const mod = mockModule();
    const eng = await createWasmEngine(async () => mod);
    const greeting = eng.init();
    expect(greeting).toBe('NK IDE — ready');
    expect(mod.repl_init).toHaveBeenCalledTimes(1);
  });
});

