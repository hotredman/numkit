# lang.run — `run('<absolute path>')` inside a script loses sibling-function resolution through the CLI native-FS bridge

- **Status:** 🔴 OPEN
- **Severity:** P2 (works in MATLAB and in-engine; breaks through the CLI)
- **Kind:** bug
- **Found:** 2026-08-31 via the fieldtest mat-comparison harness (wrapper
  scripts run real corpus code by absolute path)

## Symptom

A script executed via `run('<absolute path>')` from another wrapper script
cannot resolve sibling FUNCTION files, when both go through the WASM CLI's
native-FS bridge. The same call with a RELATIVE `run('main.m')` works.
MATLAB R2025b runs both forms; `StandardEngine::eval("run('<abs>')")` in
gtest also resolves the sibling (mfile_resolver_test is green) — the loss
is specific to the CLI bridge path.

## Repro (self-contained)

```bash
mkdir dirA && cd dirA
cat > helper.m <<'EOF'
function z = helper(x)
z = 2 * x;
end
EOF
cat > main.m <<'EOF'
disp(helper(21));
EOF
printf "run('main.m');\n"                      > wrap_rel.m
printf "run('/abs/path/to/dirA/main.m');\n"    > wrap_abs.m

node cli.js wrap_rel.m   # numkit: 42      (correct — matches MATLAB)
node cli.js wrap_abs.m   # numkit: Error (line 1): VM: undefined function 'helper'
                          # MATLAB R2025b: 42 for BOTH forms
```

## Root cause (hypothesis)

cli.js pushes the FILE argument's origin as `("native", <dir>)`. Inside the
engine, `run()` of a target by ABSOLUTE path registers the target's
script-dir for sibling lookup on the wrong filesystem (the default
in-memory VFS instead of the pushed "native" bridge), so `helper.m` is
never found on disk. With a relative target the run-resolution goes
through the native bridge and the sibling lookup follows.

## Suggested fix

Make the script-dir push inside `run()` inherit the origin filesystem of
the RUNNING script (the wrapper's "native"), not the default VFS — or make
resolveMFile_'s searchDirs consult all registered filesystems for the
dir-existence check.

## References

- **Guard:** deferred to a DISABLED_-grade gtest (the bug does not
  reproduce in-engine — only through the CLI native-FS bridge). Interim
  guard: `packages/numkit/test/cli_sibling_test.js`, deliberately RED
  while open (exit 1, prints this file); turns green = live regression
  guard the moment the fix lands.
