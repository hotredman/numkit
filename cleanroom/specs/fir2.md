# Clean-room specification — `fir2`

Frequency-sampling FIR filter design (Signal Processing toolbox).
Written per `cleanroom/PROTOCOL.md` (Spec Author role).

> **Scope.** Functional equivalence to MATLAB R2025b `fir2` over its
> full documented argument set — `fir2(n,f,m)`, `fir2(n,f,m,npt,lap)`,
> `fir2(...,window)`. The current numkit `fir2` implements only the
> 3-argument form; this rewrite is both an IP-clean reimplementation
> *and* a completion of the `npt` / `lap` / `window` arguments and the
> odd-order correction. The algorithm is the standard frequency-
> sampling method; the design is fully determined by this spec, so the
> rewrite is expected to be bit-compatible with MATLAB. Where a corner
> falls outside tolerance the test is re-baselined and documented.

## Public references

- A. V. Oppenheim & R. W. Schafer, *Discrete-Time Signal Processing*,
  3rd ed., Pearson, 2010 — §7.4–7.5, FIR design by the frequency-
  sampling method: sample the desired frequency response on a uniform
  grid, take the inverse DFT, and apply a window.
- L. R. Rabiner & B. Gold, *Theory and Application of Digital Signal
  Processing*, Prentice-Hall, 1975 — frequency-sampling FIR design and
  linear-phase filter structure.
- T. W. Parks & C. S. Burrus, *Digital Filter Design*, Wiley, 1987 —
  windowed frequency-sampling design.
- MATLAB R2025b `help fir2` — the documented interface: arguments
  `n,f,m,npt,lap,window`; `npt` "Number of grid points", default 512;
  `lap` "Length of region around duplicate frequency points";
  output `b` a row vector; default window is Hamming.

## 1. Interface

`b = fir2(n, f, m [, npt] [, lap] [, window])`

- **`n`** — filter order, a positive integer.
- **`f`** — frequency breakpoints, normalised so 1.0 is the Nyquist
  frequency. Must be non-decreasing, start at 0, end at 1.
- **`m`** — the desired magnitude at each breakpoint; same length as
  `f` (≥ 2 elements).
- **`npt`** *(optional)* — number of frequency-grid points; default
  512.
- **`lap`** *(optional)* — width (in grid points) of the smoothing
  region applied at a duplicated frequency (a response discontinuity);
  default `floor(npt/25)` (note: the MATLAB `help` text says "25", but
  the observed default is `floor(npt/25)`, e.g. 20 for npt = 512).
- **`window`** *(optional)* — a length-`(nn)` window vector (row or
  column), where `nn` is the final filter length; default a Hamming
  window. Must equal the filter length, else error.

`b` is returned as a **row vector** of length `nn` (real `double`).

## 2. Algorithm

### 2.1 Odd-order correction

A symmetric (Type I/II linear-phase) FIR filter of **odd order** has a
forced zero at the Nyquist frequency. If `n` is odd **and** the
requested response at Nyquist `m[end]` is non-zero, the order is
increased by one: `n ← n + 1` (MATLAB emits a warning; numkit may emit
one or silently bump — behaviour parity only requires the length).

Let `nn = n + 1` be the final filter length.

### 2.2 Grid size `npt`

- The requested `npt` is the user value, or 512 by default.
- A user-supplied `npt` must satisfy `2·npt ≥ nn` — otherwise an error
  ("the number of grid points must be ≥ `ceil(nn/2)`").
- If `npt` was **defaulted** and `2·512 < nn` (a long filter), raise
  the request to `ceil(nn/2)` so the grid is large enough.
- The **working grid size** is the requested `npt` rounded up to the
  next power of two: `npt ← 2^ceil(log2(npt))`. (Observed: `npt = 300`
  produces the same result as `npt = 512`; `npt = 256` is used as-is.)

### 2.3 Build the half-band response `H`

`H` is the desired complex response sampled on `npt + 1` points
covering DC … Nyquist; grid point `j` (1-based, `j = 1 … npt+1`)
corresponds to normalised frequency `(j-1)/npt`.

Piecewise-linearly interpolate the `(f, m)` breakpoints onto the grid:

