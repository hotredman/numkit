# SLEEF — math-kernel provenance (Boost Software License 1.0)

numkit does **not** link or vendor SLEEF. A few math kernels were **ported**
(transcribed into Google Highway operations) from SLEEF's double-precision
SIMD library:

- source: https://github.com/shibatch/sleef , commit `7623d6c`
- files: `src/libm/sleefsimddp.c` (`xerf_u1`; `sinpik`/`cospik` octant
  reduction + minimax polynomial), `src/common/dd.h`, `src/common/estrin.h`
- consumers:
  - `libs/builtin/src/math/special/*_highway.cpp` — erf (double-double)
  - `libs/builtin/src/math/trig/sinpi_kernel.hpp` + `trig_highway.cpp` —
    sinpi / cospi (single-double; the leading double of each split SLEEF
    constant, with a 64-bit octant index in place of SLEEF's 32-bit-lane
    range guard so large arguments stay accurate)
- license: Boost Software License 1.0 (see `LICENSE`)

The polynomial coefficients are copied verbatim; the double-double arithmetic
follows SLEEF's standard Dekker/FMA scheme. BSL-1.0 is permissive (not
copyleft) and compatible with numkit's PolyForm Noncommercial license; this
notice satisfies its source-attribution requirement. The compiled WASM/native
artifact carries no notice obligation.
