# SLEEF — math-kernel provenance (Boost Software License 1.0)

numkit does **not** link or vendor SLEEF. A few special-function kernels in
`libs/builtin/src/math/special/*_highway.cpp` were **ported** (transcribed
into Google Highway operations) from SLEEF's double-precision SIMD library:

- source: https://github.com/shibatch/sleef , commit `7623d6c`
- files: `src/libm/sleefsimddp.c` (xerf_u1, …), `src/common/dd.h`,
  `src/common/estrin.h`
- license: Boost Software License 1.0 (see `LICENSE`)

The polynomial coefficients are copied verbatim; the double-double arithmetic
follows SLEEF's standard Dekker/FMA scheme. BSL-1.0 is permissive (not
copyleft) and compatible with numkit's PolyForm Noncommercial license; this
notice satisfies its source-attribution requirement. The compiled WASM/native
artifact carries no notice obligation.
