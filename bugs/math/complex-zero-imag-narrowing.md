# numkit keeps complex-with-zero-imaginary; MATLAB narrows it to real

- **Status:** 🔴 OPEN
- **Severity:** P2 (divergent result on a fundamental type predicate; niche but real)
- **Kind:** bug
- **Found:** 2026-06-17 via the max/min complex audit (edge probe of `max(real, 2+0i)`)

## Symptom
MATLAB R2025b **narrows** a complex value whose imaginary part is exactly zero
*and that arose from arithmetic* (e.g. the literal `2+0i`, or `(1+1i)+(1-1i)`)
back to a real double — so `isreal(2+0i)` is `1`. The explicit `complex(x,0)`
constructor is the exception: it FORCES complex storage (`isreal(complex(2,0))`
is `0`). numkit never narrows: every complex-typed value stays complex.

```matlab
isreal(2+0i)            % MATLAB: 1 (narrowed)   ; numkit: 0
isreal(complex(2,0))    % MATLAB: 0              ; numkit: 0   (agree)
isreal((1+1i)+(1-1i))   % MATLAB: 1              ; numkit: 0
```

Because numkit cannot distinguish `2+0i` (should be real) from `complex(2,0)`
(genuinely complex) — both are complex-typed with zero imaginary — it cannot
match MATLAB for operations whose result depends on that distinction.

## Downstream example (how it surfaced — max/min)
Binary `max`/`min` compare complex operands by `|z|`, real operands by value.
MATLAB picks the rule from `isreal` *after narrowing*; numkit always sees the
zero-imag operand as complex → uses `|z|`:

```matlab
max([1 -3 2], 2+0i)
% MATLAB: [2 2 2]   (2+0i -> real 2; el2 max(-3,2) by value = 2)
% numkit: [2 -3 2]  (2+0i stays complex; el2 max(-3,2+0i) by |z|: |-3|=3>2 = -3)

max(complex([1 -3 2]), 2)   % both engines: [2 -3 2]  (forced complex -> |z|; AGREE)
max([1 -3 2], 2+1i)         % both engines: [2+1i -3 2+1i]  (genuine complex; AGREE)
```

So the max/min complex comparator (bugs/math/maxmin-complex.md) is CORRECT for
numkit's complex model; the only divergence is the `2+0i`-style spelling, and it
is rooted here, not in max/min.

## Root cause
numkit's complex arithmetic / literal construction does not collapse a result
with an all-zero imaginary part to real. MATLAB does (for arithmetic results;
not for `complex()`).

## Suggested fix (deferred — deep, cross-cutting)
Narrow complex→real when the imaginary part is all-zero, at the points MATLAB
does: results of complex binary arithmetic and the `a+bi` literal folding, but
NOT the `complex()` builtin. This touches the core value model and many ops
(every complex arithmetic result would need an all-zero-imag check + narrow),
has a perf cost, and changes `isreal`/display/dispatch broadly — it needs its
own focused effort + full cross-engine parity, not a local patch. Affects:
`isreal`, display, `max`/`min`/`clamp`, and any |z|-vs-real dispatch.

## References
- Repro via `isreal(2+0i)`; surfaced in `max([1 -3 2], 2+0i)`.
- Related: bugs/math/maxmin-complex.md (the comparator is correct given this gap).
- `tests/builtin/maxmin_complex_test.cpp` MixedTypeEdges documents the boundary.
