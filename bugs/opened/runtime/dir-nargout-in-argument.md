# runtime.dir — `numel(dir(path))` inside a CALL ARGUMENT returns 0 (dir runs with nargout=0 and prints its listing)

- **Status:** 🟡 OPEN
- **Severity:** P3 (probes written the natural MATLAB way silently get 0;
  found while writing the portion-35 imagesc export smoke)
- **Kind:** bug
- **Found:** 2026-09-11 portion 35 — smoke did `fprintf('%d',
  numel(dir([tempdir 'x.png'])))` and got 0 although the file existed;
  assigning first (`n = numel(dir(...))`) returns 1.

## Symptom

Any external whose behavior depends on nargout (dir prints a listing at
nargout=0, returns a struct at nargout≥1) misbehaves when the call sits
inside another call's ARGUMENT list: it is invoked with nargout=0, so it
prints and returns nothing.

## Repro (self-contained)

```matlab
clear;
figure; plot(1:3);
saveas(gcf, [tempdir 'probe_dir.png']);
a = numel(dir([tempdir 'probe_dir.png']));       % assignment context
fprintf('assigned = %d\n', a);                   % numkit: 1  ✓
fprintf('nested = %d\n', numel(dir([tempdir 'probe_dir.png'])));
% numkit: prints the dir listing, then "nested = 0"
% MATLAB R2025b: nested = 1 (argument context requests an output)
```

## Root cause (preliminary)

The evaluator resolves call arguments in a context that passes nargout=0
down to the callee instead of requesting one output for value use.
Affects every nargout-sensitive builtin in argument position; assignment
and direct-statement positions are correct.

## References

- **Guard:** `DISABLED_DirNargoutInsideCallArgument`
  (TW_VM/CommandGlueTest) in `src/bundle/tests/known_bugs_test.cpp`.
