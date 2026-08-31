# lang.handles — `h = @name` for a sibling FILE function fails to resolve ("undefined function in handle '@name'") while the direct call works

- **Status:** 🔴 OPEN
- **Severity:** P2 (the standard callback pattern `h = @helper; h(...)` dies)
- **Kind:** bug
- **Found:** 2026-08-31 while covering the varargin surface (portion-5 follow-ups)

## Symptom

Creating a function handle to a file-defined function in the same
directory fails at CALL time, though calling the function directly by
name works. Same family as `lang/run-abs-path-sibling-resolution.md`
(direct-call resolution works, other resolution paths miss the file).

## Repro (self-contained)

```matlab
% dirA/vh.m:
%   function n = vh(a, varargin)
%   n = a + sum([varargin{:}]);
%   end
% dirA/main.m:
disp(vh(2, 3, 4))       % numkit: 9   (works)
h = @vh;
disp(h(2, 3, 4))        % numkit: Error: VM: undefined function in handle '@vh'
                         % MATLAB R2025b: 9 for BOTH
```

## Root cause (hypothesis)

Handle creation resolves `@name` through the workspace/import path only,
without the script-origin directory search that direct calls use.

## Suggested fix

Route handle-target resolution through the same resolveMFile_ pass as
direct calls (script-origin dirs included).

## References

- **Guard:** deferred — needs the same investigation as
  run-abs-path-sibling-resolution (shared root); a combined DISABLED_
  guard covering both call and handle resolution lands with that fix.