```
H[1] = m[1]
nb   = 1
for each segment i = 1 … length(f)-1:
    df = f[i+1] - f[i]
    if df == 0:                       # duplicated frequency: discontinuity
        nb = ceil(nb - lap/2)         # back up half the smoothing window
        ne = nb + lap                 # ramp across `lap` grid points
    else:
        ne = floor(f[i+1] * (npt+1))  # grid index of the segment end
    for j = nb … ne:
        if ne == nb:  inc = 0
        else:         inc = (j - nb) / (ne - nb)
        H[j] = inc * m[i+1] + (1 - inc) * m[i]
    nb = ne + 1
```

All `H` indices must stay within `1 … npt+1`; an index outside that
range is a malformed input (raise an error).

### 2.4 Linear-phase shift

To make the filter linear-phase, multiply the half-band response by a
pure delay of `dt = (nn - 1)/2` samples:

```
for k = 0 … npt:
    H[k] *= exp(-j · π · dt · k / npt)
```

### 2.5 Mirror, inverse transform, window

- Form the full-length-`2·npt` spectrum by Hermitian mirroring
  (excluding DC and the Nyquist bin):
  `Hfull = [ H[0 … npt] , conj(H[npt-1]), conj(H[npt-2]), …, conj(H[1]) ]`.
- `ht = real( ifft(Hfull) )` — the inverse DFT of length `2·npt`
  (`2·npt` is a power of two; numkit's own `signal::ifft` may be used).
- Take the first `nn` samples and apply the window:
  `b[k] = ht[k] · window[k]`, `k = 0 … nn-1`.
- Default window — Hamming of length `nn`:
  `window[k] = 0.54 - 0.46·cos(2π k /(nn-1))` (and `window[0]=1` when
  `nn == 1`).

Return `b` as a `1 × nn` row vector.

## 3. Error identifiers (keep the existing `m:fir2:*` family)

- `m:fir2:nargin` — too few arguments (registration layer).
- `m:fir2:BadN` — `n` not a positive integer.
- `m:fir2:MismatchedDimensions` — `length(f) != length(m)`.
- `m:fir2:BadFLen` — fewer than 2 breakpoints.
- `m:fir2:InvalidRange` — `f` does not start at 0 / end at 1.
- `m:fir2:InvalidFreqVec` — `f` not non-decreasing.
- `m:fir2:InvalidNpt` — `npt` too small for the order.
- `m:fir2:BadWindow` — window length ≠ filter length.
- `m:fir2:SignalErr` — internal grid index out of range.

## 4. numkit interface

```cpp
struct Fir2Options {
    int   npt    = 0;            // 0 → default (512, auto-grown)
    int   lap    = 0;            // 0 → default floor(npt/25)
    Value window = Value::Empty();   // empty → Hamming(nn)
};

Value fir2(int n, const Value &f, const Value &m,
           const Fir2Options &opts = {},
           std::pmr::memory_resource *mr = nullptr);
```

PMR HARD RULE: scratch via `ScratchArena` / `ScratchVec<T>`; the
returned `Value` is allocated on `mr`. The implementation may call
numkit's own `signal::ifft`.

## 5. Verification

- gtest `libs/signal/tests/fir2_test.cpp` — lowpass / bandpass /
  highpass on the 3-arg form, plus the new branches: explicit
  `npt` / `lap`, a custom `window`, and the odd-order length bump.
  Hardcoded expected values are MATLAB R2025b reference output.
- Parity `tools/parity/specs/signal_fir2.json` — require
  `correctness = OK` vs MATLAB R2025b. The 3-arg form is expected to
  stay bit-compatible; extend the fingerprint to the new arguments. If
  a corner falls outside `tol`, re-baseline it and record the reason
  in the spec comment (the SRH/PEF/adapthisteq precedent).
- **MATLAB-independent correctness test** (mandatory): build a lowpass
  `fir2`, evaluate `|B(e^jω)|` (via FFT of the zero-padded `b`), and
  assert the magnitude is ≈ 1 in the passband and ≈ 0 in the stopband
  — i.e. the filter actually realises the requested response — and
  that `b` is symmetric (`b == fliplr(b)`, linear phase).

## Constraints for the Implementer

- Do **not** open `libs/signal/src/filter_design/filter_design.cpp`.
- Do **not** consult MATLAB `.m` source (`fir2.m`) or any third-party
  reference implementation.
- Implement solely from this specification and the cited public
  references. Every numerical convention needed for a correct result
  (1-based grid indices, `floor`/`ceil` choices, the mirror layout,
  the phase-shift sign) is stated above; follow them exactly.
