# numkit MATLAB Parity — Implementation Plan

Ordered phased plan for closing gaps catalogued in [MATLAB_PARITY_TODO.md](MATLAB_PARITY_TODO.md).
**Easiest first.** Each phase is sized so it can land as one or two atomic
commits, with a parity-bump estimate to track ROI.

Baseline: **296 ✅ + 24 ⚠️ / 1226 = 26%** (post commit `5b72296`).

Phases use these effort buckets:

- **S** small (≤ 30 min)
- **M** medium (1-3 h)
- **L** large (½-1 day)
- **XL** big (multi-day; usually a dedicated `libs/<name>/` add)

---

## Tier 1 — Trivial (~1 day total; lifts coverage by ~6%)

### 1.1 Operator-named functions [S]
Lift the 24 ⚠️ to ✅ by registering thin wrappers that route to the
existing operators. Each is 3-5 lines.

`plus`, `minus`, `times`, `mtimes`, `mldivide`, `mrdivide`, `rdivide`,
`ldivide`, `power`, `mpower`, `uminus`, `uplus`, `ctranspose`,
`transpose`, `eq`, `ne`, `lt`, `le`, `gt`, `ge`, `and`, `or`, `not`,
`colon`.

`mldivide`/`mrdivide`/`mpower` for matrix forms still need `libs/linalg`
— register the wrappers but route to the elementwise/operator path so
they are at least callable on the cases that already work.

→ 24 ✅, ~+2% coverage.

### 1.2 Numeric-limit constants [S]
`intmax`, `intmin`, `realmax`, `realmin`, `flintmax` — one-liners that
return `std::numeric_limits<T>` values.

→ 5 ✅, +0.4%.

### 1.3 Type-shape predicates [S]
`isvector`, `isrow`, `iscolumn`, `ismatrix`, `issorted`, `issortedrows`,
`isuniform` — predicates over `Dims`/sort state. Each ≤ 10 lines.

→ 7 ✅, +0.6%.

### 1.4 Index conversions [S]
`sub2ind`, `ind2sub`, `shiftdim` — column-major arithmetic on `Dims`.

→ 3 ✅, +0.25%.

### 1.5 N-D flip + repelem [S]
`flip(A, dim)` — generalized flip. `repelem` — element repeat.

→ 2 ✅, +0.2%.

### 1.6 Set ops + bit ops + array predicates [S]
`setxor`, `ismembertol`, `uniquetol`, `allunique` — extra hashing.
`bitset`, `bitget` — bit-level integer ops.
`allfinite`, `anynan` — short-circuit whole-array predicates.

→ 8 ✅, +0.65%.

### 1.7 File-path helpers [S]
`fullfile`, `fileparts`, `filesep`, `pathsep`, `tempdir`, `tempname` —
thin wrappers over `std::filesystem`.

→ 6 ✅, +0.5%.

### 1.8 Magnitude/phase utils [S]
`db`, `db2mag`, `db2pow`, `mag2db`, `pow2db`, `wrapToPi`, `wrap2Pi`,
`wrapTo180`, `wrapTo360`. Pure scalar/vector math.

→ 9 ✅, +0.75%.

**Tier 1 total: ~64 ✅, ≈ +5.5%, ~1 day of work.**

---

## Tier 2 — Simple kernels (~2-3 days; lifts coverage by ~7%)

### 2.1 Hyperbolic trig [M]
`sinh`, `cosh`, `tanh`, `asinh`, `acosh`, `atanh` (+ vector forms).
Existing `transcendental_*.cpp` SIMD backend already has the slot
pattern; just add the new entries.

→ 6 ✅, +0.5%.

### 2.2 Degree-argument trig [M]
`sind`, `cosd`, `tand`, `asind`, `acosd`, `atand`, `atan2d`, `sinpi`,
`cospi`. Wrappers that scale by π/180 (or π) and call the existing
radians kernel.

