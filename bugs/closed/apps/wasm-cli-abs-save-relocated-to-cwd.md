# apps.cli — `numkit /abs/path/script.m`: absolute-path `save` inside the script's own dir silently relocates the file to the process cwd

- **Status:** ✅ FIXED (44d356e92, 2026-08-31)
- **Severity:** P2 (silent wrong-location write; a same-named file in cwd gets clobbered)
- **Kind:** bug
- **Found:** 2026-08-31 while building the fieldtest .mat-comparison harness (R4)

## Symptom

Running a script by ABSOLUTE path, from a cwd different from the script's
directory, a `save('<absolute target>')` whose target lies INSIDE the
script's directory writes the file to the **process cwd** instead —
silently, exit 0. Targets outside the script dir are unaffected.

## Repro

```bash
mkdir -p /d/proj/sub                        # any dir NOT under the cwd
cat > /d/proj/sub/s.m <<'EOF'
save('/d/proj/sub/out.mat');                % absolute target inside scriptDir
EOF
cd /somewhere/else && node cli.js /d/proj/sub/s.m   # script by ABSOLUTE path
ls /d/proj/sub/out.mat   # numkit: MISSING
ls ./out.mat             # numkit: HERE — the file silently landed in cwd
```

Observed on numkit 0.1.0 (engine dist as of 2026-08-31): the workspace
`out.mat` appears at `<cwd>/out.mat`; MATLAB R2025b writes it at the
absolute path given, regardless of cwd.

## Root cause

`packages/numkit/bin/cli.js`, `fsResolve()` — the doubled-scriptDir-prefix
stripping loop. When the engine prefixes an absolute Windows/POSIX path
with the script-origin dir, the loop strips the prefix:

```js
if (/^([A-Za-z]:\/|\/)/.test(rest)) { p = rest; continue; } // doubled prefix
```

`continue` re-tests the `while (p.startsWith(norm))` condition. When the
restored absolute target itself lies under `norm` (the script's dir), the
loop strips a SECOND time, leaving the bare leaf (`out.mat`), which then
resolves against the process cwd.

## Suggested fix

The restored remainder after a doubled-prefix strip is already absolute —
return it instead of `continue`-ing:

```js
if (/^([A-Za-z]:\/|\/)/.test(rest)) return rest; // doubled prefix, now absolute
```

## References

- **Guard:** `packages/numkit/test/cli_fs_test.js` (live node test: script
  in dir A, abs-path save inside A, CLI invoked from dir B; asserts the file
  lands in A, not B).
