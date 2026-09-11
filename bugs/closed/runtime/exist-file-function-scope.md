# runtime.exist — `exist(absolutePath, 'file')` returns 0 inside a FUNCTION (works at top level)

- **Status:** ✅ FIXED (portion 33, commit backfilled below)
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

## Root cause (confirmed by instrumentation)

NOT the catch(...) (it never fired). Inside functions the compiler rewrites
`exist(name[, filter])` into the EXIST_VAR opcode (compiler.cpp "workspace
introspection builtins → opcodes inside functions"), and that opcode's
'file'/'dir'/'class' branches were a stub that printed a warning and
returned 0. The top level calls the registered exist() external (correct),
so the divergence looked scope-dependent.

## Fix (portion 33)

EXIST_VAR's 'file'/'dir'/'class' branches now delegate to the registered
exist() external (one true implementation: resolvePath + fs checks); the
no-filter form also falls through to it when the local var/function probes
find nothing, so MATLAB's file=2 / m-file=2 answers work inside functions
too. The 'var'/'builtin' fast paths stay local.

## References

- **Guard:** `ExistFileInsideFunction` (dual-engine, enabled) in
  `src/bundle/tests/known_bugs_test.cpp`.
