# io.load/save — MAT bytes crossed the JS↔WASM boundary on the text channel

- **Status:** ✅ FIXED (uncommitted working tree, 2026-08-29)
- **Severity:** P1 wrong result
- **Found:** 2026-08-29 via npm CLI corpus run (examples/IO through packages/numkit, WASM engine)

## Symptom
Under WASM with a callback filesystem (IDE local folder, npm CLI native-FS
bridge), `save` wrote MAT bytes through the **text** `writeFile` and `load`
read them through the **text** `readFile`. Bytes ≥ 0x80 were UTF-8-mangled
crossing the JS-string boundary → corrupted files ("corrupted MAT element
length") or silently wrong values on load (e.g. `pi` → `-0.979036`).

## Repro
```matlab
x = [1.5; pi]; save('t.mat', 'x'); clear x; s = load('t.mat');
% numkit (WASM, before fix):  s.x(2) = -0.979036
% numkit (native):            s.x(2) = 3.141593   (text path is byte-accurate natively — why it went unnoticed)
```

## Fix
`src/runtime/src/saveload_mat.cpp` — all four file-channel calls switched to
the binary channel that already existed for imread/audioread:
`writeFile → writeFileBytes` (saveMat4, saveMat), `readFile → readFileBytes`
(loadMat4, loadMat). Native backends are unaffected (VirtualFS defaults
delegate to the text methods, which are byte-accurate there).

## Regression guard
`packages/numkit` corpus: IO category 13/13 after the fix (was 9/13), incl.
`mat_roundtrip.m`, `mat_multiple_vars.m` and a uint8 `[200 255 0 128]`
roundtrip probe.
