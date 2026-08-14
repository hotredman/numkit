// @vitest-environment jsdom
//
// Unit test for the WASM engine wrapper's session init — specifically that
// it configures MATLAB compatibility mode.

import { describe, it, expect, vi } from 'vitest';
import { createWasmEngine } from './engine';

function mockModule(overrides = {}) {
  return {
    repl_init: vi.fn(() => 'Numkit IDE — ready'),
    repl_execute: vi.fn(() => ''),
    repl_set_compat_mode: vi.fn(),
    ...overrides,
  };
}

describe('createWasmEngine — session init', () => {
  it('enables MATLAB compat mode on init and returns greeting', async () => {
    const mod = mockModule();
    const eng = await createWasmEngine(async () => mod);
    const greeting = eng.init();
    expect(greeting).toBe('Numkit IDE — ready');
    expect(mod.repl_init).toHaveBeenCalledTimes(1);
    expect(mod.repl_set_compat_mode).toHaveBeenCalled();
  });

  it('still returns greeting if compat mode init throws', async () => {
    const mod = mockModule({
      repl_set_compat_mode: vi.fn(() => { throw new Error('boom'); }),
    });
    const eng = await createWasmEngine(async () => mod);
    expect(() => eng.init()).not.toThrow();
    expect(eng.init()).toBe('Numkit IDE — ready');
  });
});

