# runtime.func2str — anonymous handle returns the internal name, not the source

- **Status:** ✅ FIXED (2026-06-18) — parser reconstructs anon source onto the handle
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

## Fix (2026-06-18)
The parser reconstructs the anonymous-function source from its token span and
stores it on the handle; `func2str` returns it.

- `Parser::reconstructAnonSource` (parser.cpp) walks the `@(...)…` token span and
  concatenates token text with NO inter-token whitespace, re-quoting char/string
  literals (internal quotes doubled). This reproduces MATLAB's normalization
  exactly — `@(x) x + 1` → `@(x)x+1`, while a literal like `' world'` keeps its
  interior space (the lexer already stores each token's lexeme in `.value`).
  The text is stashed in the ANON_FUNC node's `strValue` (the `@funcName`
  discriminator still holds — an anon node always has a body child).
- `HeapObject` gains a `funcSource` field (alongside `funcName`; cloned + freed
  with it); `Value::funcHandleSource()` / `setFuncHandleSource()` access it.
  Both engines (`execAnonFunc` / `compileAnonFunc`) set it when minting the
  handle. `func2str` returns it for anon handles (falling back to `@__anon_N`).

`func2str(@(x) x + 1)` → `'@(x)x+1'`; `str2func(func2str(h))` round-trips.
Parity OK vs MATLAB R2025b (`tools/parity/specs/func2str_anonymous.json`, exact
string compare). Guards: `src/bundle/tests/builtins_test.cpp` (`Func2Str`, both
engines) + `src/bundle/tests/known_bugs_test.cpp` (`Func2StrAnonymous`, promoted
live).

**Known limitation:** a VM closure that *captures* variables (`@(x) x + a`) is
represented as a cell `{handle, caps}`, on which `func2str` still throws (it was
never a function handle there — a pre-existing VM-representation gap, separate
from this fix). TreeWalker captures via a closure env and returns a plain handle,
so `func2str` works there. Capture-free anon handles work on both engines.

## References
- `src/core/src/parser.cpp` (`reconstructAnonSource`, `parseAnonFunc`),
  `src/value/include/numkit/value/heap_object.hpp` + `src/value/src/{heap_object,value}.cpp`
  (`funcSource`), `src/core/src/{tree_walker,compiler}.cpp` (`execAnonFunc` /
  `compileAnonFunc`), `src/runtime/src/function_handles.cpp` (func2str).
- Spec `tools/parity/specs/func2str_anonymous.json`; smoke
  `src/lang/tests/smoke/func2str_anonymous_smoke.m`.
- MATLAB `doc func2str`.
