// @vitest-environment jsdom
//
// Unit test for the WASM engine wrapper's session init — specifically that
// it auto-imports the compat namespace once, so the user never has to type
// `import compat.*` (imports persist on the workspace env for the session).

import { describe, it, expect, vi } from 'vitest';
import { createWasmEngine } from './engine';

function mockModule(overrides = {}) {
  return {
    repl_init: vi.fn(() => 'Numkit IDE — ready'),
    repl_execute: vi.fn(() => ''),
    ...overrides,
  };
}

describe('createWasmEngine — session init', () => {
  it('auto-imports compat.* once and returns the greeting', async () => {
    const mod = mockModule();
    const eng = await createWasmEngine(async () => mod);
    const greeting = eng.init();
    expect(greeting).toBe('Numkit IDE — ready');
    expect(mod.repl_init).toHaveBeenCalledTimes(1);
    expect(mod.repl_execute).toHaveBeenCalledWith('import compat.*;');
  });

  it('still returns the greeting if the auto-import throws', async () => {
    const mod = mockModule({
      repl_execute: vi.fn(() => { throw new Error('boom'); }),
    });
    const eng = await createWasmEngine(async () => mod);
    expect(() => eng.init()).not.toThrow();
    expect(eng.init()).toBe('Numkit IDE — ready');
  });
});
