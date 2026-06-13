# fskmod/fskdemod — round-trip throws on TreeWalker, works on VM

- **Status:** 🔴 OPEN
- **Severity:** P3 (backend divergence on one engine; VM — the default backend — is correct)
- **Kind:** bug
- **Found:** 2026-06-13 via DualEngineTest while adding comm coverage (CommModulationTest)

## Symptom

The `fskmod` → `fskdemod` round-trip throws `Cannot convert double to scalar`
under the **TreeWalker** backend. The identical script runs correctly under the
**VM** backend (default), returning the input symbols. Other modulators in the
same suite (pskmod/pskdemod, dpskmod/dpskdemod, ofdmmod/ofdmdemod) round-trip
fine on **both** backends, so the divergence is specific to the fsk path.

## Repro

```matlab
data = [0 1 2 3 0 2 1 3];
y   = fskmod(data, 4, 100, 10, 2000);       % 80x1 complex — OK on both backends
out = fskdemod(y, 4, 100, 10, 2000);        % VM: [0 1 2 3 0 2 1 3]; TW: THROWS
```

- VM (default): `sum(abs(out(:) - data(:))) == 0`.
- TreeWalker: throws `Cannot convert double to scalar` (somewhere inside the
  fsk path — `fskmod` output is produced fine; the throw is on the
  `fskdemod` leg).

## Root cause

Not yet diagnosed. `fskmod`/`fskdemod` are engine-free compute functions
(`toolboxes/comm/src/modulation/fsk_ofdm.cpp`) reached through a `_reg`
adapter, so a TW-vs-VM divergence points at how the script-level call is
marshalled/dispatched on the TreeWalker for this particular signature (a
`toScalar()` is being applied to a non-scalar only on the TW path). Same class
of issue as other TW/VM dispatch-order divergences.

## Suggested fix

Trace the `fskdemod` call on the TreeWalker (the `toScalar` site that fires on
TW but not VM) — likely an argument-marshalling / output-shape difference in
the reg adapter or the TW call path. fskmod's output is fine, so focus on the
demod leg.

## References

- Live (disabled) guard: `CommModulationTest.DISABLED_FskRoundTrip` in
  `toolboxes/comm/tests/comm_modulation_test.cpp` — asserts the round-trip; runs
  under `--gtest_also_run_disabled_tests` and fails on the `/TW` param.
- Active coverage of `fskmod` length is `CommModulationTest.FskmodOutputLength`.
- Parity specs `tools/parity/specs/fskmod.json`, `fskdemod.json` (validated via
  the default/VM backend).
