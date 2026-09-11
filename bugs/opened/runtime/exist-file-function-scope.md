# runtime.exist — `exist(absolutePath, 'file')` returns 0 inside a FUNCTION (works at top level)

- **Status:** 🔴 OPEN
- **Severity:** P3 (file-presence probes in function/library code silently
  mis-report; found via the portion-32 export parity spec — saveas WORKS
  but exist() in the spec's local function said 0)
- **Kind:** bug
- **Found:** 2026-09-11 portion 32 — figure_export parity ran the spec in
  an isolated local function; exist=0 while the file was on disk.

## Symptom

`exist(<absolute windows path>, 'file')` → 2 at the script top level, but
0 inside a function. `dir(<same path>)` finds the file in BOTH scopes, and
saveas/imwrite write the file fine — only exist's file branch is affected.

## Repro (self-contained)

```matlab
clear;
function q()
  p = [tempdir 'probe_exist.txt'];
  fid = fopen(p, 'w'); fwrite(fid, uint8('x')); fclose(fid);
  fprintf('fn exist=%g (MATLAB: 2)\n', exist(p, 'file'));
  fprintf('fn dir=%g (MATLAB: 1)\n', numel(dir(p)));
end
q();
fprintf('top exist=%g\n', exist([tempdir 'probe_exist.txt'], 'file'));
% numkit:   fn exist=0, fn dir=1, top exist=2
% MATLAB R2025b: fn exist=2, fn dir=1, top exist=2
```

## Root cause (suspected)

exist's `vfsExists` swallows every exception (`catch (...) return false`).
resolvePath or the native fs `exists()` throws in the function context
(something scope-dependent — script-origin/cwd resolution — needs a
debug print to pin down); the catch hides it and exist reports "no file".

## Suggested fix

Find the throwing call (log it), fix the scope dependence, and narrow the
swallow to expected "not found" misses only.

## References

- **Guard:** `DISABLED_ExistFileInsideFunction` (dual-engine) in
  `src/bundle/tests/known_bugs_test.cpp`.
