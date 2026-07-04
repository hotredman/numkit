# numkit — engineering TODO / tech-debt

Tracked, non-urgent improvements that are deliberately out of scope for the
change that surfaced them. Each entry is self-contained: a session should be
able to act on it cold. Behaviour-changing items must be done under user
supervision (not folded into mechanical refactors).

Format per entry: **Title** · *Kind* (tech-debt / perf / cleanup / design) ·
*Status* (open / in-progress / done) — then Problem / Why-deferred / Fix /
Affected.

---

## 1. Replace the process-global shared RNG + mutex with a per-`Engine` stream
*Kind:* design · *Status:* open · *Surfaced:* 2026-06 layering review (binornd)

**Problem.** All random builtins draw from a single **process-global** generator
`numkit::builtin::sharedEngine()`, serialised by `numkit::builtin::rngMutex()`.
Typical shape (e.g. `binornd`, `normrnd`, `rand`, `randn`, `randi`, `randperm`,
`datasample`, `bootstrp`, channel/fading noise in comm, …):

```cpp
auto &gen = ::numkit::builtin::sharedEngine();
auto &mtx = ::numkit::builtin::rngMutex();
...
std::lock_guard<std::mutex> lk(mtx);
for (size_t i = 0; i < cnt; ++i) od[i] = dist(gen);
```

The mutex is *correct* (it stops a data race on the shared generator state when
the C++ API is called from embedder threads / `ops::parallel_for` / multiple
`Engine` instances — `dist(gen)` mutates `gen`). It is **not a hot-path cost**:
one uncontended lock per *call*, not per element, and the runtime VM/eval is
single-threaded so it is essentially never contended.

**Why it is debt, not a bug.** A single global generator + mutex is an
anti-pattern for a library:
- Two `Engine` instances (or threads) share one stream, so `rng(seed)` in one
  leaks into the other; reproducibility is not isolated per engine.
- Under real concurrency the interleaving order of draws across threads is
  nondeterministic → results are not reproducible even with a fixed seed.
- Global mutable state complicates the layering goal (RNG lives in `ops`, but a
  process-global singleton is awkward to reason about per-layer).

**Fix (behaviour-changing — do under user supervision).** Give each `Engine`
its own RNG stream (state owned by the engine / threaded through `CallContext`),
drop the global singleton and the mutex. The public C++ `*rnd` API would take
the RNG state (or `mr` already carries an arena — add an rng handle) instead of
reaching for `sharedEngine()`. Keep MATLAB `rng(seed)` semantics per engine.
This changes seeding/reproducibility semantics, so it needs explicit sign-off +
parity re-validation, NOT a mechanical pass.

**Affected.** `numkit::builtin::sharedEngine` / `rngMutex` (toolboxes/builtin RNG
infra, relocated to `ops/rng.{hpp,cpp}` in the layering refactor) and every
`*rnd` / sampling / noise builtin across toolboxes/stats, toolboxes/builtin, toolboxes/comm
(`grep -rl "sharedEngine()" libs`). Original authorship: pre-layering
(commit 67a702c3d, 2026-05-03) — not introduced by the compute/register split.
