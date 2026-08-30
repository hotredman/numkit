# lang.run — running a nullary function FILE must invoke it (MATLAB `run` semantics); numkit defines it silently

- **Status:** ✅ FIXED (2026-08-30)
- **Severity:** P1 wrong result (silent no-op: empty output, exit 0)
- **Kind:** bug
- **Found:** 2026-08-30 via fieldtest (DeepLearnToolbox `Softmax_Test.m`,
  `Fft2_Test.m`, `ComputeWhiteningTransformation_Test.m`,
  `BlobToVector_Test.m` — batch 20260830-003212)

## Symptom

`numkit file.m` where file.m is `function Name … end` with NO required
inputs: numkit defines the function and exits silently (exit 0, empty
stdout). MATLAB's `run('file.m')` **invokes** the nullary function — the
test suites print `Test Passed`.

## Repro

```matlab
% hello_ft.m:
%   function hello_ft
%   disp(42)
%   end
numkit hello_ft.m     % numkit: (no output, exit 0)
matlab -batch "run('hello_ft.m')"   % MATLAB: 42
```

Real-world damage: every xUnit-style test file in DeepLearnToolbox (the
`*_Test.m` family) silently "passes" without running — the worst failure
class, invisible without differential testing.

## Root cause

The CLI reads the file and evaluates it as a script; a top-level
FUNCTION_DEF is legal (script-local functions) so nothing errors — but
MATLAB's run() detects a function file with no required inputs and calls
it by name.

## Suggested fix

In the file-run path (CLI + `run` builtin): after parsing, if the program
is a single nullary FUNCTION_DEF (no required parameters), invoke it after
definition. (MATLAB errors for function files WITH required args — numkit
should match that too rather than silently doing nothing.)

## References
- **Guard:** `FileRunInvokesNullaryFunction` (live, dual-engine)

fieldtest batch `reports/20260830-003212.json`; test deferred (needs the
file-run path — CLI-level; tracked here, engine-level gtest to follow with
the mfile-resolver fixture).

## Resolution (2026-08-30)

One semantic, three call sites, one shared helper:
`numkit::runtime::primaryFunctionOfFile(src)` (header-only,
runtime/function_file.hpp) detects a function file and extracts the
PRIMARY function's name; the `run` builtin (runtime/eval.cpp), the native
CLI runScript (apps/numkit/main.cpp) and the WASM CLI (cli.js — JS mirror
of the same scan) now EXECUTE that function after the file evaluates.
Required-inputs function files are also invoked and fail with the natural
argument error — exactly MATLAB's class (corpus-verified:
extract_firms_data.m errors "Not enough input arguments" in MATLAB).

Verified: run('hello_ft.m') -> 42; CLI file arg -> 42/exit 0; required-
args file -> exit 1 both CLIs; scripts unaffected; live dual-engine guard
green; full Release suite exit 0.
