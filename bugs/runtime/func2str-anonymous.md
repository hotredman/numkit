# runtime.func2str — anonymous handle returns the internal name, not the source

- **Status:** 🔴 OPEN
- **Severity:** P2 (wrong output for anonymous handles)
- **Kind:** bug
- **Found:** 2026-06-04 via DEEP-PROBE (function-handle sweep)

## Symptom
`func2str` on an anonymous handle returns the internal placeholder name
`@__anon_<N>` instead of reconstructing the function source text. Named
handles are correct.

## Repro
```matlab
func2str(@(x) x + 1)
% numkit: '@__anon_0'
% MATLAB: '@(x)x+1'

func2str(@(a,b) a.*b + 1)
% numkit: '@__anon_1'
% MATLAB: '@(a,b)a.*b+1'

func2str(@sin)      % named handle — numkit == MATLAB == 'sin'
```

## Root cause
numkit does not retain the source text (or the AST) of an anonymous function;
the parser assigns each lambda an internal `__anon_<N>` name, and `func2str`
falls back to that name with an `@` prefix (see the comment in
`src/runtime/src/function_handles.cpp` func2str + BUGS.md #16).

## Suggested fix
Store the anonymous function's source text (or re-serialise its AST body and
parameter list) at parse time, and have `func2str` emit that for anon handles.
Pretty-printing the AST to match MATLAB's exact spacing (e.g. `@(x)x+1`, no
spaces) is the fiddly part. Medium — needs an AST→source serialiser or to
capture the original token span. Note: `str2func('@(x)x+1')` now round-trips
through the parser (fixed 2026-06-04), so a stored source string would make
`str2func(func2str(h))` work for anon handles too.

## References
- `src/runtime/src/function_handles.cpp` (func2str lambda)
- the parser's anonymous-function handling (`__anon_<N>` naming)
- MATLAB `doc func2str`
