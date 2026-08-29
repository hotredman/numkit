# lang.run — running a nullary function FILE must invoke it (MATLAB `run` semantics); numkit defines it silently

- **Status:** 🔴 OPEN
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

fieldtest batch `reports/20260830-003212.json`; test deferred (needs the
file-run path — CLI-level; tracked here, engine-level gtest to follow with
the mfile-resolver fixture).
