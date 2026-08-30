# core.import — chained namespace aliases don't resolve (`import t1.sub as t2` where t1 is itself an alias)

- **Status:** 🔴 OPEN
- **Severity:** P3 minor (numkit's own import-aliasing extension; not a MATLAB-parity surface — MATLAB has no `import … as`)
- **Kind:** bug
- **Found:** 2026-08-30 via the orphaned-DISABLED_-test audit (guard existed, was never enabled, fails when run)

## Symptom

Import aliasing supports one level (`import ns as a`), but chaining through an
existing alias fails to resolve at use time.

## Repro

```matlab
clear;
% (test_ns.sub.deep_answer registered by the engine-side fixture — see the guard)
import test_ns as t1;
import t1.sub as t2;
y = t2.deep_answer();
% numkit: resolution error (t2 does not resolve through the t1 alias)
% expected: 42 (alias chain resolves transitively)
```

## Root cause (suspected)

The alias table stores aliases resolved at registration; `t1.sub` is not
re-resolved through the existing alias when `t2` is registered, so the chain
never links. A transitive-resolution pass at registration (or at lookup)
closes it.

## Suggested fix

Resolve alias targets through the alias table transitively (cycle-guarded);
add the chain case next to the existing alias tests in
`src/core/tests/namespace_resolver_test.cpp` and enable
`DISABLED_AliasChainTransitive`.

## References
- **Guard:** `DISABLED_AliasChainTransitive`

Guard: `src/core/tests/namespace_resolver_test.cpp` (kept DISABLED_ — it is
the reproducer). Neighbour `DISABLED_ClosureCapturesFunctionLocalImport`
stays DISABLED deliberately: its own comment documents it as a
would-be-nice beyond MATLAB semantics, not a defect.