→ 9 ✅, +0.75%.

### 2.3 Reciprocal trig family [M]
`sec`, `csc`, `cot` and their `*h` / `*d` / `a*` variants
(~21 functions). All defined as `1 / sin(x)` etc. — straightforward
wrappers but a lot of them.

→ ~21 ✅, +1.7%.

### 2.4 Coordinate transforms [S]
`cart2pol`, `pol2cart`, `cart2sph`, `sph2cart`. Pure scalar arithmetic
returning multi-out tuples.

→ 4 ✅, +0.3%.

### 2.5 Cell / struct idioms [M]
`mat2cell`, `num2cell`, `cell2mat`, `iscellstr`, `cellplot`,
`cellstr`, `celldisp`, `getfield`, `setfield`, `orderfields`,
`struct2cell`, `cell2struct`, `deal`. Most are 10-30 lines each.

→ 13 ✅, +1.05%.

### 2.6 String utilities [M]
`strncmp`, `strncmpi`, `strfind`, `blanks`, `deblank`, `mat2str`,
`strtok`, `strjoin`, `strjust`, `cellstr` (alias).

→ 10 ✅, +0.8%.

### 2.7 Function-handle utilities [M]
`feval(handle, args...)`, `func2str(handle)`, `str2func(name)`. The
handle type already exists; these are inspection wrappers.

→ 3 ✅, +0.25%.

### 2.8 Rounding / exp extras [S]
`pow2`, `realpow`, `reallog`, `realsqrt`. Scalar wrappers with
domain checks.

→ 4 ✅, +0.3%.

### 2.9 Window-function extras [M]
`triang`, `tukeywin`, `flattopwin`, `gausswin`, `chebwin`, `parzenwin`,
`nuttallwin`, `taylorwin`. Each is a closed-form coefficient
generator; mirror the existing `hamming`/`hann` skeleton.

→ 8 ✅, +0.65%.

### 2.10 Signal stats one-pass [M]
`rms`, `rssq`, `peak2peak`, `peak2rms`. One pass over data, dim-arg
support via the existing `applyAlongDim` helper.

→ 4 ✅, +0.3%.

### 2.11 Waveform-generation extras [M]
`square`, `sawtooth`, `sinc`, `gmonopuls`, `diric`. Closed-form;
straight ports of the spec.

→ 5 ✅, +0.4%.

**Tier 2 total: ~87 ✅, ≈ +7%, ~2-3 days.**

---

## Tier 3 — Moderate algorithms (~1 week; lifts coverage by ~6%)

### 3.1 Moving statistics [L]
`movmean`, `movmedian`, `movmin`, `movmax`, `movsum`, `movstd`,
`movvar`, `movmad`, `movprod`. All share a sliding-window infra
(O(N·W) baseline; `movsum`/`movmean` admit O(N) running-sum).
Two-arg form `movmean(A, k)` + endpoint options `'omitnan'`,
`'Endpoints'`. Plus dispatcher `smoothdata` and `hampel`.

→ 11 ✅, +0.9%.

### 3.2 Filter conversions (algebraic) [M]
`sos2tf`, `sos2zp`, `tf2ss`, `ss2tf`, `ss2zp`, `zp2ss`, `sos2ss`,
`ss2sos`. Pure polynomial / matrix arithmetic.

→ 8 ✅, +0.65%.

### 3.3 Filter analysis predicates [M]
`isfir`, `islinphase`, `ismaxphase`, `isminphase`, `isstable`,
`isallpass`. Each is an algebraic test on coefficient arrays
(roots → unit-circle / mirror-image checks).

→ 6 ✅, +0.5%.

### 3.4 Filter analysis primary [M]
`impz`, `impzlength`, `stepz`, `phasedelay`, `zerophase`. Reuse the
existing `filter` kernel + simple state-walks.

→ 5 ✅, +0.4%.

