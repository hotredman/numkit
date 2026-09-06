# Builtin m-source registrations must survive `clear all` — the replay mechanism

**Problem/context.** Engine-startup `.m`-implemented builtins (the
`registerBuiltinMSource` family: inline, impulse(b,a,t), ode45/ode23,
quadprog, fmincon, fminunc, fsolve, linprog, funm, decomposition,
integral, …) and the `inline` classdef lived in the same `userFuncs_` /
`classDefs_` buckets that `clear all` / `clear classes` / `clear
functions` wipe. In MATLAB those clears unload compiled code but reload
it from disk on the next call; numkit's startup registrations have no
disk backing, so after ANY script beginning with `clear all` (every
textbook example does) all of them became undefined or mis-dispatched
(impulse fell through to the control LTI overload). Found 2026-09-06 via
springer-math pr1_1–pr1_3/pr2_4, which "regressed" the day after the
functions were implemented — the fixes were real, the functions just
died at line 1 of every script.

**Solution.** `Engine::reinstallBuiltinSources()` mirrors
`reinstallConstants()`: `registerBuiltinMSource(src)` retains the source
text; a new `registerBuiltinClassSource(src)` parses the CLASSDEF_DEF
and calls `registerClassDef` directly (the identical call the compiler
makes for a classdef statement — no eval/VM run needed) and retains the
text. The `clear` external in `src/runtime/src/workspace.cpp` calls the
replay right after its wipe in the `all`/`classes` and `functions`
branches. Replay is parser+registry work only, safe mid-chunk.

**Rationale.** Same pattern the engine already uses for constants after
`clear all`; retaining parsed artifacts instead of texts would couple
replay to UserFunction copy semantics, texts are the simplest immutable
source of truth. File-backed user functions/classes keep their existing
reload-on-reference behavior — untouched.

**Rule going forward:** anything registered as an m-source/classdef
builtin at startup MUST go through `registerBuiltinMSource` /
`registerBuiltinClassSource` (NOT bare `engine.evalSafe`) or it dies
with the first `clear all`. Guards: `Backends/ClearSemanticsTest*`,
`ClearSemanticsVMTest` in `src/bundle/tests/clear_semantics_test.cpp`
(dual-engine; the TW inline-ctor failure those tests surfaced is a
separate open bug: core/treewalker-inline-classdef-ctor).
