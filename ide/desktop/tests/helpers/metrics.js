// helpers/metrics.js — read per-process memory from Electron's main
// process. The renderer's `performance.memory` is V8 JS heap only;
// to catch leaks in WASM linear memory, typed arrays, GPU buffers,
// or IPC buffers we have to ask the OS via app.getAppMetrics(),
// which returns workingSetSize + privateBytes per Electron process.
//
// Tests use these to assert "Tab process stays bounded under N MB".
// Numbers are in MB throughout (the API gives KB).

/** Snapshot all Electron processes' memory + type. */
export async function allMetrics(app) {
  return await app.evaluate(async ({ app }) => app.getAppMetrics());
}

/** Just the renderer (Tab) process — the one we worry about leaking. */
export async function tabMemory(app) {
  const metrics = await allMetrics(app);
  const tab = metrics.find((m) => m.type === 'Tab');
  if (!tab) throw new Error('No Tab process found in metrics');
  return {
    pid:        tab.pid,
    workingSet: Math.round(tab.memory.workingSetSize / 1024),  // MB
    private:    Math.round(tab.memory.privateBytes    / 1024),  // MB
  };
}

/** Compact one-line summary across processes — handy for fixture logs. */
export async function describeMetrics(app) {
  const metrics = await allMetrics(app);
  return metrics
    .map((m) => `${m.type || 'unknown'}#${m.pid} ws=${Math.round(m.memory.workingSetSize / 1024)}MB`)
    .join(' | ');
}