### 3.5 Spec-driven filters [M]
`lowpass`, `highpass`, `bandpass`, `bandstop`. Wrappers chaining
order-estimate (`buttord`/`firpmord`) → coefficient generation
(`butter`/`fir1`) → `filtfilt`. Frontend-side glue, no new kernels.

→ 4 ✅, +0.3%.

### 3.6 Cepstrum + DFT helpers [M]
`cceps`, `rceps`, `icceps`, `dftmtx`, `bitrevorder`, `dst`, `idst`.
All built on the existing FFT.

→ 7 ✅, +0.55%.

### 3.7 Optimization 1-D + Nelder-Mead [L]
`fminbnd` (Brent's method on a bounded interval), `fminsearch`
(downhill simplex), `lsqnonneg` (active-set NNLS), `optimset` /
`optimget` (struct-options helpers).

→ 5 ✅, +0.4%.

### 3.8 Special functions [M]
`beta`, `betainc`, `betaincinv`, `gammainc`, `gammaincinv`,
`erfcinv`, `erfcx`, `expint`, `psi`, `legendre`. Use Cephes-style
series / continued-fractions; library-quality requires care, but
rough-cut wrappers around standard library equivalents land fast.

→ 10 ✅, +0.8%.

### 3.9 Bessel family [L]
`besselj`, `bessely`, `besseli`, `besselk`, `besselh`, `airy`. Need
the Numerical Recipes / Cephes implementations; non-trivial but
self-contained.

→ 6 ✅, +0.5%.

### 3.10 Output / IDE niceties [S]
`format` (short/long/eng/...), `diary`. Affects `disp` /
`fprintf` defaults.

→ 2 ✅, +0.16%.

**Tier 3 total: ~64 ✅, ≈ +5%, ~1 week.**

---

## Tier 4 — Larger algorithms (~2-3 weeks; lifts coverage by ~6%)

### 4.1 N-D FFT entry points [M]
`fft2`, `ifft2`, `fftn`, `ifftn`. Wrappers around the existing
`fft(x, [], dim)` applied across each dim. Mostly plumbing; add
parity tests against numpy/MATLAB reference.

→ 4 ✅, +0.3%.

### 4.2 Pulse-and-transition metrics [L]
`risetime`, `falltime`, `slewrate`, `settlingtime`, `overshoot`,
`undershoot`, `dutycycle`, `pulseperiod`, `pulsesep`, `statelevels`,
`midcross`. Need a state-level-detection helper (`statelevels`)
shared across the family.

→ 11 ✅, +0.9%.

### 4.3 Linear prediction / AR [L]
`lpc`, `levinson`, `arburg`, `arcov`, `armcov`, `aryule`,
`rlevinson`, `prony`, `stmcb`, `invfreqz`, `invfreqs`. Build a
`libs/signal/parametric/` sub-folder; share Levinson-Durbin core.

→ 11 ✅, +0.9%.

### 4.4 STFT family [L]
`stft`, `istft`, `pspectrum`. Build on existing `spectrogram` and
window/FFT primitives. Add `pspectrum` as an easy spectral-analysis
entry that dispatches to `pwelch`/`spectrogram`/`stft`.

→ 3 ✅, +0.25%.

### 4.5 Spectrum extras [L]
`cpsd`, `mscohere`, `tfestimate`, `pmtm`, `pburg`, `pyulear`,
`pmusic`, `peig`, `pcov`, `pmcov`, `bandpower`. Most reuse `pwelch`
infra; AR-based ones reuse Tier 4.3.

→ 11 ✅, +0.9%.

### 4.6 Spectral measurements [L]
`snr`, `sinad`, `thd`, `sfdr`, `toi`, `meanfreq`, `medfreq`. Built
on `periodogram` + harmonic-detection helper.

→ 7 ✅, +0.55%.

### 4.7 Detrending / change-point [M]
`detrend`, `findchangepts`, `cusum`, `findsignal`, `findregions`,
`findpeaks` extras (`peak2peak` already in 2.10 above).

→ 5 ✅, +0.4%.

### 4.8 Convolution extras [M]
`conv2`, `convn`, `convmtx`, `cconv`, `xcorr2`, `xcov`, `corrmtx`,
`alignsignals`, `finddelay`. Mostly reuse the existing `conv`/`xcorr`
kernels with shape generalisation.

→ 9 ✅, +0.75%.

**Tier 4 total: ~61 ✅, ≈ +5%, ~2-3 weeks.**

---

## Tier 5 — Dedicated-library effort (multi-week each)

These are large enough that each warrants a separate `libs/<name>/`
subtree, its own backends/, its own tests, and its own multi-session
plan. Listed in the order they are most useful.

### 5.1 `libs/linalg/` [XL]
LAPACK-class core: `det`, `inv`, `pinv`, `rank`, `null`, `orth`,
`norm`, `cond`, `rcond`, `linsolve`, `mldivide`/`mrdivide` (matrix
forms), `lu`, `qr`, `chol`, `svd`, `eig`, `eigs`, `schur`, `hess`,
`expm`, `logm`, `sqrtm`, `vecnorm`, `decomposition`, `lsqminnorm`,
`subspace`, `mpower` (matrix), `condeig`, `gsvd`, `bandwidth`,
`istriu`, `istril`, `isbanded`, `issymmetric`, `ishermitian`. Either
ship an in-tree implementation (Householder QR + iterative SVD/eig)
or wrap LAPACK / Eigen (decision pending — affects build). ~82
functions; lifts coverage by ~6.7% on its own.

### 5.2 `libs/sparse/` [XL]
Sparse matrix type + operations + iterative solvers: `sparse`,
`full`, `issparse`, `nnz` (already on dense), `nzmax`, `spalloc`,
`speye`, `sprand`, `sprandn`, `sprandsym`, `spdiags`, `spfun`,
`spy`, ordering (`amd`, `colamd`, `symamd`, `symrcm`, `dissect`),
iterative solvers (`pcg`, `bicg`, `bicgstab`, `cgs`, `gmres`,
`lsqr`, `minres`, `qmr`, `symmlq`, `tfqmr`), eigensolvers (`eigs`,
`svds`). Touches `Value` — sparse needs to be a recognised type
alongside dense.

### 5.3 `libs/ode/` [XL]
ODE / DAE / DDE / BVP / PDE. Initial focus: `ode23`, `ode45`,
`ode113`, `ode15s`, `ode23s`, `ode23t`, `ode23tb`, plus `odeset`/
`odeget`/`deval`. Then `bvp4c`/`bvp5c`, `dde23`/`ddesd`/`ddensd`,
finally `pdepe`. Reusable Adams/RK/BDF integrator core. ~31
functions.

### 5.4 Tables / Timetables / Categorical [XL]
Three new fundamental data types in `Value`: `table`, `timetable`,
`categorical`. Plus: ~75 table fns (`array2table`, `cell2table`,
`struct2table`, `table2*`, `addvars`, `removevars`, `convertvars`,
`vartype`, `findgroups`/`splitapply`/`groupcounts`/etc.,
`innerjoin`/`outerjoin`/`stack`/`unstack`, `synchronize`, `head`,
`tail`, `summary`); ~17 categorical fns (`addcats`, `removecats`,
`mergecats`, `categories`, `iscategory`, `isordinal`, `summary`,
`countcats`); plus the reader/writer family (`readtable`,
`writetable`, `readtimetable`, `writetimetable`, etc.). Single
biggest backlog item.

### 5.5 Datetime / Duration / Calendar [XL]
New fundamental types `datetime`, `duration`, `calendarDuration`
plus the entire `year`/`month`/`day`/`hour`/`minute`/`second`/
`hms`/`ymd`/`weekday`/`now`/`clock`/`datenum`/`datevec`/`datestr`/
`isnat`/`NaT`/`dateshift`/`between`/`caldiff`/`timeofday` family.
Often blocks the table family because timetables need it.

### 5.6 `containers.Map` / `dictionary` [L]
Hash-map type. R2022b `dictionary` is the modern form with typed
keys/values; `containers.Map` is the legacy version. Affects
`Value` dispatch. ~10 functions.

### 5.7 Wavelet family [XL]
`cwt`, `wsst`, `vmd`, `hht`, `emd`, `dwt`, `idwt`, `modwt`,
`imodwt`, `wt`. Whole continuous-wavelet/sync-squeezed/EMD line.
Either ship in-tree or skip — niche.

### 5.8 Vibration analysis [L]
`envspectrum`, `tachorpm`, `rpmtrack`, `rpmfreqmap`, `rpmordermap`,
`orderspectrum`, `orderwaveform`, `ordertrack`, `modalfit`,
`modalfrf`. Order-tracking + modal analysis — niche but
self-contained on top of FFT.

### 5.9 Graphics expansion [L]
The 5+ graphics gaps in `Tier 5`-equivalent: `plot3`, `loglog`,
`semilogx`/`semilogy`, `errorbar`, `fplot`/`fplot3`, full
`histogram` (modern), `contour3`/`contourc`/`contourslice`,
`quiver`/`quiver3`, `streamline`/`streamslice`/`stream2`/`stream3`,
volume vis (`slice`/`isosurface`/`isocaps`/`isonormals`/...),
`tiledlayout`/`nexttile`, `errorbar`, `fmesh`/`fsurf`/`surfc`/
`surfl`, etc. Probably worth grouping into one or two graphics
sprints.

### 5.10 Spreadsheets, modern CSV, MAT-binary [L]
`readmatrix`/`writematrix`, `readcell`/`writecell`, `readvars`,
`dlmread`/`dlmwrite`, full XLS/XLSX support, `matfile` (memory-mapped
MAT), `importdata`. Couples to Tier 5.4 (tables).

---

## Coverage projection

| After | ✅+⚠️ count | Coverage |
|---|---|---|
| Today (`5b72296`) | 320 / 1226 | 26% |
| Tier 1 done | 384 / 1226 | 31% |
| Tier 2 done | 471 / 1226 | 38% |
| Tier 3 done | 535 / 1226 | 43% |
| Tier 4 done | 596 / 1226 | 48% |
| + linalg (5.1) | 678 / 1226 | 55% |
| + sparse (5.2) | 731 / 1226 | 60% |
| + ODE (5.3) | 762 / 1226 | 62% |
| + tables (5.4) | 837 / 1226 | 68% |
| + datetime (5.5) | 875 / 1226 | 71% |

*Numbers are upper bounds: percentages assume every listed function
gets the basic single-arg form working. Multi-arg / option-set
parity needs a second sweep per family.*

---

## Cadence recommendation

1. **Land Tier 1 in one session** as a single commit `Parity bump: trivial wrappers (+64)`. Big morale + immediate +5% coverage.
2. **Tier 2** — split into 3-4 commits along family lines (trig family / cell family / window family). Each commit ≤ 30 fns, ≤ 1 day's work.
3. **Tier 3** — one commit per item (3.1 movstats, 3.2 filter conv, etc.). Now we are touching algorithms; pair every commit with a parity test against MATLAB / Octave reference output.
4. **Tier 4** — proper plan-doc per item, multi-session.
5. **Tier 5** — each item gets a dedicated `project_<lib>_plan.md` memory entry before starting.

After every tier, regenerate [MATLAB_PARITY_TODO.md](MATLAB_PARITY_TODO.md) (the
generator script lives in this conversation's history; can be re-derived
in ~2 minutes from `/tmp/numkit_fns.txt` + `/tmp/all_combined.txt`).

---

*Generated 2026-04-28 alongside MATLAB_PARITY_TODO.md.*
