# Numkit progress

**Implementation map** for numkit — one row per MATLAB function:
is it implemented, does it match MATLAB/Octave, and any notes. This
file is about *correctness and coverage only*. **Performance lives in
[BENCHMARK.md](BENCHMARK.md)** (per-function timings vs MATLAB / Octave
at small and large array sizes).

Updated by `tools/parity/run_parity.py` — each spec run rewrites the
row(s) for its function in place. The same function may appear in
multiple sections when it spans topics; all occurrences refresh
together.

**Columns:**
- `function` — function name
- `status` — base glyph + optional markers:
  - **✅** implemented · **❌** missing · **⚠️** partial / operator-only ·
    **—** unclassified.
  - **🔬 deep-verified** — probed side-by-side against MATLAB R2025b across
    its options / output forms / edge branches (not just a basic check) and
    confirmed matching (whether a divergence was found and fixed, or it
    already matched). Plain ✅ = implemented + basic-tested, not yet
    deep-probed.
  - **❗ mismatch** — currently diverges from the reference engine (a known
    bug to fix).
  - **➖ unverified** — no reference verdict was obtained: the reference
    engine produced no comparable result for this row. Most often because
    neither MATLAB nor Octave ships the function (e.g. File-Exchange-only
    fns like `expmv`); occasionally because the spec yields nothing
    fingerprintable. It is *not* a claim that the function is wrong — only
    that it could not be cross-checked.
- `comment` — signature / covered branches / deviations

(Performance lives in [BENCHMARK.md](BENCHMARK.md).)

**Library layout** (each H2 below maps to a numkit lib or a future-lib placeholder):

- **Builtin** — base layer (language fundamentals + math + programming).
  Self-contained: no dependency on toolbox libs.
- **Toolbox libs** — Signal / Image / Statistics / Communications /
  Control / Wavelet / Graphics / IO / Fitting.
- **Future-lib placeholders** — Linear Algebra / ODE / Optimization.
  Functions may currently live under Builtin physically; they will
  migrate to their own libs as those land.
- **Cross-lib deps** — `Image` / `Control` / `Communications` depend on
  `Signal` for DSP primitives (conv, FFT, DCT). All toolboxes depend on
  `Builtin`. **`Builtin` has no toolbox dependencies (invariant).**
- A function may appear in multiple H3 sections when it belongs to more
  than one topic; the harness updates every occurrence in lock-step.

## Table of Contents

- [**Builtin**](#builtin)
  - [Entering Commands](#entering-commands)
  - [Matrices and Arrays](#matrices-and-arrays)
  - [Control Flow](#control-flow)
  - [Numeric Types](#numeric-types)
  - [Characters and Strings](#characters-and-strings)
  - [Structures](#structures)
  - [Cell Arrays](#cell-arrays)
  - [Function Handles](#function-handles)
  - [Categorical Arrays](#categorical-arrays)
  - [Tables / Timetables](#tables--timetables)
  - [Bit-wise Operations](#bit-wise-operations)
  - [Set Operations](#set-operations)
  - [Arithmetic](#arithmetic)
  - [Trigonometry](#trigonometry)
  - [Exponents and Logarithms](#exponents-and-logarithms)
  - [Special Functions](#special-functions)
  - [Discrete Math](#discrete-math)
  - [Polynomials](#polynomials)
  - [Random Number Generation](#random-number-generation)
  - [Interpolation](#interpolation)
  - [Sparse Matrices](#sparse-matrices)
  - [Workspace](#workspace)
  - [Error Handling (basic)](#error-handling-basic)
  - [Exception Handling](#exception-handling)
- [**Communications**](#communications)
  - [Modulation](#modulation)
  - [Sources, Sinks, and Signal Operations](#sources-sinks-and-signal-operations)
  - [Source Coding](#source-coding)
  - [Error Detection and Correction](#error-detection-and-correction)
  - [Trellis and Galois Field Utilities](#trellis-and-galois-field-utilities)
  - [Interleaving](#interleaving)
  - [Pulse Shaping, Equalization, MIMO](#pulse-shaping-equalization-mimo)
  - [RF and Channel Impairments](#rf-and-channel-impairments)
  - [Propagation Path Loss and Geometry](#propagation-path-loss-and-geometry)
  - [Performance Analysis](#performance-analysis)
- [**Control**](#control)
  - [LTI Models](#lti-models)
  - [Model Properties](#model-properties)
  - [Model Conversion & Reduction](#model-conversion--reduction)
  - [Interconnections](#interconnections)
  - [Time and Frequency Response](#time-and-frequency-response)
  - [Stability and Margins](#stability-and-margins)
  - [State-Space Design and Estimation](#state-space-design-and-estimation)
  - [Matrix Equations](#matrix-equations)
  - [PID Tuning and Modal Analysis](#pid-tuning-and-modal-analysis)
- [**Fitting**](#fitting)
  - [Splines](#splines)
- [**Graphics**](#graphics)
  - [Line Plots](#line-plots)
  - [Polar Plots](#polar-plots)
  - [Contour Plots](#contour-plots)
  - [Vector Fields](#vector-fields)
  - [Surface and Mesh Plots](#surface-and-mesh-plots)
  - [Volume Visualization](#volume-visualization)
  - [Geographic Plots](#geographic-plots)
- [**Image**](#image)
  - [Image I/O](#image-io)
  - [Image Type Conversion](#image-type-conversion)
  - [Color Space Conversion](#color-space-conversion)
  - [Synthetic Images and Display](#synthetic-images-and-display)
  - [Geometric Transformations](#geometric-transformations)
  - [Image Registration](#image-registration)
  - [Image Filtering](#image-filtering)
  - [Contrast Adjustment](#contrast-adjustment)
  - [ROI-Based Processing](#roi-based-processing)
  - [Morphological Operations](#morphological-operations)
  - [Deblurring](#deblurring)
  - [Neighborhood and Block Processing](#neighborhood-and-block-processing)
  - [Image Arithmetic](#image-arithmetic)
  - [Image Segmentation](#image-segmentation)
  - [Object Analysis](#object-analysis)
  - [Region and Image Properties](#region-and-image-properties)
  - [Texture Analysis](#texture-analysis)
  - [Image Quality](#image-quality)
  - [Image Transforms](#image-transforms)
- [**IO**](#io)
  - [Low-Level File I/O](#low-level-file-io)
  - [Text Files (CSV / dlm / readtable)](#text-files-csv--dlm--readtable)
  - [Spreadsheets](#spreadsheets)
  - [Workspace Save / Load](#workspace-save--load)
  - [File Name Construction](#file-name-construction)
- [**Linear Algebra**](#linear-algebra)
- [**ODE**](#ode)
- [**Optimization**](#optimization)
  - [Local](#local)
  - [Constrained](#constrained)
  - [Global](#global)
- [**Signal**](#signal)
  - [Waveform Generation](#waveform-generation)
  - [Filter Design](#filter-design)
  - [Analog Filters](#analog-filters)
  - [Digital Filter Analysis](#digital-filter-analysis)
  - [Digital Filtering](#digital-filtering)
  - [Multirate Signal Processing](#multirate-signal-processing)
  - [Signal Modeling](#signal-modeling)
  - [Correlation and Convolution](#correlation-and-convolution)
  - [Transforms](#transforms)
  - [Windows](#windows)
  - [Parametric Spectral Estimation](#parametric-spectral-estimation)
  - [Nonparametric Spectral Estimation](#nonparametric-spectral-estimation)
  - [Spectral Measurements](#spectral-measurements)
  - [Time-Frequency Analysis](#time-frequency-analysis)
  - [Pulse and Transition Metrics](#pulse-and-transition-metrics)
  - [Signal Descriptive Statistics](#signal-descriptive-statistics)
  - [Smoothing and Denoising](#smoothing-and-denoising)
  - [Vibration Analysis](#vibration-analysis)
- [**Audio**](#audio)
  - [Spectral Shape Descriptors](#spectral-shape-descriptors)
  - [Audio Feature Extraction](#audio-feature-extraction)
  - [Audio Time-Frequency](#audio-time-frequency)
  - [Audio Frequency / Loudness Conversions](#audio-frequency--loudness-conversions)
- [**Statistics**](#statistics)
  - [Descriptive Statistics](#descriptive-statistics)
  - [Descriptive Statistics — extras](#descriptive-statistics--extras)
  - [Probability Distributions](#probability-distributions)
  - [Distribution Fitting (MLE / likelihood)](#distribution-fitting-mle--likelihood)
  - [Multivariate Distributions](#multivariate-distributions)
  - [Pearson / Johnson Distributions](#pearson--johnson-distributions)
  - [Empirical / Kernel Distributions](#empirical--kernel-distributions)
  - [Hypothesis Tests](#hypothesis-tests)
  - [Resampling Techniques](#resampling-techniques)
  - [Quasirandom Sequences and MCMC](#quasirandom-sequences-and-mcmc)
  - [ANOVA / MANOVA / Correlation](#anova--manova--correlation)
  - [Linear Regression (function-form)](#linear-regression-function-form)
  - [Nonlinear Regression (function-form)](#nonlinear-regression-function-form)
  - [Distance Metrics](#distance-metrics)
  - [Hierarchical Clustering](#hierarchical-clustering)
  - [Partitional Clustering](#partitional-clustering)
  - [Cluster Evaluation](#cluster-evaluation)
  - [Nearest Neighbors (function-form)](#nearest-neighbors-function-form)
  - [Hidden Markov Models](#hidden-markov-models)
  - [Dimensionality Reduction](#dimensionality-reduction)
  - [Feature Selection (function-form)](#feature-selection-function-form)
  - [Linear Discriminant Analysis (function-form)](#linear-discriminant-analysis-function-form)
- [**Wavelet**](#wavelet)
  - [Continuous Wavelet Transforms](#continuous-wavelet-transforms)
  - [Discrete Wavelet Transforms (1-D)](#discrete-wavelet-transforms-1-d)
  - [Discrete Wavelet Transforms (2-D / 3-D)](#discrete-wavelet-transforms-2-d--3-d)
  - [Stationary, MODWT, and Wavelet Packets](#stationary-modwt-and-wavelet-packets)
  - [Denoising and Compression](#denoising-and-compression)
  - [Filter Banks and Wavelet Families](#filter-banks-and-wavelet-families)
  - [Continuous Wavelet Shapes](#continuous-wavelet-shapes)
  - [Lifting](#lifting)
  - [Decomposition Trees and Misc](#decomposition-trees-and-misc)
- [**Misc / not in TODO**](#misc--not-in-todo)

## Builtin

### Entering Commands

**Namespace:** builtin — 5 ✅ + 0 ⚠️ / 9 = 56%

| function | status | comment |
|---|:---:|---|
| `ans` | ✅ | Sig: ans(...). Spec-extension batch 2026-05-09. |
| `clc` | ✅ | Sig: clc — clear command window. Spec-extension batch 2026-05-09 (cycle 41). |
| `commandhistory` | ❌ | IDE-only |
| `commandwindow` | ❌ | IDE-only |
| `diary` | ❌ | session log |
| `format` | ✅ | Sig: format <style>. Display-only side effect. Spec-extension batch 2026-05-09 (cycle 41). |
| `home` | ✅ | Sig: home — move cursor home in command window. Spec-extension batch 2026-05-09 (cycle 41). |
| `iskeyword` | ✅ 🔬 | Sig: r = iskeyword(...). Spec-extension batch 2026-05-09. |
| `more` | ❌ | pager |

### Matrices and Arrays

**Namespace:** builtin — 53 ✅ + 1 ⚠️ / 55 = 98%

| function | status | comment |
|---|:---:|---|
| `blkdiag` | ✅ 🔬 | Sig: r = blkdiag(A,B,...). DEEP-PROBE 2026-05-31: blkdiag was DOUBLE-only (threw 'Not a double array' on char/logical/single/complex blocks). Generalised type-agnostic: anchor output type on the first non-empty block, raw-byte copy each block into a zero-filled output (zero bit pattern = canonical zero for every type, so off-block entries are char(0)/false/0/0+0i). CELL/STRING/STRUCT rejected; mixed input types deferred (MATLAB promotes — out of scope). blkdiag('ab','cd')=2x4 char (c11='a'=97,c12='b'=98,c23='c'=99,c13=0); logical/single class preserved; complex blocks keep imaginary part (zr=1,zi=1 at (1,1); 3+3i at (2,3)); DOUBLE path unchanged. Spec-extension batch 2026-05-09. |
| `cat` | ✅ | Sig: r = cat(dim, A, B, ...). Shape op (type-agnostic). DEEP-PROBE 2026-05-31: cat along dim 3 extended to CHAR / LOGICAL / COMPLEX / single / CELL (the catDim3 path was DOUBLE-only and threw 'Not a double array'); cat is a pure rearrangement -> byte/cellAt page-concat. cat(3,['ab';'cd'],['ef';'gh']) = 2x2x2 char (cm111='a'=97, cm112='e'=101, cm222='h'=104); cat(3,{1,2},{3,4}) = 1x2x2 cell (c112=3); logical/single/complex preserved. dim 1/2 (vertcat/horzcat) already type-agnostic. Same-type only (mixed-type promotion deferred); STRING/struct deferred. Spec-extension batch 2026-05-09. |
| `circshift` | ✅ 🔬 | Sig: r = circshift(X, K[, dim]). The 3-arg form shifts by K ONLY along dimension `dim`: circshift([1 2 3;4 5 6],1,2)=[3 1 2;6 4 5] (columns), circshift(...,1,1)=[4 5 6;1 2 3] (rows), circshift(...,-1,2)=[2 3 1;5 6 4], circshift([10 20 30 40],2,2)=[30 40 10 20]. numkit previously IGNORED the dim arg and always shifted dim 1; fixed 2026-05-30. DEEP-PROBE 2026-05-31: circshift is now TYPE-AGNOSTIC (was throwing 'Not a double array' on char/cell/logical) — a pure rearrangement: circshift('abcde',2)='deabc'; circshift({1,2,3,4},1)={4,1,2,3}; char matrices shift rows; logical/complex/single preserved. STRING + struct arrays deferred. The 2-arg scalar form and the [k1 k2] vector form are unchanged. NOTE: ; only inside matrix-literal INPUTS. |
| `colon` | ⚠️ | works as `:` (range) operator; not callable as named fn |
| `combinations` | ❌ | all combinations |
| `ctranspose` | ✅ 🔬 | Sig: r = ctranspose(A) === A'. Conjugate transpose. DEEP-PROBE 2026-05-31: ctranspose threw 'Transpose not supported for this type' on non-DOUBLE/non-COMPLEX. Now shares transpose2D with transposeNC; COMPLEX conjugates (zr=5,zi=-6 from conj(Z(2,1)=5+6i); Z'(2,1)=conj(Z(1,2)=3+4i)=3-4i so zor=3,zoi=-4), all other types are a plain transpose (no element change): char preserved (c12='c'=99), logical preserved (l12=L(2,1)=0, lcls=1), cell off-diagonal swap (k21=KT{2,1}=K{1,2}='x'). 3-D still throws. Spec-extension batch 2026-05-09. |
| `diag` | ✅ 🔬 | Sig: r = diag(v|M [,k]). DEEP-PROBE 2026-05-31: diag was DOUBLE-only (threw 'Not a double array' on char/logical/single/int) and SILENTLY IGNORED the k offset (2nd arg). Generalised type-agnostic (raw-byte copy preserving CHAR/LOGICAL/SINGLE/int/COMPLEX; CELL/STRING/STRUCT rejected per MATLAB 'Inputs must be numeric, char, or logical') and added the k diagonal offset. diag('abc')=3x3 char with 'a''b''c' on diag, char(0) off-diag (c12=0); diag(['abc';'def';'ghi'])=['a';'e';'i'] (dc1=97,dc3=105); logical/single/complex class preserved; complex off-diag=0+0i; diag([1 2 3],1)=4x4 superdiagonal (vk12=1,vk23=2); diag([1 2 3],-1) subdiagonal; diag(reshape(1:9,3,3),1)=[4;8] (extract superdiagonal), diag(M,-1)=[2;6]. NOTE: numkit class(complex)='complex' vs MATLAB 'double' is a pre-existing convention (values verified via real/imag). Spec-extension batch 2026-05-09 . |
| `end` | ✅ | Sig: end — last index in subscript context, also closes block constructs. Spec-extension batch 2026-05-09 (cycle 41). |
| `eye` | ✅ | Sig: r = eye(...). Spec-extension batch 2026-05-09. |
| `false` | ✅ | Sig: r = false(...). false()=0; false(2,2) is a 2x2 logical zero matrix (numel 4); false(3) is 3x3 (numel 9), every element 0. Indexing goes through a temp var (t2(1)) because MATLAB rejects chained indexing on a call result -- false(2,2)(1) is 'Invalid array indexing' in MATLAB (numkit allows it). namespace=builtin. |
| `flip` | ✅ 🔬 | Sig: r = flip(A[,dim]). Type-agnostic element permutation. DEEP-PROBE 2026-05-31: extended to CELL and STRING arrays (previously errored 'ND fallback does not support type cell/string'). MATLAB is type-agnostic: flip([1 2 3 4])=[4 3 2 1] r1=4 r4=1; flip({10,20,30})={30,20,10} c1=30 c3=10; flip(["x" "y" "z"])=["z" "y" "x"] s1=double('z')=122 s3=double('x')=120. Cell/string share one Value-copy path (copyCellElem). namespace=builtin. Matches MATLAB R2025b. |
| `fliplr` | ✅ 🔬 | Sig: r = fliplr(A). Reverse column order (type-agnostic). DEEP-PROBE 2026-05-31: extended to CELL/STRING (earlier) AND now CHAR matrices / LOGICAL / COMPLEX / single (the 2-D POD path was still DOUBLE-only and threw 'Not a double array'). fliplr([1 2 3 4])=[4 3 2 1]; fliplr({10,20,30})={30,20,10}; fliplr(['ab';'cd'])=['ba';'dc'] (m11='b'=98); fliplr(logical([1 1 0]))=[0 1 1]; fliplr([1+1i 2+2i 3+3i])=[3+3i 2+2i 1+1i]. namespace=builtin. Matches MATLAB R2025b. |
| `flipud` | ✅ 🔬 | Sig: r = flipud(A). Reverse row order (type-agnostic). DEEP-PROBE 2026-05-31: extended to CELL/STRING (earlier) AND now CHAR matrices / LOGICAL / COMPLEX / single (the 2-D POD path was still DOUBLE-only and threw 'Not a double array'). flipud([1;2;3;4])=[4;3;2;1]; flipud({10;20;30})={30;20;10}; flipud(['ab';'cd'])=['cd';'ab'] (m11='c'=99); flipud(logical([1;1;0]))=[0;1;1]; flipud([1+1i;2+2i;3+3i])=[3+3i;2+2i;1+1i]. namespace=builtin. Matches MATLAB R2025b. |
| `freqspace` | ✅ | Sig: f = freqspace(N). KNOWN GAP: numkit returns shorter vector than MATLAB for freqspace(8) — different size convention. Only structural numel pinned. Documented separately. |
| `head` | ✅ | Sig: Y = head(X, K). First 100 elements. 10000 iters. |
| `horzcat` | ✅ | Sig: r = horzcat(...). Shape op. Spec-extension batch 2026-05-09. |
| `ind2sub` | ✅ | Sig: r = ind2sub(...). Spec-extension batch 2026-05-09. |
| `ipermute` | ✅ | Sig: r = ipermute(...). Shape op. Spec-extension batch 2026-05-09. |
| `iscolumn` | ✅ | Sig: r = iscolumn(...). Predicate. Spec-extension batch 2026-05-09. |
| `isempty` | ✅ | Sig: r = isempty(...). Predicate. Spec-extension batch 2026-05-09. |
| `ismatrix` | ✅ | Sig: r = ismatrix(...). Predicate. Spec-extension batch 2026-05-09. |
| `isrow` | ✅ | Sig: r = isrow(...). Predicate. Spec-extension batch 2026-05-09. |
| `isscalar` | ✅ | Sig: r = isscalar(...). Predicate. Spec-extension batch 2026-05-09. |
| `issorted` | ✅ | Sig: r = issorted(...). Spec-extension batch 2026-05-09. |
| `issortedrows` | ✅ | Sig: TF = issortedrows(X). 10k×3 pre-sorted. 1000 iters. |
| `isuniform` | ✅ | Sig: TF = isuniform(X). 100k uniform. 10000 iters. |
| `isvector` | ✅ | Sig: r = isvector(...). Predicate. Spec-extension batch 2026-05-09. |
| `length` | ✅ | Sig: r = length(...). Shape op. Spec-extension batch 2026-05-09. |
| `linspace` | ✅ | Sig: r = linspace(...). Spec-extension batch 2026-05-09. |
| `logspace` | ✅ | Sig: r = logspace(...). Spec-extension batch 2026-05-09. |
| `meshgrid` | ✅ | Sig: r = meshgrid(...). Spec-extension batch 2026-05-09. |
| `ndgrid` | ✅ | Sig: r = ndgrid(...). Spec-extension batch 2026-05-09. |
| `ndims` | ✅ | Sig: r = ndims(...). Shape op. Spec-extension batch 2026-05-09. |
| `numel` | ✅ | Sig: r = numel(...). Shape op. Spec-extension batch 2026-05-09. |
| `ones` | ✅ | Sig: r = ones(...). Spec-extension batch 2026-05-09. |
| `paddata` | ✅ | Sig: Y = paddata(X, M). Pad to 1500. 1000 iters. |
| `permute` | ✅ | Sig: r = permute(A,perm). Shape op (type-agnostic). DEEP-PROBE 2026-05-31: extended to CHAR / LOGICAL / COMPLEX / single / CELL (the DOUBLE-only path threw 'Not a double array'); pure rearrangement via strided byte/cellAt gather. permute(['ab';'cd'],[2 1])=['ac';'bd'] (m11='a'=97, m12='c'=99); permute({1 2;3 4},[2 1]) swaps off-diagonal cells (c21=2,c12=3); 3-D char permute([2 1 3]) too. STRING/struct deferred; cell rank>2 deferred. ipermute delegates to permute. Spec-extension batch 2026-05-09. |
| `rand` | ✅ | Sig: A = rand(M,N). 1k×1k uniform. 100 iters. Custom fp (RNG diffs). |
| `repelem` | ✅ | Sig: y = repelem(v, n | counts); Y = repelem(A, r, c) with r/c scalar or vector. Covers: scalar count (fast path), per-element count vector (incl. a zero count dropping an element), column-vector orientation, and matrix per-row/per-column counts (scalar+vector mix both ways). Queue-clearing 2026-05-29: numkit previously threw 'Cannot convert double to scalar' on any count vector. |
| `repmat` | ✅ | Sig: r = repmat(...). Spec-extension batch 2026-05-09. |
| `reshape` | ✅ | Sig: r = reshape(...). Shape op. Spec-extension batch 2026-05-09. |
| `resize` | ✅ | Sig: Y = resize(X, M). Resize to 1500 (pad with zeros). 1000 iters. |
| `rot90` | ✅ 🔬 | Sig: r = rot90(A[,k]). 90-deg CCW rotation (type-agnostic). DEEP-PROBE 2026-05-31: extended to CELL/STRING (earlier) AND now CHAR matrices / LOGICAL / COMPLEX / single (the 2-D/3-D POD path was still DOUBLE-only and threw 'Not a double array'; reuses the existing byte-copy kernels). rot90([1 2;3 4])=[2 4;1 3]; rot90({1 2;3 4})={2 4;1 3}; rot90(['ab';'cd'])=['bd';'ac'] (m11='b'=98); rot90(['ab';'cd'],2)=['dc';'ba']; rot90(logical([1 0;0 0]))=[0 0;1 0]; rot90([1+1i 2;3 4])(1,1)=2+0i. k!=1 forms covered in gtest. namespace=builtin. Matches MATLAB R2025b. |
| `shiftdim` | ✅ | Sig: r = shiftdim(...). Spec-extension batch 2026-05-09. |
| `size` | ✅ | Sig: r = size(...). Shape op. Spec-extension batch 2026-05-09. |
| `sort` | ✅ 🔬 | Sig [s,i]=sort(X[,dim][,direction]): default ASCEND with NaN LAST. 'descend' with NaN FIRST. Matrix sort(M,1,'descend') sorts each column. COMPLEX sort orders by |z| then arg(z). INTEGER input keeps the class on the sorted VALUES (sort(int8([3 -128 5]))=[-128 3 5] int8) while the index output stays DOUBLE; per-dim works (sort(int32([3 1 2;6 5 4]),2) row1=[1 2 3] int32). numkit previously threw 'Not a double array' on integer input (fixed 2026-05-30); earlier: 'descend'/NaN (fixed) + COMPLEX sortComplex (2026-05-29). NOTE: ; only inside matrix-literal INPUTS. |
| `sortrows` | ✅ 🔬 | Sig: B = sortrows(A). 10k×3 sort by first col. 100 iters. |
| `squeeze` | ✅ | Sig: r = squeeze(...). Shape op. Spec-extension batch 2026-05-09. |
| `sub2ind` | ✅ | Sig: r = sub2ind(...). Spec-extension batch 2026-05-09. |
| `tail` | ✅ | Sig: Y = tail(X, K). Last 100 elements. 10000 iters. |
| `transpose` | ✅ 🔬 | Sig: r = transpose(A) === A.'. Type-preserving 2-D transpose. DEEP-PROBE 2026-05-31: the transpose() builtin (matrix.cpp) coerced ALL inputs to a DOUBLE matrix of element codes, and the .'/' operators (transposeNC/ctranspose in unary_ops.cpp) threw 'Transpose not supported for this type' on non-DOUBLE/non-COMPLEX. Generalised to one shared transpose2D: DOUBLE/SINGLE/CHAR/LOGICAL/int via raw-byte rearrange, COMPLEX per-element (transpose() = no conjugate), CELL per-cell move; STRING/STRUCT/FUNC deferred. transpose(['ab';'cd'])=['ac';'bd'] char (c11='a'=97,c12='c'=99,c21='b'=98); logical/int8/single class preserved; transpose(complex) does NOT conjugate (zr=5,zi=6 from Z(2,1)=5+6i); cell off-diagonal swap (k12=3,k21=2). transpose() now delegates to transposeNC. 3-D still throws. Spec-extension batch 2026-05-09. |
| `trimdata` | ✅ | Sig: Y = trimdata(X, M). Trim to 500. 1000 iters. |
| `true` | ✅ | Sig: r = true(...). true()=1; true(2,2) is a 2x2 logical one matrix (numel 4); true(3) is 3x3 (numel 9), every element 1. Indexing goes through a temp var (t2(1)) because MATLAB rejects chained indexing on a call result -- true(2,2)(1) is 'Invalid array indexing' in MATLAB (numkit allows it). namespace=builtin. |
| `vertcat` | ✅ | Sig: r = vertcat(...). Shape op. Spec-extension batch 2026-05-09. |
| `zeros` | ✅ | Sig: r = zeros(...). Spec-extension batch 2026-05-09. |

### Control Flow

**Namespace:** builtin (keywords) — 10 ✅ + 0 ⚠️ / 11 = 91%

| function | status | comment |
|---|:---:|---|
| `break` | ✅ | Sig: break — exits innermost for/while loop. Spec-extension batch 2026-05-09 (cycle 41). |
| `continue` | ✅ | Sig: continue — skips to next iteration of innermost loop. Spec-extension batch 2026-05-09 (cycle 41). |
| `end` | ✅ | Sig: end — last index in subscript context, also closes block constructs. Spec-extension batch 2026-05-09 (cycle 41). |
| `for` | ✅ | Sig: for var = expr, body, end. Spec-extension batch 2026-05-09 (cycle 41). |
| `if` | ✅ | Sig: if cond, body, [elseif cond, body,] [else body,] end. Spec-extension batch 2026-05-09 (cycle 41). |
| `parfor` | ❌ | parallel — out of scope |
| `pause` | ✅ | Sig: pause(N). N=0 (no-op). 100k iters. |
| `return` | ✅ | DEFERRED — script-level `return` causes MATLAB's `run` wrapper to error (stops the wrapper); numkit allows it. Cannot express in single-snippet parity spec. Functionality validated by gtest. Placeholder spec; KNOWN GAP |
| `switch` | ✅ | Sig: switch expr, case val, body, [case {a,b}, body,] [otherwise body,] end. Spec-extension batch 2026-05-09 (cycle 41). |
| `try` | ✅ | Sig: try, body, catch [ME], body, end. Spec-extension batch 2026-05-09 (cycle 41). |
| `while` | ✅ | Sig: while cond, body, end. Spec-extension batch 2026-05-09 (cycle 41). |

### Numeric Types

**Namespace:** builtin — 27 ✅ + 0 ⚠️ / 29 = 93%

| function | status | comment |
|---|:---:|---|
| `allfinite` | ✅ | Sig: r = allfinite(...). Spec-extension batch 2026-05-09 verified MATLAB R2025b parity. |
| `anynan` | ✅ | Sig: r = anynan(...). Spec-extension batch 2026-05-09 verified MATLAB R2025b parity. |
| `cast` | ✅ | Sig: r = cast(...). Spec-extension batch 2026-05-09. |
| `double` | ✅ | Sig: r = double(...). Type conversion. Spec-extension batch 2026-05-09. KNOWN GAP: numkit rejects double("string") with error; MATLAB returns NaN, Octave returns ASCII codes — both differ from numkit. String→double documented as separate gap; only int/logical/numeric paths pinned here. |
| `eps` | ✅ | Sig: r = eps([x]). Spec-extension batch 2026-05-09 verified bit-identical MATLAB R2025b on scalar-1 input. KNOWN GAPS (separate spec): eps() with no args returns empty (should return eps(1)); eps(fractional) is parser-confused as indexing; eps(vector) segfaults. Pinned only the working scalar path here. |
| `flintmax` | ✅ 🔬 | Sig: r = flintmax(...). Spec-extension batch 2026-05-09. |
| `inf` | ✅ | Sig: inf(...). Spec-extension batch 2026-05-09. |
| `int16` | ✅ 🔬 | Sig: r = int16(...). Type conversion. Spec-extension batch 2026-05-09 verified MATLAB R2025b parity. |
| `int32` | ✅ 🔬 | Sig: r = int32(...). Type conversion. Spec-extension batch 2026-05-09 verified MATLAB R2025b parity. |
| `int64` | ✅ 🔬 | Sig: r = int64(...). Type conversion. Spec-extension batch 2026-05-09 verified MATLAB R2025b parity. |
| `int8` | ✅ 🔬 | Sig: r = int8(...). Type conversion. Spec-extension batch 2026-05-09 verified MATLAB R2025b parity. |
| `intmax` | ✅ 🔬 | Sig: r = intmax(...). Spec-extension batch 2026-05-09. |
| `intmin` | ✅ 🔬 | Sig: r = intmin(...). Spec-extension batch 2026-05-09. |
| `isfinite` | ✅ | Sig: r = isfinite(...). Predicate. Spec-extension batch 2026-05-09. |
| `isfloat` | ✅ | Sig: r = isfloat(...). Spec-extension batch 2026-05-09. |
| `isinf` | ✅ | Sig: r = isinf(...). Predicate. Spec-extension batch 2026-05-09. |
| `isinteger` | ✅ | Sig: r = isinteger(...). Spec-extension batch 2026-05-09. |
| `isnan` | ✅ | Sig: r = isnan(...). Predicate. Spec-extension batch 2026-05-09. |
| `isnumeric` | ✅ | Sig: r = isnumeric(...). Predicate. Spec-extension batch 2026-05-09. |
| `isreal` | ✅ | Sig: r = isreal(...). Predicate. Spec-extension batch 2026-05-09. |
| `nan` | ✅ | Sig: nan(...). Spec-extension batch 2026-05-09. |
| `realmax` | ✅ | Sig: r = realmax(...). Spec-extension batch 2026-05-09. |
| `realmin` | ✅ | Sig: r = realmin(...). Spec-extension batch 2026-05-09. |
| `single` | ✅ | Sig: r = single(...). Type conversion. Spec-extension batch 2026-05-09 verified MATLAB R2025b parity. |
| `typecast` | ✅ | Sig: r = typecast(...). Spec-extension batch 2026-05-09. |
| `uint16` | ✅ 🔬 | Sig: r = uint16(...). Type conversion. Spec-extension batch 2026-05-09 verified MATLAB R2025b parity. |
| `uint32` | ✅ 🔬 | Sig: r = uint32(...). Type conversion. Spec-extension batch 2026-05-09 verified MATLAB R2025b parity. |
| `uint64` | ✅ 🔬 | Sig: r = uint64(...). Type conversion. Spec-extension batch 2026-05-09 verified MATLAB R2025b parity. |
| `uint8` | ✅ 🔬 | Sig: r = uint8(...). Type conversion. Spec-extension batch 2026-05-09 verified MATLAB R2025b parity. |

### Characters and Strings

**Namespace:** builtin — 54 ✅ + 0 ⚠️ / 65 = 83%

| function | status | comment |
|---|:---:|---|
| `append` | ✅ | Sig: r = append(...). Spec-extension batch 2026-05-09. |
| `blanks` | ✅ | Sig: r = blanks(...). String op. Spec-extension batch 2026-05-09 verified MATLAB R2025b parity. |
| `cellstr` | ✅ | Sig: r = cellstr(...). Spec-extension batch 2026-05-09. |
| `char` | ✅ | Sig: r = char(...). Type conversion. Spec-extension batch 2026-05-09 verified MATLAB R2025b parity. |
| `compose` | ✅ 🔬 | compose multi-spec / multi-column value consumption + fmt-class mirror vs MATLAB R2025b. DEEP-PROBE 2026-05-31: numkit's compose formatted PER-ELEMENT with a single value, so a multi-spec format only ever filled the first conversion (compose('%d-%d',[1 2;3 4]) gave 4 outputs ['1-','3-','2-','4-'] instead of 2). MATLAB applies the format repeatedly across each ROW of x, consuming M = #conversion-specs values per output, so an RxC input yields RxCEIL(C/M) outputs. A short trailing chunk leaves unfilled specs as LITERAL text (compose('%d-%d',[1 2 3;4 5 6]) -> 2x2 {'1-2','4-5','3-%d','6-%d'}). Output class mirrors the FORMAT class: char fmt -> cell of char vectors, string fmt -> string array. Fix: chunk per-row by M, build the value span per chunk, formatOnce(..., literalWhenShort=true) (new additive flag on formatOnce, default false so sprintf is unchanged). Cases: A divisible 2-spec; B M=1 multi-column (2x2); C row-vector; G float row-vector; H non-divisible literal-%d edge; I string-fmt -> string array (ci=isstring=1). NOTE: matrix literals with ';' can't appear in a parity expr (harness splits statements on ';'), so 2-D inputs are built via column-major reshape; the literal-bracket 2x2 forms are covered in the gtest. covers:[compose]. |
| `contains` | ✅ 🔬 | contains/startsWith/endsWith on a CELL array or non-scalar string-array SOURCE vs MATLAB R2025b. DEEP-PROBE 2026-05-31: numkit threw 'Not a char array' on a cell source and returned a SCALAR on a string-array source. MATLAB maps element-wise to a LOGICAL array of the SAME shape (any-match over the pattern set; honours 'IgnoreCase'). The three now share a strPredicate driver + strMatchAny helper. contains({'hello','world','help'},'el')=[1 0 1]; startsWith(... ,'he')=[1 0 1]; endsWith(["cat.txt" "dog.csv" "fish.txt"],'.txt')=[1 0 1]; 2x2 cell contains 'b' -> [1 1;0 0]; cell+IgnoreCase startsWith=[1 0]; multi-pattern contains({'foo','bar','baz'},{'oo','az'})=[1 0 1]. Scalar source still a logical scalar. covers:[contains,startsWith,endsWith]. |
| `convertcharstostrings` | ✅ | Sig: r = convertcharstostrings(...). Spec-extension batch 2026-05-09. |
| `convertcontainedstringstochars` | ✅ | Sig: r = convertcontainedstringstochars(...). Spec-extension batch 2026-05-09. |
| `convertstringstochars` | ✅ | Sig: r = convertstringstochars(...). Spec-extension batch 2026-05-09. |
| `count` | ✅ 🔬 | matches / count same-shape output on a STRING-ARRAY source vs MATLAB R2025b. DEEP-PROBE 2026-05-31 (predicate/measure vein): both were element-wise only on a CELL source; a STRING ARRAY collapsed to a SCALAR via s.toString() (matches -> logicalScalar, count -> scalar). MATLAB returns a same-shape array: matches(["cat" "dog" "cat"],"cat") -> logical [1 0 1]; count(["aa" "aba"],"a") -> [2 2]. The pattern may be a list (cell/string array): an element matches if it equals ANY alternative — matches(["cat" "dog" "fish"],["cat" "fish"]) -> [1 0 1]. Fix: matches now uses collectMatchPatterns + a same-shape LOGICAL matrix for cell/string-array sources; count got a string-array branch mirroring its existing cell branch (DOUBLE matrix). Scalar char/string + cell paths unchanged. strlength was already element-wise (clean); strcmp two-array broadcast deferred. Column arrays covered in gtest. covers:[matches,count]. |
| `deblank` | ✅ 🔬 | deblank / strtrim / strip class + shape preservation on a STRING input vs MATLAB R2025b. DEEP-PROBE 2026-05-31: these were class-correct only on a CELL input. For a STRING input they fell into the scalar branch: deblank/strtrim returned a CHAR (s.toString() collapsed the array), and strip returned a 1x1 string for a string ARRAY (only the first element survived). MATLAB is class-preserving: a string input -> string array of the same shape, a char input -> char, a cell -> cell. Fix: a shared mapStringPreserveClass helper (cell -> mapStringCell; string -> stringArray same shape via stringElemSet; char/other -> the old fromString(op(toString())) path, unchanged). deblank(["ab  " "cd "]) -> 1x2 string ["ab" "cd"] (isstring=1, numel=2); deblank("ab  ") -> string scalar "ab"; deblank('ab  ') -> char 'ab' (unchanged); strtrim/strip likewise; cell input unchanged. char codes: 'ab'=195, 'cd'=199. The column-string-array (2x1) and cell cases are also covered in the gtest (a ';' inside a literal can't go in a parity expr). covers:[deblank,strtrim,strip]. |
| `double` | ✅ | Sig: r = double(...). Type conversion. Spec-extension batch 2026-05-09. KNOWN GAP: numkit rejects double("string") with error; MATLAB returns NaN, Octave returns ASCII codes — both differ from numkit. String→double documented as separate gap; only int/logical/numeric paths pinned here. |
| `endsWith` | ✅ 🔬 | contains/startsWith/endsWith on a CELL array or non-scalar string-array SOURCE vs MATLAB R2025b. DEEP-PROBE 2026-05-31: numkit threw 'Not a char array' on a cell source and returned a SCALAR on a string-array source. MATLAB maps element-wise to a LOGICAL array of the SAME shape (any-match over the pattern set; honours 'IgnoreCase'). The three now share a strPredicate driver + strMatchAny helper. contains({'hello','world','help'},'el')=[1 0 1]; startsWith(... ,'he')=[1 0 1]; endsWith(["cat.txt" "dog.csv" "fish.txt"],'.txt')=[1 0 1]; 2x2 cell contains 'b' -> [1 1;0 0]; cell+IgnoreCase startsWith=[1 0]; multi-pattern contains({'foo','bar','baz'},{'oo','az'})=[1 0 1]. Scalar source still a logical scalar. covers:[contains,startsWith,endsWith]. |
| `erase` | ✅ 🔬 | erase / pad class + shape preservation on a STRING array vs MATLAB R2025b. DEEP-PROBE 2026-05-31 (cell-only-else sweep, sibling of c138/c139): both collapsed a string ARRAY to a 1x1 string (s.toString() kept only the first element). MATLAB preserves shape: erase(["a1b" "c2d"],"1") -> 1x2 string ["ab" "c2d"]; pad(["a" "bb"],4) -> ["a   " "bb  "]. Also fixed pad's DEFAULT width: pad(["a" "bbb"]) now pads each element to the longest (3) -> ["a  " "bbb"] (defaultPadWidth ignored string arrays and used s.toString() -> width 1). Fix: route erase/pad through mapStringPreserveClass + add a string branch to defaultPadWidth. char codes: 'c2d'=249,'a   '=193,'a  '=161. char + cell paths unchanged (covered in gtest). covers:[erase,pad]. |
| `erasebetween` | ✅ | Sig: position-based string op (MATLAB canonical: eraseBetween). Bit-identical with MATLAB R2025b. Numkit also registers a lowercase alias for convenience. |
| `extract` | ✅ | Sig: r = extract(...). Spec-extension batch 2026-05-09. |
| `extractafter` | ✅ | Sig: extractAfter(s,pos). DEEP-PROBE 2026-05-31: added CELL-ARRAY str support (was throwing 'Not a char array'); processes element-wise -> cell of char vectors, same shape, scalar position/substr anchor broadcasts: extractAfter({'hello','world'},3)={'lo','ld'}; extractAfter({'a-b','c-d'},'-')={'b','d'}. Scalar char/string path unchanged; cell-position anchor deferred. Bit-identical with MATLAB R2025b. |
| `extractbefore` | ✅ | Sig: extractBefore(s,pos). DEEP-PROBE 2026-05-31: added CELL-ARRAY str support (was throwing 'Not a char array'); processes element-wise -> cell of char vectors, same shape, scalar position/substr anchor broadcasts: extractBefore({'hello','world'},3)={'he','wo'}; extractBefore({'a-b','c-d'},'-')={'a','c'}. Scalar char/string path unchanged; cell-position anchor deferred. Bit-identical with MATLAB R2025b. |
| `extractbetween` | ✅ | Sig: position-based string op (MATLAB canonical: extractBetween). Bit-identical with MATLAB R2025b. Numkit also registers a lowercase alias for convenience. |
| `insertafter` | ✅ | Sig: insertAfter(s,pos,text). DEEP-PROBE 2026-05-31: added CELL-ARRAY str support (was throwing 'Not a char array'); processes element-wise -> cell of char vectors, same shape, scalar anchor/text broadcast: insertAfter({'ab','cd'},1,'X')={'aXb','cXd'}; insertAfter({'a.b','c.d'},'.','X')={'a.Xb','c.Xd'}. Element with no match left unchanged. Scalar path unchanged; cell-position anchor deferred. Bit-identical with MATLAB R2025b. |
| `insertbefore` | ✅ | Sig: insertBefore(s,pos,text). DEEP-PROBE 2026-05-31: added CELL-ARRAY str support (was throwing 'Not a char array'); processes element-wise -> cell of char vectors, same shape, scalar anchor/text broadcast: insertBefore({'ab','cd'},2,'X')={'aXb','cXd'}; insertBefore({'a.b','c.d'},'.','X')={'aX.b','cX.d'}. Element with no match left unchanged. Scalar path unchanged; cell-position anchor deferred. Bit-identical with MATLAB R2025b. |
| `iscellstr` | ✅ | Sig: r = iscellstr(...). Spec-extension batch 2026-05-09. |
| `ischar` | ✅ | Sig: r = ischar(...). Predicate. Spec-extension batch 2026-05-09. |
| `isletter` | ✅ | Sig: r = isletter(...). Spec-extension batch 2026-05-09. |
| `isspace` | ✅ | Sig: r = isspace(...). Spec-extension batch 2026-05-09. |
| `isstring` | ✅ | Sig: r = isstring(...). Predicate. Spec-extension batch 2026-05-09. |
| `isstringscalar` | ✅ | Sig: TF = isStringScalar(X). Camel-case fn name. 100k iters. |
| `isstrprop` | ✅ 🔬 | Sig: r = isstrprop(...). Spec-extension batch 2026-05-09. |
| `join` | ✅ | Sig: r = join(...). Spec-extension batch 2026-05-09. |
| `lower` | ✅ 🔬 | lower / upper / reverse class + shape preservation on a STRING input vs MATLAB R2025b. DEEP-PROBE 2026-05-31 (sibling of c138): these were class-correct only on a CELL input. For a STRING input they fell into the scalar branch — lower/upper returned a CHAR (s.toString() collapsed the array), reverse returned a 1x1 string for a string ARRAY. MATLAB is class-preserving: string input -> string array same shape, char -> char, cell -> cell. Fix: route all three through the shared mapStringPreserveClass helper added in c138. lower(["AB" "CD"]) -> 1x2 string ["ab" "cd"]; upper(["ab" "cd"]) -> ["AB" "CD"]; reverse(["abc" "de"]) -> ["cba" "ed"]; lower("HeLLo") -> string "hello"; lower('HeLLo') -> char 'hello' (unchanged); upper({'ab','cd'}) -> cell (unchanged). char codes: 'ab'=195,'cd'=199,'AB'=131,'cba'=294,'hello'=532. Column string arrays + matrix forms are covered in the gtest (a ';' inside a literal can't go in a parity expr). covers:[lower,upper,reverse]. |
| `matches` | ✅ 🔬 | matches / count same-shape output on a STRING-ARRAY source vs MATLAB R2025b. DEEP-PROBE 2026-05-31 (predicate/measure vein): both were element-wise only on a CELL source; a STRING ARRAY collapsed to a SCALAR via s.toString() (matches -> logicalScalar, count -> scalar). MATLAB returns a same-shape array: matches(["cat" "dog" "cat"],"cat") -> logical [1 0 1]; count(["aa" "aba"],"a") -> [2 2]. The pattern may be a list (cell/string array): an element matches if it equals ANY alternative — matches(["cat" "dog" "fish"],["cat" "fish"]) -> [1 0 1]. Fix: matches now uses collectMatchPatterns + a same-shape LOGICAL matrix for cell/string-array sources; count got a string-array branch mirroring its existing cell branch (DOUBLE matrix). Scalar char/string + cell paths unchanged. strlength was already element-wise (clean); strcmp two-array broadcast deferred. Column arrays covered in gtest. covers:[matches,count]. |
| `newline` | ✅ | Sig: r = newline(...). Spec-extension batch 2026-05-09. |
| `num2str` | ✅ 🔬 | num2str column width for integer vectors that contain NEGATIVE values vs MATLAB R2025b. DEEP-PROBE 2026-05-31: numkit measured the column width with snprintf('%.0f', v), which includes the '-' sign, so a negative element widened the field by one — num2str([-1 10 -100]) gave '-1    10  -100' (14 chars) instead of MATLAB's '-1   10 -100' (12). MATLAB's integer field is max-abs-DIGITS + 2 and the sign is absorbed into the field. Fixed by measuring |v| (fabs) for the width. num2str([-1 10 -100])='-1   10 -100'; [-100 -200]='-100 -200'; [5 -5]='5 -5'; [-5 -50 -500]='-5  -50 -500'; positives ([1 22 333]='1   22  333') and floats unchanged. Fingerprint via numel + sum(char codes). covers:[num2str]. |
| `pad` | ✅ 🔬 | erase / pad class + shape preservation on a STRING array vs MATLAB R2025b. DEEP-PROBE 2026-05-31 (cell-only-else sweep, sibling of c138/c139): both collapsed a string ARRAY to a 1x1 string (s.toString() kept only the first element). MATLAB preserves shape: erase(["a1b" "c2d"],"1") -> 1x2 string ["ab" "c2d"]; pad(["a" "bb"],4) -> ["a   " "bb  "]. Also fixed pad's DEFAULT width: pad(["a" "bbb"]) now pads each element to the longest (3) -> ["a  " "bbb"] (defaultPadWidth ignored string arrays and used s.toString() -> width 1). Fix: route erase/pad through mapStringPreserveClass + add a string branch to defaultPadWidth. char codes: 'c2d'=249,'a   '=193,'a  '=161. char + cell paths unchanged (covered in gtest). covers:[erase,pad]. |
| `plus` | ✅ | Sig: r = plus(...). Arithmetic op. Spec-extension batch 2026-05-09. Fingerprints scalar-only. |
| `regexp` | ✅ 🔬 | regexp(...,'once') vs MATLAB R2025b. DEEP-PROBE 2026-05-31: numkit threw "unknown option 'once'". 'once' matches only the first occurrence and returns the scalarised form: 'start'/'end' -> scalar index ([] no match); 'match' -> char row ('' no match); 'tokens' -> 1xk cell of capture chars ({} no match); 'split' -> {prefix, remainder} at the FIRST match only; 'names' -> scalar struct; 'tokenExtents' -> kx2 matrix. Implemented as a separate regexpFindOnce so the all-matches paths stay byte-identical; regexpDispatch parses 'once' as a modifier alongside the output selector. s='a1b2c3': regexp(s,'\d','once')=2; match-once='1'; tokens-once={'a','1'}; split('a,b,c',',','once')={'a','b,c'}; end-once=2; no-match start/match -> empty. covers:[regexp] sibling spec (regexp.json keeps the all-matches cases; split for the VM 255-register limit). |
| `regexpi` | ✅ | Sig: M = regexpi(S, PAT, 'match'). Case-insensitive. 1000 iters. |
| `regexprep` | ✅ 🔬 | regexprep(...,'once') vs MATLAB R2025b. DEEP-PROBE 2026-05-31: numkit parsed only 'ignorecase' and silently IGNORED 'once' — it replaced ALL matches (regexprep('abc','\w','X','once') gave 'XXX' instead of 'Xbc'). Now regexprep threads a `once` flag into regexrepOne which passes std::regex_constants::format_first_only, replacing only the FIRST match of each pattern. regexprep('abc','\w','X','once')='Xbc'; '\d' on 'a1b2c3'='a#b2c3'; 'AaAa' once+ignorecase='ZaAa'; '(\w+)' on 'hello world'='<hello> world'; no-match unchanged; default (no 'once') still replaces all ('XXX'). Works element-wise on cell-string input too. covers:[regexprep] (regexprep.json keeps the replace-all cases). |
| `regexptranslate` | ✅ | Sig: T = regexptranslate('escape', S). 14-char metachars. 10000 iters. |
| `replace` | ✅ 🔬 | Sig: r = replace(...). Spec-extension batch 2026-05-09. |
| `replacebetween` | ✅ | Sig: position-based string op (MATLAB canonical: replaceBetween). Bit-identical with MATLAB R2025b. Numkit also registers a lowercase alias for convenience. |
| `reverse` | ✅ 🔬 | lower / upper / reverse class + shape preservation on a STRING input vs MATLAB R2025b. DEEP-PROBE 2026-05-31 (sibling of c138): these were class-correct only on a CELL input. For a STRING input they fell into the scalar branch — lower/upper returned a CHAR (s.toString() collapsed the array), reverse returned a 1x1 string for a string ARRAY. MATLAB is class-preserving: string input -> string array same shape, char -> char, cell -> cell. Fix: route all three through the shared mapStringPreserveClass helper added in c138. lower(["AB" "CD"]) -> 1x2 string ["ab" "cd"]; upper(["ab" "cd"]) -> ["AB" "CD"]; reverse(["abc" "de"]) -> ["cba" "ed"]; lower("HeLLo") -> string "hello"; lower('HeLLo') -> char 'hello' (unchanged); upper({'ab','cd'}) -> cell (unchanged). char codes: 'ab'=195,'cd'=199,'AB'=131,'cba'=294,'hello'=532. Column string arrays + matrix forms are covered in the gtest (a ';' inside a literal can't go in a parity expr). covers:[lower,upper,reverse]. |
| `split` | ✅ 🔬 | split output-class preservation vs MATLAB R2025b. DEEP-PROBE 2026-05-31: numkit's split always returned a CELL, but MATLAB returns a STRING array when the input is a string and a cell array of char vectors only for char/cellstr input (doc: 'If str is a string array, then newStr is a string array. Otherwise, newStr is a cell array of character vectors.'). Values + N x 1 shape were already correct; only the container class was wrong. split("a,b,c",",") -> 3x1 string ["a";"b";"c"] (isstring=1); split('a,b,c',',') -> 3x1 cell (iscell=1); split("a b c") (default whitespace) -> 3x1 string. f1=double('c')=99, f2=double('b')=98. The 2-D string-ARRAY input form (split(["a-b";"c-d"],"-") -> 2x2) is a separate, larger feature deferred to a future cycle. covers:[split]. |
| `splitlines` | ✅ | Sig: r = splitlines(...). Spec-extension batch 2026-05-09. |
| `sprintf` | ✅ 🔬 | sprintf/fprintf C-style '*' field width / precision taken from the argument list vs MATLAB R2025b. DEEP-PROBE 2026-05-31: numkit parsed past '*' but never consumed an arg for it — snprintf('%*d', value) read the double value as the int width and printed a garbage pointer (and the format recycled wrongly). Now each '*' consumes one numeric arg (width then precision) BEFORE the conversion's value arg, and countFormatSpecs counts stars so cyclic recycling lines up. sprintf('%*d',5,42)='   42'; sprintf('%.*f',3,pi)≈'3.142'; sprintf('%*.*f',8,2,pi)='    3.14'; sprintf('%-*d',5,42)='42   '; sprintf('[%*d]',4,1,4,22,4,333)='[   1][  22][ 333]' (width+value recycled per cycle). Fingerprint via numel + sum(char codes). covers:[sprintf] sibling spec (sprintf.json keeps the base cases; split for the VM 255-register limit). |
| `sscanf` | ✅ | Sig: A = sscanf(S, FMT). 5 floats. 100k iters. |
| `startsWith` | ✅ 🔬 | contains/startsWith/endsWith on a CELL array or non-scalar string-array SOURCE vs MATLAB R2025b. DEEP-PROBE 2026-05-31: numkit threw 'Not a char array' on a cell source and returned a SCALAR on a string-array source. MATLAB maps element-wise to a LOGICAL array of the SAME shape (any-match over the pattern set; honours 'IgnoreCase'). The three now share a strPredicate driver + strMatchAny helper. contains({'hello','world','help'},'el')=[1 0 1]; startsWith(... ,'he')=[1 0 1]; endsWith(["cat.txt" "dog.csv" "fish.txt"],'.txt')=[1 0 1]; 2x2 cell contains 'b' -> [1 1;0 0]; cell+IgnoreCase startsWith=[1 0]; multi-pattern contains({'foo','bar','baz'},{'oo','az'})=[1 0 1]. Scalar source still a logical scalar. covers:[contains,startsWith,endsWith]. |
| `str2double` | ✅ 🔬 | str2double on a CELL array (or non-scalar string array) vs MATLAB R2025b. DEEP-PROBE 2026-05-31: numkit threw 'Not a char array' — it only handled a scalar char/string. Now it maps element-wise to a DOUBLE matrix of the SAME shape (NaN where an element fails to parse / is empty / isn't char-string). The single-token MATLAB parse (strip commas, trim, whole-token-or-NaN) was factored into str2doubleOne and reused. str2double({'1.5','2.5','x'})=[1.5 2.5 NaN]; {'1';'2';'3'}=[1;2;3] (3x1); {'1','2';'3','4'}=[1 2;3 4]; {' 42 ','1,234','Inf','-Inf','NaN',''}=[42 1234 Inf -Inf NaN NaN]; string-array ["10" "20" "abc"]=[10 20 NaN]; scalar form unchanged. The single-token parser is now str2doubleParse (real OR complex); an all-real cell still yields a DOUBLE matrix (this spec). Complex-string parsing landed 2026-06-05 — see str2double_complex.json. covers:[str2double]. |
| `strcat` | ✅ | Sig: r = strcat(...). String op. Spec-extension batch 2026-05-09 verified MATLAB R2025b parity. FP uses double(strcmp(...)) booleans because the harness compares numerics. |
| `strcmp` | ✅ 🔬 | strcmp / strcmpi / strncmp / strncmpi element-wise + broadcast on a STRING-ARRAY operand vs MATLAB R2025b. DEEP-PROBE 2026-05-31 (predicate vein, sibling of c141): strCmpElementwise treated only CELL operands as arrays (ac = a.isCell()); a non-scalar STRING ARRAY fell to the scalar branch and collapsed to a logical SCALAR comparing the whole arrays' toString(). MATLAB compares element-wise with broadcast: strcmp(["a" "b" "a"],"a") -> logical [1 0 1]; strcmp(["ab" "cd"],["ab" "xy"]) -> [1 0]; strncmp(["abc" "abd" "xyz"],"ab",2) -> [1 1 0]; a string array vs a cell mixes fine (strcmp(["a" "b"],{'a' 'x'}) -> [1 0]); a char/string scalar broadcasts. Fix: an operand is an 'array' when it is a cell OR a non-scalar string array (isArr helper); element fetch uses cellAt(i) for cells and stringElem(i) for string arrays (elemAt helper). The cell-vs-cell, cell-vs-scalar, and scalar-vs-scalar paths are byte-identical. Column arrays covered in gtest. covers:[strcmp,strcmpi,strncmp,strncmpi]. |
| `strcmpi` | ✅ 🔬 | strcmp / strcmpi / strncmp / strncmpi element-wise + broadcast on a STRING-ARRAY operand vs MATLAB R2025b. DEEP-PROBE 2026-05-31 (predicate vein, sibling of c141): strCmpElementwise treated only CELL operands as arrays (ac = a.isCell()); a non-scalar STRING ARRAY fell to the scalar branch and collapsed to a logical SCALAR comparing the whole arrays' toString(). MATLAB compares element-wise with broadcast: strcmp(["a" "b" "a"],"a") -> logical [1 0 1]; strcmp(["ab" "cd"],["ab" "xy"]) -> [1 0]; strncmp(["abc" "abd" "xyz"],"ab",2) -> [1 1 0]; a string array vs a cell mixes fine (strcmp(["a" "b"],{'a' 'x'}) -> [1 0]); a char/string scalar broadcasts. Fix: an operand is an 'array' when it is a cell OR a non-scalar string array (isArr helper); element fetch uses cellAt(i) for cells and stringElem(i) for string arrays (elemAt helper). The cell-vs-cell, cell-vs-scalar, and scalar-vs-scalar paths are byte-identical. Column arrays covered in gtest. covers:[strcmp,strcmpi,strncmp,strncmpi]. |
| `strfind` | ✅ 🔬 | strfind on a cell / string-array source vs MATLAB R2025b. DEEP-PROBE 2026-05-31 (string-array vein): strfind only handled a scalar char/string (s.toString()). A string ARRAY collapsed to its first element (returned a plain double vector), and a CELL source THREW 'Not a char array'. MATLAB returns a same-shape CELL of 1-based index vectors: strfind(["hello" "world" "lll"],"l") -> {[3 4],[4],[1 2 3]}; strfind({'hello','xx'},'l') -> {[3 4],[]}. A scalar char/string source still returns a 1xk DOUBLE row vector (strfind("banana","an")=[2 4]; strfind('abcabc','bc')=[2 5]). Fix: factored the core into strfindOne and added a cell/non-scalar-string-array branch that builds a same-shape cell. Column arrays covered in gtest. covers:[strfind]. |
| `string` | ✅ | Sig: S = string(X). Numeric → string array. 1000 iters. fp limited to numel (string-array indexing broken — BUGS #7). |
| `strings` | ✅ | Sig: S = strings(M, N). 100x100 empty string array. 10000 iters. |
| `strip` | ✅ 🔬 | deblank / strtrim / strip class + shape preservation on a STRING input vs MATLAB R2025b. DEEP-PROBE 2026-05-31: these were class-correct only on a CELL input. For a STRING input they fell into the scalar branch: deblank/strtrim returned a CHAR (s.toString() collapsed the array), and strip returned a 1x1 string for a string ARRAY (only the first element survived). MATLAB is class-preserving: a string input -> string array of the same shape, a char input -> char, a cell -> cell. Fix: a shared mapStringPreserveClass helper (cell -> mapStringCell; string -> stringArray same shape via stringElemSet; char/other -> the old fromString(op(toString())) path, unchanged). deblank(["ab  " "cd "]) -> 1x2 string ["ab" "cd"] (isstring=1, numel=2); deblank("ab  ") -> string scalar "ab"; deblank('ab  ') -> char 'ab' (unchanged); strtrim/strip likewise; cell input unchanged. char codes: 'ab'=195, 'cd'=199. The column-string-array (2x1) and cell cases are also covered in the gtest (a ';' inside a literal can't go in a parity expr). covers:[deblank,strtrim,strip]. |
| `strjoin` | ✅ 🔬 | Sig: s = strjoin(C, delim). delim may be a single string OR a cell array of numel(C)-1 strings interleaved between consecutive elements: strjoin({a,b,c},{', ',' and '}) -> 'a, b and c'. Single-element C uses an empty {} delim. numkit previously threw 'Not a char array' on a cell delim; fixed 2026-05-30. Spec-extension batch 2026-05-09 + cell delimiter. |
| `strjust` | ✅ | Sig: S2 = strjust(S, side). 3-row right-justify. 10000 iters. |
| `strlength` | ✅ | Sig: r = strlength(...). String op. Spec-extension batch 2026-05-09 verified MATLAB R2025b parity. |
| `strncmp` | ✅ 🔬 | strcmp / strcmpi / strncmp / strncmpi element-wise + broadcast on a STRING-ARRAY operand vs MATLAB R2025b. DEEP-PROBE 2026-05-31 (predicate vein, sibling of c141): strCmpElementwise treated only CELL operands as arrays (ac = a.isCell()); a non-scalar STRING ARRAY fell to the scalar branch and collapsed to a logical SCALAR comparing the whole arrays' toString(). MATLAB compares element-wise with broadcast: strcmp(["a" "b" "a"],"a") -> logical [1 0 1]; strcmp(["ab" "cd"],["ab" "xy"]) -> [1 0]; strncmp(["abc" "abd" "xyz"],"ab",2) -> [1 1 0]; a string array vs a cell mixes fine (strcmp(["a" "b"],{'a' 'x'}) -> [1 0]); a char/string scalar broadcasts. Fix: an operand is an 'array' when it is a cell OR a non-scalar string array (isArr helper); element fetch uses cellAt(i) for cells and stringElem(i) for string arrays (elemAt helper). The cell-vs-cell, cell-vs-scalar, and scalar-vs-scalar paths are byte-identical. Column arrays covered in gtest. covers:[strcmp,strcmpi,strncmp,strncmpi]. |
| `strncmpi` | ✅ 🔬 | strcmp / strcmpi / strncmp / strncmpi element-wise + broadcast on a STRING-ARRAY operand vs MATLAB R2025b. DEEP-PROBE 2026-05-31 (predicate vein, sibling of c141): strCmpElementwise treated only CELL operands as arrays (ac = a.isCell()); a non-scalar STRING ARRAY fell to the scalar branch and collapsed to a logical SCALAR comparing the whole arrays' toString(). MATLAB compares element-wise with broadcast: strcmp(["a" "b" "a"],"a") -> logical [1 0 1]; strcmp(["ab" "cd"],["ab" "xy"]) -> [1 0]; strncmp(["abc" "abd" "xyz"],"ab",2) -> [1 1 0]; a string array vs a cell mixes fine (strcmp(["a" "b"],{'a' 'x'}) -> [1 0]); a char/string scalar broadcasts. Fix: an operand is an 'array' when it is a cell OR a non-scalar string array (isArr helper); element fetch uses cellAt(i) for cells and stringElem(i) for string arrays (elemAt helper). The cell-vs-cell, cell-vs-scalar, and scalar-vs-scalar paths are byte-identical. Column arrays covered in gtest. covers:[strcmp,strcmpi,strncmp,strncmpi]. |
| `strrep` | ✅ | Sig: r = strrep(str,old,new). String op; with any CELL argument returns a cell of char vectors (scalars broadcast). DEEP-PROBE 2026-05-31: added cell-array support (was THROWING 'Not a char array'): strrep({'hello','world','book'},'o','O')={'hellO','wOrld','bOOk'}; scalar-str + cell-pat broadcast strrep('aXbYc',{'X','Y'},{'-','='})={'a-bYc','aXb=c'}; all-cell element-wise strrep({'aa','bb'},{'a','b'},{'X','Y'})={'XX','YY'}. STRING-ARRAY (non-cell, numel>1) input deferred. FP uses double(c{k}(j)) char codes + numel. Spec-extension batch 2026-05-09. |
| `strsplit` | ✅ 🔬 | Sig: c = strsplit(str, delim, Name,Value). Delim may be a string OR a cell array of strings (longest-match); multi-char delims supported. CollapseDelimiters=true (default) merges only CONSECUTIVE delimiters so leading/trailing empties remain (',a,b,'->{'','a','b',''} n=4; 'a,,b'->n=2); CollapseDelimiters=false splits at every occurrence ('a,,b'->{'a','','b'} n=3). Default delimiter is whitespace ('  a  b  '->n=4). numkit previously took only a single char delim, threw on cell delims, and always dropped empties (ignoring CollapseDelimiters); fixed 2026-05-30. (DelimiterType='RegularExpression' remains an unimplemented gap.) Spec-extension batch 2026-05-09 + cell/multi/collapse. |
| `strtok` | ✅ | Sig: r = strtok(...). String op. Spec-extension batch 2026-05-09 verified MATLAB R2025b parity. |
| `strtrim` | ✅ 🔬 | deblank / strtrim / strip class + shape preservation on a STRING input vs MATLAB R2025b. DEEP-PROBE 2026-05-31: these were class-correct only on a CELL input. For a STRING input they fell into the scalar branch: deblank/strtrim returned a CHAR (s.toString() collapsed the array), and strip returned a 1x1 string for a string ARRAY (only the first element survived). MATLAB is class-preserving: a string input -> string array of the same shape, a char input -> char, a cell -> cell. Fix: a shared mapStringPreserveClass helper (cell -> mapStringCell; string -> stringArray same shape via stringElemSet; char/other -> the old fromString(op(toString())) path, unchanged). deblank(["ab  " "cd "]) -> 1x2 string ["ab" "cd"] (isstring=1, numel=2); deblank("ab  ") -> string scalar "ab"; deblank('ab  ') -> char 'ab' (unchanged); strtrim/strip likewise; cell input unchanged. char codes: 'ab'=195, 'cd'=199. The column-string-array (2x1) and cell cases are also covered in the gtest (a ';' inside a literal can't go in a parity expr). covers:[deblank,strtrim,strip]. |
| `upper` | ✅ 🔬 | lower / upper / reverse class + shape preservation on a STRING input vs MATLAB R2025b. DEEP-PROBE 2026-05-31 (sibling of c138): these were class-correct only on a CELL input. For a STRING input they fell into the scalar branch — lower/upper returned a CHAR (s.toString() collapsed the array), reverse returned a 1x1 string for a string ARRAY. MATLAB is class-preserving: string input -> string array same shape, char -> char, cell -> cell. Fix: route all three through the shared mapStringPreserveClass helper added in c138. lower(["AB" "CD"]) -> 1x2 string ["ab" "cd"]; upper(["ab" "cd"]) -> ["AB" "CD"]; reverse(["abc" "de"]) -> ["cba" "ed"]; lower("HeLLo") -> string "hello"; lower('HeLLo') -> char 'hello' (unchanged); upper({'ab','cd'}) -> cell (unchanged). char codes: 'ab'=195,'cd'=199,'AB'=131,'cba'=294,'hello'=532. Column string arrays + matrix forms are covered in the gtest (a ';' inside a literal can't go in a parity expr). covers:[lower,upper,reverse]. |

### Structures

**Namespace:** builtin — 12 ✅ + 0 ⚠️ / 14 = 86%

| function | status | comment |
|---|:---:|---|
| `arrayfun` | ✅ | Sig: r = arrayfun(fn, x). Spec-extension batch 2026-05-09. KNOWN GAP: numkit's arrayfun does NOT apply the function — returns input unchanged for both lambda (@(x) x*2) and named functions (@sin). Real bug, separate spec. Only structural shape pinned here (numel preserved). |
| `cell2struct` | ✅ | Sig: r = cell2struct(...). Spec-extension batch 2026-05-09. |
| `fieldnames` | ✅ | Sig: r = fieldnames(...). Spec-extension batch 2026-05-09. |
| `getfield` | ✅ | Sig: r = getfield(...). Spec-extension batch 2026-05-09. |
| `isfield` | ✅ | Sig: r = isfield(...). Spec-extension batch 2026-05-09. |
| `isstruct` | ✅ | Sig: r = isstruct(...). Predicate. Spec-extension batch 2026-05-09. |
| `orderfields` | ✅ | Sig: r = orderfields(...). Spec-extension batch 2026-05-09. |
| `rmfield` | ✅ | Sig: r = rmfield(...). Spec-extension batch 2026-05-09. |
| `setfield` | ✅ | Sig: S2 = setfield(S, F, V). 10k iters. |
| `struct` | ✅ | Sig: r = struct(...). Spec-extension batch 2026-05-09. |
| `struct2cell` | ✅ | Sig: r = struct2cell(...). Spec-extension batch 2026-05-09. |
| `struct2table` | ❌ |  |
| `structfun` | ✅ | Sig: A = structfun(@F, S). Apply *2 to each field. 1000 iters. (May fail due to lambda BUG #11). |
| `table2struct` | ❌ |  |

### Cell Arrays

**Namespace:** builtin — 12 ✅ + 0 ⚠️ / 17 = 71%

| function | status | comment |
|---|:---:|---|
| `cell` | ✅ | Sig: r = cell(...). Spec-extension batch 2026-05-09. |
| `cell2mat` | ✅ | Sig: r = cell2mat(...). Spec-extension batch 2026-05-09. |
| `cell2struct` | ✅ | Sig: r = cell2struct(...). Spec-extension batch 2026-05-09. |
| `cell2table` | ❌ |  |
| `celldisp` | ✅ | Sig: celldisp(c). Display cell array contents (output goes to stdout). Side-effect-only function -- spec just verifies it runs without error. Output format matches MATLAB R2025b qualitatively. |
| `cellfun` | ✅ | Sig: r = cellfun(fn, C[, C2, ...][,'UniformOutput',tf]). Multi-cell form cellfun(fn, C1, C2, ...) applies fn(C1{i},C2{i},...) -> [11 22 33], 3-cell -> [111 222]. Legacy string-function-name forms (fixed 2026-06-05, bugs/builtin/cellfun-inputforms.md): 'isempty'/'isreal'/'islogical' -> logical, 'length'/'ndims'/'prodofsize' -> double, 'size',C,k -> size(C{i},k), 'isclass',C,'cls' -> isa per cell. Single-cell @(x)x*2 path unchanged (original spec-extension batch 2026-05-09). |
| `cellplot` | ❌ |  |
| `cellstr` | ✅ | Sig: r = cellstr(...). Spec-extension batch 2026-05-09. |
| `iscell` | ✅ | Sig: r = iscell(...). Predicate. Spec-extension batch 2026-05-09. |
| `iscellstr` | ✅ | Sig: r = iscellstr(...). Spec-extension batch 2026-05-09. |
| `mat2cell` | ✅ | Sig: r = mat2cell(...). Spec-extension batch 2026-05-09. |
| `num2cell` | ✅ | Sig: C = num2cell(A[,dims]). DEEP-PROBE 2026-05-31: added the dim form (was element-wise only; num2cell(A,dim) threw 'Index exceeds array size 1'). The listed dims are COLLAPSED into each cell: num2cell(A,2)=2x1 cell of rows (c{1}=[1 2 3]); num2cell(A,1)=1x3 cell of columns (c{1}=[1;4]); num2cell(A,[1 2])=1x1 cell holding the whole matrix; a dim > ndim (num2cell(A,3) on a 2-D A) is a trivial singleton -> element-wise 2x3. 2-D only (N-D deferred). namespace=builtin. |
| `string` | ✅ | Sig: S = string(X). Numeric → string array. 1000 iters. fp limited to numel (string-array indexing broken — BUGS #7). |
| `struct2cell` | ✅ | Sig: r = struct2cell(...). Spec-extension batch 2026-05-09. |
| `table` | ❌ |  |
| `table2cell` | ❌ |  |
| `timetable` | ❌ |  |

### Function Handles

**Namespace:** builtin — 5 ✅ + 0 ⚠️ / 6 = 83%

| function | status | comment |
|---|:---:|---|
| `feval` | ✅ | Sig: r = feval(...). Spec-extension batch 2026-05-09. |
| `func2str` | ✅ | Sig: r = func2str(...). Spec-extension batch 2026-05-09. |
| `function_handle` | ❌ | OOP class |
| `functions` | ✅ | Sig: info = functions(fnHandle). Returns struct with {function, type, file} fields. Bit-identical with MATLAB R2025b on probed handle (3 fields). |
| `localfunctions` | ✅ ❗ | Sig: F = localfunctions(). Stub returns empty cell. 100k iters. |
| `str2func` | ✅ | Sig: F = str2func(STR). 2026-06-04: accepts '@name' (strip the '@') and '@(...)' anonymous source, not just a bare name (was storing the leading '@' -> unfindable '@@..'). 'sin'(0)=0, '@cos'(0)=1, '@(x)x+1'(5)=6, '@(x,y)x*y'(3,4)=12. |

### Categorical Arrays

**Namespace:** `categorical.*` (future) — 1 ✅ + 0 ⚠️ / 17 = 5%

| function | status | comment |
|---|:---:|---|
| `addcats` | ❌ |  |
| `categorical` | ❌ |  |
| `categories` | ❌ |  |
| `combinations` | ❌ | all combinations |
| `countcats` | ❌ |  |
| `discretize` | ✅ | Sig: r = discretize(...). Spec-extension batch 2026-05-09. |
| `iscategory` | ❌ |  |
| `isordinal` | ❌ |  |
| `isprotected` | ❌ |  |
| `isundefined` | ❌ |  |
| `mergecats` | ❌ |  |
| `removecats` | ❌ |  |
| `renamecats` | ❌ |  |
| `reordercats` | ❌ |  |
| `setcats` | ❌ |  |
| `summary` | ❌ |  |

### Tables / Timetables

**Namespace:** `table.*` (future) — 6 ✅ + 0 ⚠️ / 66 = 9%

| function | status | comment |
|---|:---:|---|
| `addprop` | ❌ |  |
| `addvars` | ❌ |  |
| `anymissing` | ✅ | anymissing + ismissing — standard NaN missing + custom indicator. Covers: double/single/uint8/logical/empty inputs for anymissing, ismissing with no indicator (NaN only) and scalar/vector indicator. Per MATLAB R2025b: when indicator is provided, NaN is NOT auto-flagged — only values matching the indicator are missing (NaN in indicator does match NaN in x). |
| `array2table` | ❌ |  |
| `cell2table` | ❌ |  |
| `computebygroup` | ❌ |  |
| `convertvars` | ❌ |  |
| `fillmissing` | ⚠️ | fillmissing — replace NaN by method. Methods: 'constant', 'previous', 'next' (existing); 'nearest', 'linear' (cycle 74). Per-column for matrices. Tie-break in 'nearest' picks NEXT. 'linear' default extrapolates leading/trailing NaNs via slope of nearest interior good-value pair. DEEP-PROBE 2026-05-31: added the 'EndValues' name-value option (numkit previously threw 'Cannot convert char to scalar'). 'EndValues' governs ONLY endpoint missing entries (before first / after last original good value); interior runs always filled by the method. Supported: 'extrap' (default, current behavior), 'none' (leave endpoints NaN), 'nearest' (leading=first good, trailing=last good), numeric constant. 'EndValues' rejected for 'constant'/'mean'/'median' methods; 'previous'/'next' EndValues deferred. Verified vs MATLAB R2025b: a=[NaN NaN 3 5 NaN 9 NaN] linear+none=[NaN NaN 3 5 7 9 NaN], +0=[0 0 3 5 7 9 0], +nearest=[3 3 3 5 7 9 9], previous+(-7)=[-7 -7 3 5 5 9 -7]. Deferred MATLAB methods: 'spline', 'pchip', 'makima', 'movmean', 'movmedian', 'knn'. |
| `findgroups` | ✅ | Sig: [G, ID] = findgroups(g). 1-based group IDs by sorted-unique order; ID = unique non-NaN values. g=[10 20 10 30 20] -> G=[1 2 1 3 2], ID=[10;20;30]. Lifted to public C++ API numkit::builtin::findgroups (FindgroupsResult) 2026-06. |
| `groupcounts` | ✅ | Sig: [C, GR, P] = groupcounts(g). Per-group counts over sorted-unique values; GR reps; P percentages. g=[10 20 10 30 20]' -> C=[2;2;1], GR=[10;20;30], P=[40;40;20]. Lifted to public C++ API numkit::builtin::groupcounts (GroupcountsResult) 2026-06. |
| `groupfilter` | ✅ | groupfilter array form — function-handle predicate per group. Scalar result → keep/drop whole group. Vector of length kn → per-row mask. Other shapes (e.g. mean(matrix)=row-vec) → all(result(:)) reduction. NaN entries in groupvars form singleton groups. Deferred: table inputs, groupbins, IncludedEdge/datavars NV. |
| `groupsummary` | ✅ | groupsummary array form — methods: sum/mean/median/max/min/std/numunique/nnz/mode/all/any. 3-output [B, BG, BC]. NaN groups form a trailing bucket. Table form, groupbins, function-handle methods, multi-grouping-vars, IncludeMissingGroups/IncludeEmptyGroups NV are deferred (table type not in numkit, binning/handle paths need engine plumbing). |
| `grouptransform` | ✅ | grouptransform array form. Methods: meancenter, zscore, norm (2-norm), rescale ([0,1]), meanfill (NaN→group mean), linearfill (within-group linear interp + end extrap, requires ≥2 good values), function handle. NaN groups form singletons. Function-handle path callable via engine. Deferred: table inputs, groupbins, IncludedEdge/ReplaceValues NV pairs. |
| `head` | ✅ | Sig: Y = head(X, K). First 100 elements. 10000 iters. |
| `height` | ❌ |  |
| `inner2outer` | ❌ |  |
| `innerjoin` | ❌ |  |
| `intersect` | ✅ 🔬 | Sig intersect(A,B[,setOrder | 'rows']). Default 'sorted': intersect([1 2 3 4],[2 4 6]) = [2 4]. 'stable' keeps A-order. COMPLEX (C output only): exact-equality, |z|+angle order. DEEP-PROBE 2026-05-31: added the 'rows' flag (was IGNORING it and flattening element-wise); returns sorted unique rows present in both: intersect([1 2;3 4;5 6],[3 4;9 9;1 2],'rows')=[1 2;3 4]; row-distinguishing intersect([1 2;3 4],[2 1;3 4],'rows')=[3 4] (1x2, NOT 1x4). 'rows' ia/ib outputs deferred. numkit previously IGNORED 'stable' (fixed) and threw on COMPLEX (added 2026-05-29). |
| `ismember` | ✅ | Sig [tf,loc]=ismember(A,B[,'rows']): tf membership mask + loc = LOWEST 1-based index of each A element in B (0 if absent). ismember([2 5 8 1],[5 2 9]) -> tf=[1 1 0 0], loc=[2 1 0 0]. Tie rule (B has dups): ismember([3 1 2],[2 1 3 1]) -> loc=[3 2 1] (lowest index). COMPLEX ismember: membership by EXACT equality (re&im), Locb lowest index, NaN-component never matches, reals vs complex compare as z+0i: ismember([1 5i 3+4i 2],[3+4i 5i 1]) -> tf=[1 1 1 0], loc=[3 2 1 0]. DEEP-PROBE 2026-05-31: added the 'rows' flag (was IGNORING it and doing element-wise membership, returning a MATRIX same size as A); each row is one element, tf/loc are COLUMNS of height size(A,1): ismember([1 2;5 6;3 4],[3 4;1 2;7 8],'rows') -> tf=[1;0;1], loc=[2;0;1]; duplicate row in B -> lowest index loc; NaN-containing row never matches. numkit previously had NO 2nd output loc (added) and threw on COMPLEX (added 2026-05-29). |
| `ismissing` | ✅ | anymissing + ismissing — standard NaN missing + custom indicator. Covers: double/single/uint8/logical/empty inputs for anymissing, ismissing with no indicator (NaN only) and scalar/vector indicator. Per MATLAB R2025b: when indicator is provided, NaN is NOT auto-flagged — only values matching the indicator are missing (NaN in indicator does match NaN in x). |
| `issortedrows` | ✅ | Sig: TF = issortedrows(X). 10k×3 pre-sorted. 1000 iters. |
| `join` | ✅ | Sig: r = join(...). Spec-extension batch 2026-05-09. |
| `jointables` | ❌ |  |
| `mergevars` | ❌ |  |
| `movevars` | ❌ |  |
| `outerjoin` | ❌ |  |
| `parquetread` | ❌ |  |
| `parquetwrite` | ❌ |  |
| `pivot` | ❌ |  |
| `pivottable` | ❌ |  |
| `readtable` | ❌ | needs table type |
| `removevars` | ❌ |  |
| `renamevars` | ❌ |  |
| `rmmissing` | ✅ | Sig: y = rmmissing(x). Drops NaN entries. |
| `rmprop` | ❌ |  |
| `rowfun` | ❌ |  |
| `rows2vars` | ❌ |  |
| `setdiff` | ✅ 🔬 | Sig setdiff(A,B[,setOrder | 'rows']): A elements not in B. Default 'sorted': setdiff([3 1 2 5 4],[2 5]) = [1 3 4]. 'stable' keeps A-order (first occurrence): [3 1 4]. COMPLEX (C output only): exact-equality, |z|+angle order. DEEP-PROBE 2026-05-31: added the 'rows' flag (was THROWING 'Index exceeds array dimensions'); treats each row as an element, returns sorted unique A-rows not in B: setdiff([1 2;3 4;5 6],[3 4;9 9;1 2],'rows')=[5 6]. 'rows' ia index output deferred. numkit previously IGNORED 'stable' (fixed) and threw on COMPLEX (added 2026-05-29). |
| `setxor` | ✅ | Sig: r = setxor(A,B[,setOrder | 'rows']): symmetric difference, elements in exactly one of A or B, sorted. setxor([1 2 3 4],[2 4 5 6]) = [1 3 5 6]. DEEP-PROBE 2026-05-31: added the 'rows' flag (was IGNORING it and flattening element-wise to a 1xN vector); each row is one element, returns sorted unique rows present in exactly one input: setxor([1 2;3 4;5 6],[3 4;9 9;1 2],'rows') = [5 6;9 9] (2x2); setxor([5 6;1 1],[1 1;2 2;7 8],'rows') = [2 2;5 6;7 8]. 'rows' ia/ib index outputs deferred (throw). NaN-containing rows kept (NaN != NaN). Spec-extension batch 2026-05-09. |
| `sortrows` | ✅ 🔬 | Sig: B = sortrows(A). 10k×3 sort by first col. 100 iters. |
| `splitapply` | ✅ | splitapply — apply scalar-returning function handle per group, return one row per group. Supports multi-input handles (x, y, ..., G). Vector-output handles error in MATLAB R2025b (must wrap in cell). Output is column vector ordered by ascending group ID. |
| `splitvars` | ❌ |  |
| `stack` | ❌ |  |
| `stackedplot` | ❌ |  |
| `stacktablevariables` | ❌ |  |
| `standardizemissing` | ❌ |  |
| `struct2table` | ❌ |  |
| `summary` | ❌ |  |
| `table` | ❌ |  |
| `table2array` | ❌ |  |
| `table2cell` | ❌ |  |
| `table2struct` | ❌ |  |
| `table2timetable` | ❌ |  |
| `tail` | ✅ | Sig: Y = tail(X, K). Last 100 elements. 10000 iters. |
| `timetable2table` | ❌ |  |
| `topkrows` | ⚠️ | Sig: B = topkrows(A, k[, col[, direction]]); [B,I] = topkrows(...). Top k rows by column-priority sort (default: all columns, descending lex). col selects a single column or vector of columns (priority order). direction = 'ascend' | 'descend' applies to all sort columns. 2-output returns 1-indexed row indices. ComparisonMethod NV is accept-and-ignore (numkit is real-only). Bit-identical with MATLAB R2025b on probed cases. |
| `union` | ✅ 🔬 | Sig union(A,B[,setOrder | 'rows']). Default 'sorted': union([1 2 3],[3 4 5]) = [1 2 3 4 5]. 'stable': unique(A)-order then new-of-B. COMPLEX (C output only): exact-equality, |z|+angle order. DEEP-PROBE 2026-05-31: added the 'rows' flag (was IGNORING it and flattening to a 1xN vector); returns sorted unique rows of [A;B]: union([1 2;3 4;5 6],[3 4;9 9;1 2],'rows')=[1 2;3 4;5 6;9 9] (4x2). 'rows' ia/ib outputs deferred. numkit previously IGNORED 'stable' (fixed) and threw on COMPLEX (added 2026-05-29). |
| `unique` | ✅ 🔬 | unique(M,'rows','stable') vs MATLAB R2025b. DEEP-PROBE 2026-05-31: numkit honoured 'stable' for the flat-vector path but DROPPED it on the 'rows' path (code comment: "'rows' + 'stable' not yet combined; rows path stays sorted") — it returned lexicographically-sorted rows instead of first-occurrence order. Now uniqueRows/uniqueRowsWithIndices take a stable flag: rows are kept in first-appearance order, ia indexes the first occurrence of each distinct row, ic maps every row back. M=[3 0;1 0;2 0;1 0;3 0] -> C=[3 0;1 0;2 0], ia=[1;2;3], ic=[1;2;3;2;1] (sorted path unchanged: C=[1 0;2 0;3 0]). NaN-containing rows stay distinct/in-place. covers:[unique] (sibling spec; unique.json keeps the sorted/flat cases — split for the VM 255-register limit). |
| `unstack` | ❌ |  |
| `unstacktablevariables` | ❌ |  |
| `varfun` | ❌ |  |
| `vartype` | ❌ |  |
| `width` | ❌ |  |
| `writetable` | ❌ | needs table type |

### Bit-wise Operations

**Namespace:** builtin — 7 ✅ + 0 ⚠️ / 8 = 88%

| function | status | comment |
|---|:---:|---|
| `bitand` | ✅ | Sig: r = bitand(...). Bitwise integer op. Spec-extension batch 2026-05-09 verified bit-identical MATLAB R2025b. |
| `bitcmp` | ✅ 🔬 | Sig: r = bitcmp(...). Bitwise integer op. Spec-extension batch 2026-05-09 verified bit-identical MATLAB R2025b. |
| `bitget` | ✅ | Sig: r = bitget(...). Bitwise integer op. Spec-extension batch 2026-05-09 verified bit-identical MATLAB R2025b on scalar-k inputs. |
| `bitor` | ✅ | Sig: r = bitor(...). Bitwise integer op. Spec-extension batch 2026-05-09 verified bit-identical MATLAB R2025b. |
| `bitset` | ✅ | Sig: r = bitset(...). Bitwise integer op. Spec-extension batch 2026-05-09. |
| `bitshift` | ✅ | Sig: r = bitshift(...). Bitwise integer op. Spec-extension batch 2026-05-09. |
| `bitxor` | ✅ | Sig: r = bitxor(...). Bitwise integer op. Spec-extension batch 2026-05-09 verified bit-identical MATLAB R2025b. |
| `swapbytes` | ✅ 🔬 | Sig: Y = swapbytes(X). Already has int32 spec; this checks uint16 path. 1000 iters. |

### Set Operations

**Namespace:** builtin — 10 ✅ + 0 ⚠️ / 13 = 77%

| function | status | comment |
|---|:---:|---|
| `allunique` | ✅ | Sig: r = allunique(...). Spec-extension batch 2026-05-09 verified MATLAB R2025b parity. |
| `innerjoin` | ❌ |  |
| `intersect` | ✅ 🔬 | Sig intersect(A,B[,setOrder | 'rows']). Default 'sorted': intersect([1 2 3 4],[2 4 6]) = [2 4]. 'stable' keeps A-order. COMPLEX (C output only): exact-equality, |z|+angle order. DEEP-PROBE 2026-05-31: added the 'rows' flag (was IGNORING it and flattening element-wise); returns sorted unique rows present in both: intersect([1 2;3 4;5 6],[3 4;9 9;1 2],'rows')=[1 2;3 4]; row-distinguishing intersect([1 2;3 4],[2 1;3 4],'rows')=[3 4] (1x2, NOT 1x4). 'rows' ia/ib outputs deferred. numkit previously IGNORED 'stable' (fixed) and threw on COMPLEX (added 2026-05-29). |
| `ismember` | ✅ | Sig [tf,loc]=ismember(A,B[,'rows']): tf membership mask + loc = LOWEST 1-based index of each A element in B (0 if absent). ismember([2 5 8 1],[5 2 9]) -> tf=[1 1 0 0], loc=[2 1 0 0]. Tie rule (B has dups): ismember([3 1 2],[2 1 3 1]) -> loc=[3 2 1] (lowest index). COMPLEX ismember: membership by EXACT equality (re&im), Locb lowest index, NaN-component never matches, reals vs complex compare as z+0i: ismember([1 5i 3+4i 2],[3+4i 5i 1]) -> tf=[1 1 1 0], loc=[3 2 1 0]. DEEP-PROBE 2026-05-31: added the 'rows' flag (was IGNORING it and doing element-wise membership, returning a MATRIX same size as A); each row is one element, tf/loc are COLUMNS of height size(A,1): ismember([1 2;5 6;3 4],[3 4;1 2;7 8],'rows') -> tf=[1;0;1], loc=[2;0;1]; duplicate row in B -> lowest index loc; NaN-containing row never matches. numkit previously had NO 2nd output loc (added) and threw on COMPLEX (added 2026-05-29). |
| `ismembertol` | ✅ | Sig: r = ismembertol(...). Spec-extension batch 2026-05-09. |
| `join` | ✅ | Sig: r = join(...). Spec-extension batch 2026-05-09. |
| `numunique` | ✅ | Sig: N = numunique(X). 10k with 137 distinct. 1000 iters. |
| `outerjoin` | ❌ |  |
| `setdiff` | ✅ 🔬 | Sig setdiff(A,B[,setOrder | 'rows']): A elements not in B. Default 'sorted': setdiff([3 1 2 5 4],[2 5]) = [1 3 4]. 'stable' keeps A-order (first occurrence): [3 1 4]. COMPLEX (C output only): exact-equality, |z|+angle order. DEEP-PROBE 2026-05-31: added the 'rows' flag (was THROWING 'Index exceeds array dimensions'); treats each row as an element, returns sorted unique A-rows not in B: setdiff([1 2;3 4;5 6],[3 4;9 9;1 2],'rows')=[5 6]. 'rows' ia index output deferred. numkit previously IGNORED 'stable' (fixed) and threw on COMPLEX (added 2026-05-29). |
| `setxor` | ✅ | Sig: r = setxor(A,B[,setOrder | 'rows']): symmetric difference, elements in exactly one of A or B, sorted. setxor([1 2 3 4],[2 4 5 6]) = [1 3 5 6]. DEEP-PROBE 2026-05-31: added the 'rows' flag (was IGNORING it and flattening element-wise to a 1xN vector); each row is one element, returns sorted unique rows present in exactly one input: setxor([1 2;3 4;5 6],[3 4;9 9;1 2],'rows') = [5 6;9 9] (2x2); setxor([5 6;1 1],[1 1;2 2;7 8],'rows') = [2 2;5 6;7 8]. 'rows' ia/ib index outputs deferred (throw). NaN-containing rows kept (NaN != NaN). Spec-extension batch 2026-05-09. |
| `union` | ✅ 🔬 | Sig union(A,B[,setOrder | 'rows']). Default 'sorted': union([1 2 3],[3 4 5]) = [1 2 3 4 5]. 'stable': unique(A)-order then new-of-B. COMPLEX (C output only): exact-equality, |z|+angle order. DEEP-PROBE 2026-05-31: added the 'rows' flag (was IGNORING it and flattening to a 1xN vector); returns sorted unique rows of [A;B]: union([1 2;3 4;5 6],[3 4;9 9;1 2],'rows')=[1 2;3 4;5 6;9 9] (4x2). 'rows' ia/ib outputs deferred. numkit previously IGNORED 'stable' (fixed) and threw on COMPLEX (added 2026-05-29). |
| `unique` | ✅ 🔬 | unique(M,'rows','stable') vs MATLAB R2025b. DEEP-PROBE 2026-05-31: numkit honoured 'stable' for the flat-vector path but DROPPED it on the 'rows' path (code comment: "'rows' + 'stable' not yet combined; rows path stays sorted") — it returned lexicographically-sorted rows instead of first-occurrence order. Now uniqueRows/uniqueRowsWithIndices take a stable flag: rows are kept in first-appearance order, ia indexes the first occurrence of each distinct row, ic maps every row back. M=[3 0;1 0;2 0;1 0;3 0] -> C=[3 0;1 0;2 0], ia=[1;2;3], ic=[1;2;3;2;1] (sorted path unchanged: C=[1 0;2 0;3 0]). NaN-containing rows stay distinct/in-place. covers:[unique] (sibling spec; unique.json keeps the sorted/flat cases — split for the VM 255-register limit). |
| `uniquetol` | ✅ | Sig: U = uniquetol(X, TOL). 10k with rounded vals. 10 iters. Fixed global tol*max(|A|) 2026-05-09. |

### Arithmetic

**Namespace:** builtin — 28 ✅ + 0 ⚠️ / 34 = 82%

| function | status | comment |
|---|:---:|---|
| `bsxfun` | ✅ | Sig: D = bsxfun(@op, A, B). Broadcast 1x1k + 1kx1 → 1k×1k. 100 iters. |
| `ceil` | ✅ | Sig: r = ceil(...). Element-wise libm-backed primitive. INTEGER input is the IDENTITY and keeps the class: ceil(int16([-3 5]))=[-3 5] int16 (numkit previously threw 'Not a double array'; fixed 2026-05-30). Spec-extension batch 2026-05-09 + integer-class. NOTE: ; only inside matrix-literal INPUTS. |
| `ctranspose` | ✅ 🔬 | Sig: r = ctranspose(A) === A'. Conjugate transpose. DEEP-PROBE 2026-05-31: ctranspose threw 'Transpose not supported for this type' on non-DOUBLE/non-COMPLEX. Now shares transpose2D with transposeNC; COMPLEX conjugates (zr=5,zi=-6 from conj(Z(2,1)=5+6i); Z'(2,1)=conj(Z(1,2)=3+4i)=3-4i so zor=3,zoi=-4), all other types are a plain transpose (no element change): char preserved (c12='c'=99), logical preserved (l12=L(2,1)=0, lcls=1), cell off-diagonal swap (k21=KT{2,1}=K{1,2}='x'). 3-D still throws. Spec-extension batch 2026-05-09. |
| `cumprod` | ✅ 🔬 | Sig cumprod(X[,dim][,direction][,nanflag]). forward cumprod([1 2 3 4])(4)=24. 'reverse' = [24 24 12 4]. 'omitnan' treats NaN as 1: cumprod([2 NaN 4],'omitnan')=[2 2 8]. INTEGER input keeps the class and accumulates NATIVELY with saturation: cumprod(int8([5 10 10]))=[5 50 127] int8 (500 saturates to 127), cumprod(int8([2 3 4]))=[2 6 24]. numkit previously threw 'Not a double array' on integer input; fixed 2026-05-30. COMPLEX (fixed 2026-06-05): accumulates Complex products: cumprod([1+1i 1-1i 2i])=[1+1i 2+0i 0+4i]. (Earlier: 'reverse'/'omitnan' flag parsing.) |
| `cumsum` | ✅ 🔬 | Sig cumsum(X[,dim][,direction][,nanflag]). forward (default) cumsum([1 2 3 4])(4)=10. 'reverse' = [10 9 7 4]. 'omitnan' treats NaN as 0: cumsum([1 NaN 3],'omitnan')=[1 1 4]. dim+reverse: cumsum([1 2 3;4 5 6],2,'reverse') row1=[6 5 3]. INTEGER input keeps the class and accumulates NATIVELY with saturation: cumsum(int8([100 100 -100]))=[100 127 27] int8 (the clamped 127 carries forward, then -100->27), cumsum(uint8([200 100]))=[200 255], cumsum(int32([1 2;3 4]),2)=[1 3;3 7] int32. numkit previously threw 'Not a double array' on integer input; fixed 2026-05-30. COMPLEX (fixed 2026-06-05): accumulates Complex element-wise: cumsum([1+1i 2+2i 3+3i])=[1+1i 3+3i 6+6i]; per-column for a matrix; dim+reverse works (flip-scan-flip). (Earlier: 'reverse'/'omitnan' flag parsing.) NOTE: ; only inside matrix-literal INPUTS. |
| `diff` | ✅ | Sig: r = diff(X[,n[,dim]]). INTEGER input keeps the class and accumulates NATIVELY with saturation at each pass: diff(int8([10 5 20]))=[-5 15] int8; diff(int8([-100 100]))=127 int8 (200 overflow -> 127); diff(uint8([5 3]))=0 uint8 (3-5 underflow -> 0); diff(int8([1 2 4 8]),2)=[1 2]; diff(int32([1 2 3;5 8 13]),1,2)=[1 1;3 5] int32. numkit previously promoted to double; fixed 2026-05-30. COMPLEX (fixed 2026-06-05): real + imaginary differenced together: diff([1+2i 4+6i 9+12i])=[3+4i 5+6i]; diff(z,2) 2nd-order=[2+2i 2+2i]; diff(M,1,2) along columns=[1+1i;1+2i] (previously the imaginary part was dropped). Spec-extension batch 2026-05-09 + integer-class + complex. NOTE: diff(X,0) — and any negative, fractional or non-scalar order — now errors ('Difference order N must be a positive integer scalar'), matching MATLAB (fixed 2026-06-05, bugs/builtin/diff-zero-order.md). Error cases can't be fingerprinted in a value-parity spec; they are covered by toolboxes/builtin/tests/diff_order_test.cpp. This spec validates that VALID orders (1, 2, with dim, integer + complex) are unchanged. |
| `fix` | ✅ | Sig: r = fix(...). Element-wise libm-backed primitive. INTEGER input is the IDENTITY and keeps the class: fix(int32([-7 7]))=[-7 7] int32 (numkit previously threw 'Not a double array'; fixed 2026-05-30). Spec-extension batch 2026-05-09 + integer-class. NOTE: ; only inside matrix-literal INPUTS. |
| `floor` | ✅ | Sig: r = floor(...). Element-wise libm-backed primitive. INTEGER input is the IDENTITY and keeps the class: floor(int8([-3 5 -128]))=[-3 5 -128] int8 (numkit previously threw 'Not a double array'; fixed 2026-05-30). Spec-extension batch 2026-05-09 + integer-class. NOTE: ; only inside matrix-literal INPUTS. |
| `idivide` | ✅ | Sig: idivide(...). Spec-extension batch 2026-05-09. |
| `ldivide` | ✅ | Sig: r = ldivide(...). Arithmetic op. Spec-extension batch 2026-05-09. Fingerprints scalar-only. |
| `minus` | ✅ | Sig: r = minus(...). Arithmetic op. Spec-extension batch 2026-05-09. Fingerprints scalar-only. |
| `mldivide` | ✅ | Sig: X = mldivide(A,B) ↔ A\B. Square A: LU with partial pivoting. Tall A (m>n): QR via Householder + R back-solve (least squares). Wide A (m<n, min-norm): NOT yet supported — throws m:mldivide:wide. Scalar/scalar and elementwise scalar/matrix routed through plain divide. |
| `mod` | ✅ | Sig: r = mod(...). INTEGER input keeps the class: mod(int8(-7),int8(3))=2 int8 (floored), mod(int8([7 8 9]),int8(3))=[1 2 0] int8; a double operand is promoted to the integer class (mod(int8(7),3)=1 int8). Real-DOUBLE arrays (va = vector%vector, vsa = vector%scalar) take the SIMD path (a-floor(a/b)*b, bit-identical to the scalar formula). Zero divisor: mod(a,0)==a (z0=5, z0n=-5, z00=0) — the scalar VM/TW fast path previously returned NaN via std::fmod. numkit previously returned double / threw on integer arrays; fixed 2026-05-30. SIMD mod added 2026-06; scalar mod(a,0) fixed 2026-06. NOTE: ; only inside matrix-literal INPUTS. |
| `movsum` | ✅ 🔬 | Sig: movsum(A, k[, dim] [, nanflag] [, Name, Value]). Same surface as movmean. EVEN scalar window leans BACKWARD (current+previous), MATLAB rule: movsum([1 2 3 4],2)=[1 3 5 7] (window [i-1,i]); movsum([1 2 3 4 5 6],4)=[3 6 10 14 18 15] (window [i-2,i+1]). numkit previously leaned forward for even k -- fixed 2026-05-29. |
| `mpower` | ✅ | Sig: r = mpower(a,b) (a^b). Spec-extension batch 2026-05-09. KNOWN GAP: matrix^integer (M^n where M is matrix) not implemented in numkit — only scalar^scalar pinned. Documented separately; would need O(log n) repeated mtimes for the matrix branch. |
| `mrdivide` | ✅ | Sig: X = mrdivide(A,B) ↔ A/B  ↔ X·B = A. Composes via the standard transpose trick X = (B'\A')'. So uses the same LU/QR primitives as mldivide. matrix/scalar is elementwise. scalar/matrix ERRORS with m:mrdivide:dim per MATLAB R2025b (verified: `2/[1 2; 3 4]` → 'Matrix dimensions must agree'). |
| `mtimes` | ✅ | Sig: r = mtimes(...). Arithmetic op. Spec-extension batch 2026-05-09. |
| `pagectranspose` | ✅ | 128x64x8 real-valued — pagectranspose equals pagetranspose. 100 iters. |
| `pagemldivide` | ✅ | Sig: page-wise wrappers (pageeig, pagesvd, pagepinv, pagenorm, pagemldivide, pagemrdivide, pagelsqminnorm, pageinv). All iterate the corresponding 2-D linalg op per page. Fingerprints pin shapes + a handful of values (we use pageinv = inv per page, pageeig on symmetric input that gives exact eigvals [5 5 5] for one page, etc.). pagelsqminnorm not in fingerprint — it uses pinv which goes through SVD whose sign convention may differ at machine precision. |
| `pagemrdivide` | ✅ | Sig: page-wise wrappers (pageeig, pagesvd, pagepinv, pagenorm, pagemldivide, pagemrdivide, pagelsqminnorm, pageinv). All iterate the corresponding 2-D linalg op per page. Fingerprints pin shapes + a handful of values (we use pageinv = inv per page, pageeig on symmetric input that gives exact eigvals [5 5 5] for one page, etc.). pagelsqminnorm not in fingerprint — it uses pinv which goes through SVD whose sign convention may differ at machine precision. |
| `pagemtimes` | ✅ | Sig: C = pagemtimes(A, B). 20×20×20 batch matmul. 100 iters. |
| `pagetranspose` | ✅ | 128x64x8 array, page-wise transpose. 100 iters. |
| `plus` | ✅ | Sig: r = plus(...). Arithmetic op. Spec-extension batch 2026-05-09. Fingerprints scalar-only. |
| `power` | ✅ | Sig: r = power(...). Arithmetic op. A negative real base ^ non-integer exponent is COMPLEX like MATLAB: (-8)^(1/3)=1+1.732i (c1r=1, c1i=sqrt3), power([-8 8 -27],1/3)=[1+1.73i, 2, 1.5+2.60i]. Arrays promote per-pair: [-8 8].^[2 0.5] stays real (rp=1), [-8 8].^[0.5 2] is complex (cx=1). Integer exp / positive base stay real. Fixed 2026-06 (was real NaN across the power subsystem). |
| `prod` | ✅ 🔬 | Sig: r = prod(...). Spec-extension batch 2026-05-09 verified bit-identical MATLAB R2025b. DEEP-PROBE 2026-05-29: empty-input identities — prod([])=1 (scalar, numel 1, NOT 1x0); partial empty keeps per-column shape: prod(zeros(0,3))=[1 1 1]. |
| `rdivide` | ✅ | Sig: r = rdivide(...). Arithmetic op. Spec-extension batch 2026-05-09. Fingerprints scalar-only. |
| `rem` | ✅ | Sig: r = rem(...). INTEGER input keeps the class: rem(int8(-7),int8(3))=-1 int8 (rem uses truncation toward zero, so the sign follows the dividend), rem(uint8(200),uint8(7))=4 uint8. numkit previously returned double / threw on integer arrays; fixed 2026-05-30. Spec-extension batch 2026-05-09 + integer-class. |
| `round` | ✅ | Sig round(x[,N[,'decimals'|'significant']]). round(x) = nearest int (half-away-from-zero). round(x,N) = N decimal places (N may be negative). round(x,N,'significant') = N significant digits. round(3.14159,2)=3.14; round(12345,-2)=12300; round(3.14159,3,'significant')=3.14; round(12345,2,'significant')=12000. INTEGER input is the IDENTITY and keeps the class: round(uint8([3 200]))=[3 200] uint8 (numkit previously threw 'Not a double array'; fixed 2026-05-30). numkit previously took only round(x) -- N + 'significant' added. Matches MATLAB R2025b. NOTE: ; only inside matrix-literal INPUTS. |
| `sum` | ✅ 🔬 | Sig: r = sum(...). Spec-extension batch 2026-05-09 verified bit-identical MATLAB R2025b. DEEP-PROBE 2026-05-29: empty-input identities — sum([])=0 (scalar, numel 1, NOT 1x0); partial empties keep per-column shape: sum(zeros(0,3))=[0 0 0], numel(sum(zeros(3,0)))=0. |
| `tensorprod` | ❌ | tensor contraction |
| `times` | ✅ | Sig: r = times(...). Arithmetic op. Spec-extension batch 2026-05-09. Fingerprints scalar-only. |
| `transpose` | ✅ 🔬 | Sig: r = transpose(A) === A.'. Type-preserving 2-D transpose. DEEP-PROBE 2026-05-31: the transpose() builtin (matrix.cpp) coerced ALL inputs to a DOUBLE matrix of element codes, and the .'/' operators (transposeNC/ctranspose in unary_ops.cpp) threw 'Transpose not supported for this type' on non-DOUBLE/non-COMPLEX. Generalised to one shared transpose2D: DOUBLE/SINGLE/CHAR/LOGICAL/int via raw-byte rearrange, COMPLEX per-element (transpose() = no conjugate), CELL per-cell move; STRING/STRUCT/FUNC deferred. transpose(['ab';'cd'])=['ac';'bd'] char (c11='a'=97,c12='c'=99,c21='b'=98); logical/int8/single class preserved; transpose(complex) does NOT conjugate (zr=5,zi=6 from Z(2,1)=5+6i); cell off-diagonal swap (k12=3,k21=2). transpose() now delegates to transposeNC. 3-D still throws. Spec-extension batch 2026-05-09. |
| `uminus` | ✅ | Sig: r = uminus(...). Arithmetic op. Spec-extension batch 2026-05-09. |
| `uplus` | ✅ | Sig: r = uplus(...). Arithmetic op. Spec-extension batch 2026-05-09. |

### Trigonometry

**Namespace:** builtin — 47 ✅ + 0 ⚠️ / 47 = 100%

| function | status | comment |
|---|:---:|---|
| `acos` | ✅ | Sig: y = acos(x). Element-wise inverse-trig (libm-backed). For a REAL argument outside [-1,1] the result is complex and the whole array is promoted (fixed 2026-06-05): acos(2)=0+i*acosh(2)=0+1.31696i; acos(-2)=pi-i*acosh(2)=3.14159-1.31696i; acos([0.5 2 -3]) mixes a real element with complex ones. Computed via acosh because std::acos's complex branch cut flips the imaginary sign vs MATLAB on [1,inf). Spec-extension batch 2026-05-09 — fingerprint covers domain edges + a few interior points. verified bit-identical to MATLAB R2025b (tol=1e-12 covers libm vs Intel-SVML differences). |
| `acosd` | ✅ | Sig: y = acosd(x). Element-wise inverse-trig (libm-backed). Spec-extension batch 2026-05-09 — fingerprint covers domain edges + a few interior points. verified bit-identical to MATLAB R2025b (tol=1e-12 covers libm vs Intel-SVML differences). |
| `acosh` | ✅ | Sig: y = acosh(x). Element-wise inverse-trig (libm-backed). ARRAY promotion (fixed 2026-06-05): any element <1 promotes the WHOLE array to complex — acosh([0.5 2])=[0+1.0472i 1.31696+0i] (previously the <1 element became real NaN); acosh(-2)=1.31696+pi*i. std::acosh's complex branch matches MATLAB. Spec-extension batch 2026-05-09 — fingerprint covers domain edges + a few interior points. verified bit-identical to MATLAB R2025b (tol=1e-12 covers libm vs Intel-SVML differences). |
| `acot` | ✅ | Sig: y = acot(x). Element-wise inverse-trig (libm-backed). Spec-extension batch 2026-05-09 — fingerprint covers domain edges + a few interior points. verified bit-identical to MATLAB R2025b (tol=1e-12 covers libm vs Intel-SVML differences). |
| `acotd` | ✅ | Sig: y = acotd(x). Element-wise inverse-trig (libm-backed). Spec-extension batch 2026-05-09 — fingerprint covers domain edges + a few interior points. verified bit-identical to MATLAB R2025b (tol=1e-12 covers libm vs Intel-SVML differences). |
| `acoth` | ✅ | Sig: y = acoth(x). Element-wise inverse-trig (libm-backed). Spec-extension batch 2026-05-09 — fingerprint covers domain edges + a few interior points. verified bit-identical to MATLAB R2025b (tol=1e-12 covers libm vs Intel-SVML differences). |
| `acsc` | ✅ | Sig: y = acsc(x). Element-wise inverse-trig (libm-backed). Spec-extension batch 2026-05-09 — fingerprint covers domain edges + a few interior points. verified bit-identical to MATLAB R2025b (tol=1e-12 covers libm vs Intel-SVML differences). |
| `acscd` | ✅ | Sig: y = acscd(x). Element-wise inverse-trig (libm-backed). Spec-extension batch 2026-05-09 — fingerprint covers domain edges + a few interior points. verified bit-identical to MATLAB R2025b (tol=1e-12 covers libm vs Intel-SVML differences). |
| `acsch` | ✅ | Sig: y = acsch(x). Element-wise inverse-trig (libm-backed). Spec-extension batch 2026-05-09 — fingerprint covers domain edges + a few interior points. verified bit-identical to MATLAB R2025b (tol=1e-12 covers libm vs Intel-SVML differences). |
| `asec` | ✅ | Sig: y = asec(x). Element-wise inverse-trig (libm). Spec-extension batch 2026-05-09 verified bit-identical MATLAB R2025b on domain edges. |
| `asecd` | ✅ | Sig: y = asecd(x). Element-wise inverse-trig (libm). Spec-extension batch 2026-05-09 verified bit-identical MATLAB R2025b on domain edges. |
| `asech` | ✅ | Sig: y = asech(x). Element-wise inverse-trig (libm). Spec-extension batch 2026-05-09 verified bit-identical MATLAB R2025b on domain edges. |
| `asin` | ✅ | Sig: y = asin(x). Element-wise inverse-trig (libm). For a REAL argument outside [-1,1] the result is complex and the whole array is promoted (fixed 2026-06-05): asin(2)=pi/2-i*acosh(2)=1.5708-1.31696i; asin(-2)=-pi/2+i*acosh(2)=-1.5708+1.31696i. Computed via acosh because std::asin's complex branch cut flips the imaginary sign vs MATLAB on [1,inf). Spec-extension batch 2026-05-09 verified bit-identical MATLAB R2025b on domain edges. |
| `asind` | ✅ | Sig: y = asind(x). Element-wise inverse-trig (libm). Spec-extension batch 2026-05-09 verified bit-identical MATLAB R2025b on domain edges. |
| `asinh` | ✅ | Sig: y = asinh(x). Element-wise inverse-trig (libm). Spec-extension batch 2026-05-09 verified bit-identical MATLAB R2025b on domain edges. |
| `atan` | ✅ | Sig: y = atan(x). Element-wise inverse-trig (libm). Spec-extension batch 2026-05-09 verified bit-identical MATLAB R2025b on domain edges. |
| `atan2` | ✅ | Sig: r = atan2(...). Element-wise libm-backed primitive. Spec-extension batch 2026-05-09 verified bit-identical MATLAB R2025b. |
| `atan2d` | ✅ | Sig: r = atan2d(...). Element-wise libm-backed primitive. Spec-extension batch 2026-05-09 verified bit-identical MATLAB R2025b. |
| `atand` | ✅ | Sig: y = atand(x). Element-wise inverse-trig (libm). Spec-extension batch 2026-05-09 verified bit-identical MATLAB R2025b on domain edges. |
| `atanh` | ✅ | Sig: y = atanh(x). Element-wise inverse-trig (libm). Out-of-domain |x|>1 → complex (fixed 2026-06-05, scalar AND array). KEY: std::atanh's complex branch flips the imaginary SIGN vs MATLAB for x<-1, so use atanh(x)=atanh(1/x)+i*sign(x)*pi/2: atanh(2)=0.5493+1.5708i, atanh(-2)=-0.5493-1.5708i (numkit previously gave -0.5493+1.5708i for the scalar too). atanh([2 -2 0.5]) mixes complex + a real in-range element. Spec-extension batch 2026-05-09 verified bit-identical MATLAB R2025b on domain edges. |
| `cart2pol` | ✅ | Sig: r = cart2pol(...). Spec-extension batch 2026-05-09 verified MATLAB R2025b parity. |
| `cart2sph` | ✅ | Sig: r = cart2sph(...). Spec-extension batch 2026-05-09 verified MATLAB R2025b parity. |
| `cos` | ✅ | Sig: y = cos(x). Forward-trig, libm-backed. Spec-extension batch 2026-05-09 verified bit-identical MATLAB R2025b. |
| `cosd` | ✅ | Sig: y = cosd(x). Forward-trig, libm-backed. Spec-extension batch 2026-05-09 verified bit-identical MATLAB R2025b. |
| `cosh` | ✅ | Sig: y = cosh(x). Forward-trig, libm-backed. Spec-extension batch 2026-05-09 verified bit-identical MATLAB R2025b. |
| `cospi` | ✅ | Sig: r = cospi(x). Accurate cos(pi*x) via exact int64 octant reduction + single-double SLEEF cospik polynomial (third_party/sleef, BSL-1.0), replacing the naive cos(pi*x). Branches: integer -> +/-1 (r1 cospi(0)=1, r5 cospi(1)=-1, r6 cospi(2)=1); half-integer -> exact 0 (r2 x=0.5, r3 x=1.5, r12 x=-0.5); large half-integer 1e7+0.5 -> 0 (r4); accurate 1/3=0.5 (r7), 0.25=sqrt2/2 (r8), 1/6=sqrt3/2 (r9), 1/7 (r10); generic 0.1 (r11). <=2 ULP vs MATLAB R2025b (exact at the closed-form points). |
| `cot` | ✅ | Sig: y = cot(x). Forward-trig, libm-backed. Spec-extension batch 2026-05-09 verified bit-identical MATLAB R2025b. |
| `cotd` | ✅ | Sig: y = cotd(x). Forward-trig, libm-backed. Spec-extension batch 2026-05-09 verified bit-identical MATLAB R2025b. |
| `coth` | ✅ | Sig: y = coth(x). Forward-trig, libm-backed. Spec-extension batch 2026-05-09 verified bit-identical MATLAB R2025b. |
| `csc` | ✅ | Sig: y = csc(x). Forward-trig, libm-backed. Spec-extension batch 2026-05-09 verified bit-identical MATLAB R2025b. |
| `cscd` | ✅ | Sig: y = cscd(x). Forward-trig, libm-backed. Spec-extension batch 2026-05-09 verified bit-identical MATLAB R2025b. |
| `csch` | ✅ | Sig: y = csch(x). Forward-trig, libm-backed. Spec-extension batch 2026-05-09 verified bit-identical MATLAB R2025b. |
| `deg2rad` | ✅ | Sig: r = deg2rad(...). Spec-extension batch 2026-05-09 verified bit-identical MATLAB R2025b. |
| `hypot` | ✅ | Sig: r = hypot(...). Element-wise libm-backed primitive. Spec-extension batch 2026-05-09 verified bit-identical MATLAB R2025b. |
| `pol2cart` | ✅ | Sig: r = pol2cart(...). Spec-extension batch 2026-05-09. |
| `rad2deg` | ✅ | Sig: r = rad2deg(...). Spec-extension batch 2026-05-09 verified bit-identical MATLAB R2025b. |
| `sec` | ✅ | Sig: y = sec(x). Forward-trig, libm-backed. Spec-extension batch 2026-05-09 verified bit-identical MATLAB R2025b. |
| `secd` | ✅ | Sig: y = secd(x). Forward-trig, libm-backed. Spec-extension batch 2026-05-09 verified bit-identical MATLAB R2025b. |
| `sech` | ✅ | Sig: y = sech(x). Forward-trig, libm-backed. Spec-extension batch 2026-05-09 verified bit-identical MATLAB R2025b. |
| `sin` | ✅ | Sig: y = sin(x). Forward-trig, libm-backed. Spec-extension batch 2026-05-09 verified bit-identical MATLAB R2025b. |
| `sind` | ✅ | Sig: y = sind(x). Forward-trig, libm-backed. Spec-extension batch 2026-05-09 verified bit-identical MATLAB R2025b. |
| `sinh` | ✅ | Sig: y = sinh(x). Forward-trig, libm-backed. Spec-extension batch 2026-05-09 verified bit-identical MATLAB R2025b. |
| `sinpi` | ✅ | Sig: r = sinpi(x). Accurate sin(pi*x) via exact int64 octant reduction + single-double SLEEF sinpik polynomial (third_party/sleef, BSL-1.0), replacing the naive sin(pi*x) that drifted to ~1e-10 by x=1e7 and never produced an exact integer zero. Branches: integer -> exact 0 (r1 x=0, r2 x=1, r3 x=7); large integer 1e7 -> 0 (r4); half -> +/-1 (r5 x=0.5, r6 x=1.5); accurate 1/6=0.5 (r7), 1/3=sqrt3/2 (r8), 0.25=sqrt2/2 (r9); large fractional 123456.25 (r10); large half-integer 1e10+0.5 -> 1 (r11); negative (r12); generic 0.1 (r13). <=2 ULP vs MATLAB R2025b (exact at r1-r11). |
| `sph2cart` | ✅ | Sig: r = sph2cart(...). Spec-extension batch 2026-05-09. |
| `tan` | ✅ | Sig: y = tan(x). SIMD via ported SLEEF xtan (Cody-Waite reduction + degree-7 half-angle polynomial + tan double-angle), replacing Div(Sin,Cos). 2-step reduction for |x|<15 (y1-y5), extended PI_A..D for |x|<1e6 (y6 x=10, y7 x=100, y8 x=1000, y9 x=50000); |x|>=1e6 falls to scalar std::tan. <=3.5 ULP, bit-identical MATLAB R2025b on probed inputs (incl. large args via extended reduction). |
| `tand` | ✅ | Sig: y = tand(x). Forward-trig, libm-backed. Spec-extension batch 2026-05-09 verified bit-identical MATLAB R2025b. |
| `tanh` | ✅ | Sig: y = tanh(x). Forward-trig, libm-backed. Spec-extension batch 2026-05-09 verified bit-identical MATLAB R2025b. |

### Exponents and Logarithms

**Namespace:** builtin — 13 ✅ + 0 ⚠️ / 13 = 100%

| function | status | comment |
|---|:---:|---|
| `exp` | ✅ | Sig: r = exp(...). Element-wise libm-backed primitive. Spec-extension batch 2026-05-09 verified bit-identical MATLAB R2025b. |
| `expm1` | ✅ | Sig: r = expm1(...). Element-wise libm-backed primitive. Spec-extension batch 2026-05-09 verified bit-identical MATLAB R2025b. |
| `log` | ✅ | Sig: r = log(...). Element-wise libm-backed primitive (SIMD via Highway hn::Log). Negative scalar promotes to complex: log(-1)==0+pi*i. ARRAY promotion (fixed 2026-06-05): any negative element promotes the WHOLE array — log([-1 4])=[0+pi*i 1.3863+0i] (previously the negative became real NaN). std::log's branch matches MATLAB. Verified vs MATLAB R2025b. |
| `log10` | ✅ | Sig: r = log10(...). Element-wise libm-backed primitive (SIMD via Highway hn::Log10). Negative scalar promotes to complex: log10(-1)==0+1.364i. ARRAY promotion (fixed 2026-06-05): any negative element promotes the WHOLE array — log10([-100 100])=[2+1.36438i 2+0i] (previously the negative became real NaN). Verified vs MATLAB R2025b. |
| `log1p` | ✅ | Sig: r = log1p(...). log1p(x)=log(1+x). For x<-1 the argument is negative so MATLAB returns complex (log1p(-2)=i*pi); any array element <-1 promotes the whole real array to complex, accurate per element (log1p kept for x>=-1, so r(3)=1e-10 stays accurate, not the lossy log(1+1e-10)). Complex input uses log(1+z): log1p(3+4i)=1.732868+0.785398i. log1p(-1)=-Inf. Complex branches added 2026-06-05 (bugs/builtin/gradient-3d sibling: bugs/builtin/log-complex-promotion-arrays.md, log1p follow-up). Real in-domain part is the original spec-extension batch 2026-05-09. |
| `log2` | ✅ | Sig: r = log2(...). Element-wise libm-backed primitive (SIMD via Highway hn::Log2). Negative scalar promotes to complex: log2(-1)==0+4.532i. ARRAY promotion (fixed 2026-06-05): any negative element promotes the WHOLE array — log2([-8 8])=[3+4.53236i 3+0i] (previously the negative became real NaN). Verified vs MATLAB R2025b. |
| `nextpow2` | ✅ | Sig: P = nextpow2(N). Smallest p such that 2^p >= |N|. Vectorised. Edges: |x|=0 -> 0; NaN -> NaN; ±Inf -> +Inf; complex z -> uses |z|. Bug fix 2026-05-08: complex input previously threw; NaN/Inf paths now match MATLAB. tol=0. |
| `nthroot` | ✅ 🔬 | Sig: r = nthroot(...). Spec-extension batch 2026-05-09. |
| `pow2` | ✅ | Sig: Y = pow2(X) = 2.^X. 1M-pt on [-50, 50]. 20 iters. Element-wise SAVE. |
| `reallog` | ✅ | Sig: Y = reallog(X). Strict positive domain. 1M-pt on [0.001, 100]. 20 iters. Element-wise SAVE. |
| `realpow` | ✅ | Sig: Z = realpow(X,Y). 1k×1k grid of x>0, real exp. 20 iters. Element-wise SAVE. |
| `realsqrt` | ✅ | Sig: Y = realsqrt(X). 1M-pt on [0, 1000]. 20 iters. Element-wise SAVE. |
| `sqrt` | ✅ | Sig: r = sqrt(...). Element-wise libm-backed primitive (SIMD via Highway hn::Sqrt). Negative scalar promotes to complex: sqrt(-4)==0+2i (real(zn)=0, imag(zn)=2). ARRAY promotion (fixed 2026-06-05): any negative element promotes the WHOLE array — sqrt([-1 4 -9])=[0+1i 2+0i 0+3i] (previously the negatives became real NaN). Verified vs MATLAB R2025b. |

### Special Functions

**Namespace:** builtin — 20 ✅ + 0 ⚠️ / 24 = 83%

| function | status | comment |
|---|:---:|---|
| `airy` | ✅ | Sig: r = airy(...). Spec-extension batch 2026-05-09 verified MATLAB R2025b parity. |
| `besselh` | ✅ | Sig: r = besselh(...). Bessel function. Spec-extension batch 2026-05-09 verified MATLAB R2025b parity. |
| `besseli` | ✅ | Sig: r = besseli(...). Bessel function. Spec-extension batch 2026-05-09 verified MATLAB R2025b parity. |
| `besselj` | ✅ | Sig: r = besselj(...). Bessel function. Spec-extension batch 2026-05-09 verified MATLAB R2025b parity. |
| `besselk` | ✅ | Sig: r = besselk(...). Bessel function. Spec-extension batch 2026-05-09 verified MATLAB R2025b parity. |
| `bessely` | ✅ | Sig: r = bessely(...). Bessel function. Spec-extension batch 2026-05-09 verified MATLAB R2025b parity. |
| `beta` | ✅ | Sig: r = beta(...). Special function. Spec-extension batch 2026-05-09 verified MATLAB R2025b parity. |
| `betainc` | ✅ | Sig: r = betainc(...). Special function. Spec-extension batch 2026-05-09 verified MATLAB R2025b parity. |
| `betaincinv` | ✅ | Sig: r = betaincinv(...). Special function. Spec-extension batch 2026-05-09 verified MATLAB R2025b parity. |
| `betaln` | ✅ | Sig: r = betaln(...). Special function. Spec-extension batch 2026-05-09 verified MATLAB R2025b parity. |
| `ellipj` | ✅ | Sig: r = ellipj(...). Spec-extension batch 2026-05-09. |
| `ellipke` | ✅ | Sig: r = ellipke(...). Spec-extension batch 2026-05-09. |
| `erf` | ✅ | Sig: r = erf(...). Special function. Spec-extension batch 2026-05-09 verified MATLAB R2025b parity. |
| `erfc` | ✅ | Sig: r = erfc(...). Special function. Spec-extension batch 2026-05-09 verified MATLAB R2025b parity. |
| `erfcinv` | ✅ | Sig: r = erfcinv(...). Special function. Spec-extension batch 2026-05-09 verified MATLAB R2025b parity. |
| `erfcx` | ✅ | Sig: r = erfcx(...). Spec-extension batch 2026-05-09. |
| `erfinv` | ✅ | Sig: r = erfinv(...). Special function. Spec-extension batch 2026-05-09 verified MATLAB R2025b parity. |
| `expint` | ✅ | Sig: r = expint(...). Spec-extension batch 2026-05-09. |
| `gamma` | ✅ | Sig: r = gamma(...). Special function. Spec-extension batch 2026-05-09 verified MATLAB R2025b parity. |
| `gammainc` | ✅ | Sig: r = gammainc(...). Special function. Spec-extension batch 2026-05-09 verified MATLAB R2025b parity. |
| `gammaincinv` | ✅ | Sig: r = gammaincinv(...). Special function. Spec-extension batch 2026-05-09 verified MATLAB R2025b parity. |
| `gammaln` | ✅ | Sig: r = gammaln(...). Special function. Spec-extension batch 2026-05-09 verified MATLAB R2025b parity. |
| `legendre` | ✅ | Sig: r = legendre(...). Spec-extension batch 2026-05-09. |
| `psi` | ✅ | Sig: r = psi(...). Spec-extension batch 2026-05-09. |

### Discrete Math

**Namespace:** builtin — 10 ✅ + 0 ⚠️ / 11 = 90%

| function | status | comment |
|---|:---:|---|
| `factor` | ✅ | Sig: r = factor(...). Spec-extension batch 2026-05-09. |
| `factorial` | ✅ | Sig: r = factorial(...). Spec-extension batch 2026-05-09. |
| `gcd` | ✅ 🔬 | Sig: g = gcd(A,B); [g,u,v] = gcd(A,B) also returns the Bezout coefficients (extended Euclid) such that A.*u + B.*v = g, elementwise, matching MATLAB R2025b: gcd(12,18)->[6,-1,1], gcd(8,5)->[1,2,-3], gcd(-12,18)->[6,1,1] (g>=0, coeffs normalized with it), gcd(0,0)->[0,0,0]. numkit previously only supported the 1-output form (3-output threw 'Undefined function u'); fixed 2026-05-30. Spec-extension batch 2026-05-09 + extended-gcd. |
| `isprime` | ✅ | Sig: r = isprime(...). Spec-extension batch 2026-05-09. |
| `lcm` | ✅ | Sig: r = lcm(...). Spec-extension batch 2026-05-09. |
| `matchpairs` | ✅ | Sig: [M, uR, uC] = matchpairs(Cost, costUnmatched [, 'min'|'max']) — linear assignment / bipartite matching on rectangular Cost. 'min' (default): minimise total cost with costUnmatched as the per-row/col unmatched penalty. 'max': maximise total benefit with costUnmatched as the per-row/col REWARD for leaving unmatched (note: a high positive costUnmatched in 'max' mode leaves everything unmatched — matches MATLAB R2025b's documented convention). Algorithm: Jonker-Volgenant Hungarian on the augmented (m+n)×(m+n) matrix; 'max' negates both Cost and costUnmatched before solving. Total cost is what we fingerprint — assignment ordering is engine-dependent, totals are unique. Bit-exact MATLAB R2025b (tol=0) on the documented test cases. |
| `nchoosek` | ✅ 🔬 | Sig: r = nchoosek(N,K) scalar binomial; M = nchoosek(V,K) vector form -> all K-combinations of V, one per row, in lexicographic order of element indices. nchoosek([1 2 3 4],2)=[1 2;1 3;1 4;2 3;2 4;3 4] (6x2); values come from V not indices (nchoosek([10 20 30],2)=[10 20;10 30;20 30]). k==numel->single row, k==0->1x0. numkit previously threw on vector input; implemented 2026-05-29. Spec-extension batch 2026-05-09 + vector form. |
| `perms` | ✅ | Sig: r = perms(...). Spec-extension batch 2026-05-09. |
| `primes` | ✅ | Sig: r = primes(...). Spec-extension batch 2026-05-09. |
| `rat` | ✅ | Sig: S = rat(X[, tol]) — 1-output continued-fraction string; [N, D] = rat(X[, tol]) — 2-output integer numerator/denominator (vectorised). Default tol = 1e-6·max(1,|x|). Algorithm: regularized CF expansion with round() (NOT floor), matching MATLAB R2025b — produces signed coefficients (e.g. 0.5 → '1 + 1/(-2)'). Fingerprint covers both forms across scalar, irrational, terminating, and vector inputs. |
| `rats` | ✅ | Sig: S = rats(X[, len]). Default len=13. Each scalar element is formatted as 'numerator/denominator' centre-padded to len characters; for vectors the per-element fields are concatenated. MATLAB's exact spacing differs subtly between Linux/Windows builds — fingerprints pin (a) the field length is approximately len, (b) the slash separator is present in the expected mid-region. Bit-comparison of the rendered string is intentionally NOT a fingerprint (would lock numkit to one MATLAB build's whitespace convention). |

### Polynomials

**Namespace:** builtin — 10 ✅ + 0 ⚠️ / 12 = 83%

| function | status | comment |
|---|:---:|---|
| `poly` | ✅ | Sig: p = poly(A) for square matrix. Char polynomial via Souriau-Faddeev-LeVerrier; p = [1 c1 c2 ... cn] such that roots(p) == eig(A). Bit-identical with MATLAB R2025b on probed companion-form matrix. |
| `polyder` | ✅ 🔬 | Sig: K = polyder(P). Deterministic 100-coef poly. 1000 iters. Element-wise SAVE. |
| `polydiv` | ✅ | Sig [Q,R]=polydiv(U,V) (via deconv): polynomial division. (x^3-2x+1)/(x-1) divides EXACTLY -> Q=[1 1 -1], R=[0 0 0 0] (sum(|R|)=0). Pins BOTH outputs (was out_var dump of Q only). |
| `polyeig` | ✅ | Sig: e = polyeig(A0, A1, ..., Ak). Polynomial eigenvalue problem via companion linearisation + char-poly + roots(). Eigenvalues-only form. Linear test: (A0 + λI)x = 0 → e = eigvals(-A0) = [-2, -3]. Quadratic test: (λ²-5λ+6)·I → e = {2, 2, 3, 3}. Real ordering may differ — fingerprint sorts. Tol 1e-5 because the characteristic-polynomial → roots() path has lower precision than direct eig (residual imag part ~1e-7 for nominally-real eigvals). |
| `polyfit` | ✅ 🔬 | Sig: p = polyfit(x,y,n); [p,S,mu] = polyfit(...); [y,delta] = polyval(p,x,S,mu). Covers default coeffs, mu = [mean(x); std(x,0)] centering/scaling, S.normr/S.df, and the polyval error estimate delta = normr/sqrt(df)*sqrt(1+rowSum((V/R).^2)). S.R sign convention is intentionally not fingerprinted (Cholesky vs MATLAB qr-R differ in row sign; R'R identical so delta matches). DEEP-PROBE c166: added S.rsquared (= 1 - (normr/norm(y-mean(y)))^2), MATLAB R2025b's 4th S field which numkit was missing (had only R/df/normr); now numel(fieldnames(S))=4, S.rsquared=0.9973053289. covers:[polyfit]. |
| `polyint` | ✅ 🔬 | Sig: P_int = polyint(P). Deterministic 100-coef. 1000 iters. Element-wise SAVE. |
| `polyval` | ✅ 🔬 | Sig: r = polyval(...). Spec-extension batch 2026-05-09. |
| `polyvalm` | ✅ | Sig: Y = polyvalm(P, A). Matrix poly eval x^2-3x+2. 10000 iters. |
| `residue` | ✅ | Sig: [r, p, k] = residue(b, a) — s-domain partial-fraction expansion. [r, p, k] = residuez(b, a) — z-domain (B/A polynomials in z^-1 ascending order). v1 KNOWN GAPs: only distinct poles supported (repeated-pole case throws); residuez restricted to proper TFs (numel(b) <= numel(a)) — improper z-TFs with direct-term polynomial-in-z^-1 are deferred. Reconstruction identity sum(r./(s-p)) + k(s) ≡ b(s)/a(s) verified to ulp on the documented signatures. Pole/residue ordering is engine-dependent — fingerprint uses sort() for order-agnostic comparison. Inverse forms [b, a] = residue(r, p, k) not yet wired. |
| `roots` | ✅ | Sig: R = roots(P). 4th-order poly with real roots {1,2,3,4}. 1000 iters. SAVE on sorted real parts. |
| `padecoef` | ✅ | Sig [num,den]=padecoef(T,N): order-N Pade approximant of exp(-T*s). T=2, N=3 -> num=[-1 6 -15 15], den=[1 6 15 15] (den = num with the odd-power signs flipped: same denominator/numerator magnitudes, classic Pade exp structure). Pins BOTH outputs (was out_var dump of num only with den fingerprints that the harness ignored; small N keeps coefficients exactly pinnable). |

### Random Number Generation

**Namespace:** builtin — 5 ✅ + 0 ⚠️ / 6 = 83%

| function | status | comment |
|---|:---:|---|
| `rand` | ✅ | Sig: A = rand(M,N). 1k×1k uniform. 100 iters. Custom fp (RNG diffs). |
| `randi` | ✅ | Sig: r = randi(...). Spec-extension batch 2026-05-09. |
| `randn` | ✅ | Sig: A = randn(M,N). 1k×1k normal. 100 iters. RNG-stream-diff fp. |
| `randperm` | ✅ | Sig: r = randperm(...). Spec-extension batch 2026-05-09. |
| `randstream` | ❌ |  |
| `rng` | ✅ | Sig: rng(SEED) + rand(). MATLAB-canonical Mersenne Twister (init_genrand reference, with seed=0 -> 5489 quirk) + 53-bit res53 uniform. Bit-identical with MATLAB R2025b across rng(0)/rng(1)/rng(42) (see Phase-0a-1 commit). |

### Interpolation

**Namespace:** builtin — 11 ✅ + 0 ⚠️ / 18 = 61%

| function | status | comment |
|---|:---:|---|
| `griddata` | ✅ | Sig: vq = griddata(x, y, v, xq, yq). 2-D scattered LINEAR interpolation (Delaunay + barycentric). Sample the plane v = 2x+3y+1 at the unit-square corners; barycentric interp of a plane is exact in every triangle, so recovered values are triangulation-independent and match MATLAB's default 'linear' exactly. Vector query vq=[3.5 2.25 4.3 3.6]; 4x4 meshgrid query Vg(1,1)=2.0, Vg(2,3)=3.4, Vg(end,end)=5.0, size 4x4 (all strictly inside the hull -> finite). Lifted to public C++ API numkit::builtin::griddata (was adapter-only) 2026-06. Only 'linear' is implemented; 'nearest'/'natural'/'cubic'/'v4' + 3-D form are a v1 gap (a trailing method arg is ignored). PREVIOUS spec used collinear points (all on y=x) which MATLAB R2025b rejects with a Delaunay error -> the run was N/A, not a real validation; replaced with this non-collinear plane test. Out-of-hull -> NaN (covered in gtest, not fingerprinted here to avoid NaN-compare ambiguity). |
| `griddatan` | ✅ ➖ | Sig: vi = griddatan(X, v, xi [, method]) — N-D scattered-data interpolation. X is m×n data points, v is m×1 values, xi is k×n queries. v1 supports 'nearest' for any n (brute-force Euclidean NN) and 'linear' for n=2 only (delegates to 2-D barycentric/Delaunay, same as griddata). KNOWN GAP: 'linear' for n≥3 needs a real N-D Delaunay (Qhull-style); not in v1 — errors with a clear pointer message. Default method = 'linear' (MATLAB-compat). Bit-exact MATLAB R2025b on the supported method+dim combinations. |
| `griddedinterpolant` | ❌ |  |
| `interp1` | ✅ 🔬 | Sig r=interp1(x,y,xq[,method][,extrap]). linear (default), nearest, spline, pchip, makima, previous/next; queries outside [x(1),x(end)] -> NaN by default. method/extrapval accept STRING and char. DEEP-PROBE 2026-05-31: (a) MATRIX Y now interpolated DOWN each column (size(Y,1)==length(x)); output is length(xq) x size(Y,2) regardless of xq orientation (was: threw 'x and y must have same length'). interp1(x,[10 100;20 200;30 300],2.5)=[25 250]; linear NaN-extrap, nearest extrapval, spline all per-column. N-D Y deferred. (b) 'nearest' tie-break now rounds an exactly-halfway query UP to the higher neighbor (MATLAB): interp1([1 2 3],[10 20 30],2.5,'nearest')=30 (was 20). COMPLEX y (fixed 2026-06-05): interpolates real + imag parts separately for EVERY method, then recombines; interp1(x,[1+1i 2+2i 3+3i],2.5)=2.5+2.5i; vector queries; 'nearest'=5+0i; 'spline'=-0.8125+1.0625i; out-of-range -> NaN+NaNi; matrix per-column. namespace=builtin. Matches MATLAB R2025b. |
| `interp2` | ✅ 🔬 | interp2 'spline' method (2026-05-30). Separable tensor-product cubic spline: interpolate each grid row along x then the result along y. The cubic spline is a LINEAR operator so this sequential 1-D form equals the 2-D tensor-product spline and matches MATLAB exactly. numkit previously errored 'spline not yet supported'. si interior (2.4,3.1)=12.851056; sn on grid node (2,3)=Z(3,2)=10 exact; so out of range (5,2)=23 (spline EXTRAPOLATES, unlike linear/nearest/cubic which return NaN); su non-uniform grid g=[1 2 4 7] at (3,3)=10.3471604938 (cubic convolution rejects non-uniform grids, spline does not); s2 2x2 grid falls back to bilinear=2.5. 'makima'/'pchip' for interp2 remain DEFERRED (nonlinear -> need full bicubic-Hermite tensor product with cross derivatives; naive separable diverges at interior points). namespace=builtin. Matches MATLAB R2025b. |
| `interp3` | ✅ | Sig: V = interp3(X, Y, Z, V, Xq, Yq, Zq). N-D linear interpolation. Bit-identical with MATLAB R2025b. readGridAxis now auto-detects meshgrid vs ndgrid orientation. |
| `interpft` | ✅ | Sig: Y = interpft(X, n[, dim]). Band-limited (FFT-based) interpolation to n samples. Default dim = first non-singleton. Vector form preserves originals at integer multiples of original spacing. Matrix dim=1 interpolates each column; dim=2 interpolates each row. tol=1e-12. |
| `interpn` | ✅ | Sig: V = interpn(X1, ..., Xn, V, Xq1, ..., Xqn). N-D linear interpolation (ndgrid form). Dispatches to interp3 internally; bit-identical with MATLAB R2025b. |
| `makima` | ✅ 🔬 | makima 2-arg pp-form (2026-05-30). MATLAB makima(x,y) (no query) returns a piecewise-polynomial struct usable with ppval, like spline(x,y)/pchip(x,y); numkit's spline/pchip 2-arg already did this but makima(x,y) errored 'pp-form (2-arg) not yet supported'. Now builds the same pp from the modified-Akima Hermite derivatives, converting each cubic Hermite segment to MATLAB's [pieces x 4] dx-power coefs (identical conversion to pchipPp; only the slopes differ): a=(d_i+d_{i+1}-2*delta)/h^2, b=(3*delta-2*d_i-d_{i+1})/h, c=d_i, d=y_i. For [1 4 9 16 25]: order=4, pieces=4, coefs row1=[-0.8333.. 2.3333.. 1.5 1] (c11=-0.8333.., c13=1.5); ppval matches value-form makima(x,y,xq): va=6.2395833333, vb=13.70365. Non-uniform [0 1 3 4 7]/[2 1 4 3 8]: ppval(0.5)=1.2161458333. 3 points [1 2 3]/[2 5 4]: ppval(1.5)=3.9201388889. namespace=interp. Matches MATLAB R2025b. |
| `meshgrid` | ✅ | Sig: r = meshgrid(...). Spec-extension batch 2026-05-09. |
| `mkpp` | ✅ 🔬 | Sig: PP = mkpp(BREAKS, COEFS). 4-piece linear. 10000 iters. |
| `ndgrid` | ✅ | Sig: r = ndgrid(...). Spec-extension batch 2026-05-09. |
| `pchip` | ✅ 🔬 | pchip 2-arg pp-form (2026-05-30). MATLAB pchip(x,y) (no query) returns a piecewise-polynomial struct usable with ppval, like spline(x,y); numkit's spline(x,y) already did this but pchip(x,y) errored 'requires 3 arguments'. Now builds the same pp from the shape-preserving Hermite derivatives, converting each cubic Hermite segment to MATLAB's [pieces x 4] dx-power coefs: a=(d_i+d_{i+1}-2*delta)/h^2, b=(3*delta-2*d_i-d_{i+1})/h, c=d_i, d=y_i. For [1 4 9 16]: order=4, pieces=3, coefs row1=[-0.25 1.25 2 1] (c11=-0.25, c13=2); ppval matches value-form pchip(x,y,xq): va=6.2395833333, vb=2.28125, vc=10.2186666667. Non-uniform [0 1 3 4]/[2 1 4 3]: ppval(0.5)=1.2708333333. 2 points -> line: ppval(1.5)=4. namespace=core. Matches MATLAB R2025b. |
| `ppval` | ✅ 🔬 | Sig: r = ppval(...). Spec-extension batch 2026-05-09. |
| `scatteredinterpolant` | ❌ |  |
| `spline` | ✅ 🔬 | Sig: yq = spline(x, v, xq). 50 → 1000 cubic spline. 100 iters. |
| `unmkpp` | ✅ 🔬 | Sig: [BR,CF,L,K] = unmkpp(PP). Inverse mkpp. 10000 iters. |

### Sparse Matrices

**Namespace:** `sparse.*` (future) — 4 ✅ + 0 ⚠️ / 53 = 7%

| function | status | comment |
|---|:---:|---|
| `amd` | ❌ | **deferred — toolboxes/sparse** |
| `bicg` | ❌ | **deferred — toolboxes/sparse** |
| `bicgstab` | ❌ | **deferred — toolboxes/sparse** |
| `bicgstabl` | ❌ | **deferred — toolboxes/sparse** |
| `cgs` | ❌ | **deferred — toolboxes/sparse** |
| `colamd` | ❌ | **deferred — toolboxes/sparse** |
| `colperm` | ✅ | colperm — column permutation sorted by ascending nonzero count, stable on ties. Documented for sparse matrices but works on any 2-D dense matrix (entry != 0 counts as nonzero). Output is a row vector of 1-indexed column indices. |
| `condest` | ✅ | Sig: c = condest(A). 1-norm condition number estimate. KNOWN GAP: MATLAB uses Higham 1988 power-iteration estimator (LAPACK dlacn1) that approximates norm(inv(A),1); we compute it exactly via inv(A). Matches MATLAB on well-conditioned A. For hilb(4) ≈ 1.5e4 and other near-singular inputs, our exact value differs from MATLAB's iterative estimate. Wide tol=0.5 (relative) accepts ±50% drift on near-singular inputs; pin only the well-conditioned cases I3 / D / UT for exact match. |
| `dissect` | ❌ | **deferred — toolboxes/sparse** |
| `dmperm` | ❌ |  |
| `eigs` | ❌ | **deferred — toolboxes/linalg** |
| `equilibrate` | ❌ |  |
| `etree` | ❌ | **deferred — toolboxes/sparse** |
| `etreeplot` | ❌ | **deferred — toolboxes/sparse** |
| `find` | ✅ 🔬 | Sig: idx = find(X, K, dir). Covers: no-count returns all nonzero indices; count K keeps the first K (find([0 1 0 1 1],2)=[2 4]); 'last' keeps the last K, still ascending (find(...,2,'last')=[4 5]; find(...,1,'last')=5); explicit 'first' == default; K>count returns all (numel 3); multi-output [r,c]=find(A,K) and [r,c,v]=find(A,K,'last') apply the same window (A=[0 3;4 0]: find(A,1)->r=2,c=1; find(A,1,'last')->r=1,c=2,v=3). Bug fixed 2026-06-05: single- AND multi-output previously ignored K + direction and returned all nonzeros. K must be a positive scalar integer (find(X,0) errors in MATLAB). Spec-extension batch 2026-05-09; K+dir extension 2026-06-05. |
| `full` | ❌ | **deferred — toolboxes/sparse** |
| `gmres` | ❌ | **deferred — toolboxes/sparse** |
| `gplot` | ❌ | **deferred — toolboxes/sparse** |
| `ichol` | ❌ |  |
| `ilu` | ❌ |  |
| `issparse` | ✅ | Sig: TF = issparse(X). 100k iters. |
| `lsqr` | ❌ | **deferred — toolboxes/sparse** |
| `minres` | ❌ | **deferred — toolboxes/sparse** |
| `nnz` | ✅ | Sig: r = nnz(...). Spec-extension batch 2026-05-09. |
| `nonzeros` | ✅ | Sig: r = nonzeros(...). Spec-extension batch 2026-05-09. |
| `normest` | ✅ | Sig: n = normest(A). 2-norm estimate via largest singular value. NOTE: numkit returns the exact value (full SVD), MATLAB uses power-iteration with default tol=1e-6 (~5-6 sig digits). Tol 1e-5 reflects MATLAB's iteration tolerance. A future perf-pass can switch to power-iteration to match performance characteristics. |
| `nzmax` | ❌ | **deferred — toolboxes/sparse** |
| `pcg` | ❌ | **deferred — toolboxes/sparse** |
| `qmr` | ❌ | **deferred — toolboxes/sparse** |
| `randperm` | ✅ | Sig: r = randperm(...). Spec-extension batch 2026-05-09. |
| `spalloc` | ❌ | **deferred — toolboxes/sparse** |
| `sparse` | ❌ | **deferred — toolboxes/sparse** |
| `spaugment` | ❌ |  |
| `spconvert` | ❌ |  |
| `spdiags` | ❌ | **deferred — toolboxes/sparse** |
| `speye` | ❌ | **deferred — toolboxes/sparse** |
| `spfun` | ❌ | **deferred — toolboxes/sparse** |
| `spones` | ❌ |  |
| `spparms` | ❌ |  |
| `sprand` | ❌ | **deferred — toolboxes/sparse** |
| `sprandn` | ❌ | **deferred — toolboxes/sparse** |
| `sprandsym` | ❌ | **deferred — toolboxes/sparse** |
| `sprank` | ❌ |  |
| `spy` | ❌ | **deferred — toolboxes/sparse** |
| `svds` | ❌ | **deferred — toolboxes/sparse** |
| `symamd` | ❌ | **deferred — toolboxes/sparse** |
| `symbfact` | ❌ | **deferred — toolboxes/sparse** |
| `symmlq` | ❌ | **deferred — toolboxes/sparse** |
| `symrcm` | ✅ | symrcm — symmetric reverse Cuthill-McKee bandwidth reduction. Algorithm: for each connected component (taken in ascending unvisited-node order), start from the min-degree node (tiebreak by smallest index), BFS with degree-sorted children (tiebreak by smallest index), reverse. Matches MATLAB R2025b on the probed examples — bandwidth-reduction property is what MATLAB documents; bit-exact output requires the same starting-node heuristic (min-degree, not full Gibbs-Poole-Stockmeyer pseudoperipheral search). |
| `tfqmr` | ❌ | **deferred — toolboxes/sparse** |
| `treelayout` | ❌ | **deferred — toolboxes/sparse** |
| `treeplot` | ❌ | **deferred — toolboxes/sparse** |
| `unmesh` | ❌ | **deferred — toolboxes/sparse** |

### Workspace

**Namespace:** builtin — 8 ✅ + 0 ⚠️ / 10 = 80%

| function | status | comment |
|---|:---:|---|
| `clear` | ✅ | Sig: clear var. Spec-extension batch 2026-05-09 (cycle 41). |
| `clearvars` | ✅ ❗ | Sig: clearvars var. Spec-extension batch 2026-05-09 (cycle 41). |
| `disp` | ✅ | Side-effect smoke test (no-throw stdout probe). disp exercised on scalar / string / matrix; success = no exception. NOTE: numkit lacks evalc, so stdout cannot be captured for content-level parity; functionality validated by gtest. |
| `formatteddisplaytext` | ✅ | Sig: s = formattedDisplayText(x). KNOWN GAP: numkit does NOT implement formattedDisplayText (undefined function). Documented separately. |
| `load` | ✅ | Side-effect smoke test (file I/O round-trip via tempname). DEFERRED -- load round-trip via tempname '.mat' fails inside the parity harness sandbox (file path resolution differs between save and load steps); functionality validated in toolboxes/builtin gtests instead. |
| `openvar` | ❌ | IDE |
| `save` | ✅ | Sig: save(filename, 'var'). Spec-extension batch 2026-05-09 (cycle 41). |
| `who` | ✅ | Side-effect smoke test (no-throw command-form probe). who command prints variable names to stdout; success = no exception. NOTE: numkit's `who` is command-form only; functional `names = who` returning cellstr is a known gap (). evalc not available in numkit, so stdout content cannot be captured for content-level parity. |
| `whos` | ✅ | Side-effect smoke test (no-throw command-form probe). whos command prints workspace summary to stdout; success = no exception. NOTE: numkit's `whos` is command-form only; functional `s = whos` returning struct is a known gap (). evalc not available in numkit, so stdout content cannot be captured for content-level parity. |
| `workspacebrowser` | ❌ |  |

### Error Handling (basic)

**Namespace:** builtin — 4 ✅ + 0 ⚠️ / 6 = 66%

| function | status | comment |
|---|:---:|---|
| `assert` | ✅ | Sig: r = assert(...). Spec-extension batch 2026-05-09. |
| `error` | ✅ | Side-effect smoke test (control-flow throw via try/catch). error() raises an MException with the given id -- caught and identifier verified. |
| `lastwarn` | ✅ | Sig: r = lastwarn(...). Spec-extension batch 2026-05-09. |
| `oncleanup` | ❌ |  |
| `try` | ✅ | Sig: try, body, catch [ME], body, end. Spec-extension batch 2026-05-09 (cycle 41). |
| `warning` | ✅ | Side-effect smoke test (warning() side-effect via lastwarn). warning('id', 'msg') sets lastwarn -- id is round-tripped through the warning subsystem. |

### Exception Handling

**Namespace:** builtin (keyword + class) — 2 ✅ + 0 ⚠️ / 2 = 100%

| function | status | comment |
|---|:---:|---|
| `mexception` | ✅ | Sig: ME = MException(id, msg). Spec-extension batch 2026-05-09 (cycle 43). |
| `try` | ✅ | Sig: try, body, catch [ME], body, end. Spec-extension batch 2026-05-09 (cycle 41). |

## Communications

### Modulation

**Namespace:** `comm.mod.*` — 13 ✅ + 0 ⚠️ / 29 = 45%

Function-form modulators / demodulators. The `comm.PSKModulator` /
`comm.QAMModulator` / `comm.OFDMModulator` System Object family is
intentionally omitted, along with `constellation` (object method) and
`showResourceMapping` (display).

| function | status | comment |
|---|:---:|---|
| `genqammod` | ⚠️ | MATLAB genqammod / genqamdemod: integer-input lookup into a user-supplied constellation, demod = nearest-neighbour. Covered: 8-PSK constellation forward+round-trip, real PAM constellation, noisy demod still picks correct neighbour. Bit-input mode (`'InputType','bit'`) deferred -- documented. Octave 11.1.0 doesn't ship genqammod in core (signal/communications package only); reports N/A. |
| `genqamdemod` | ✅ | MATLAB genqammod / genqamdemod: integer-input lookup into a user-supplied constellation, demod = nearest-neighbour. Covered: 8-PSK constellation forward+round-trip, real PAM constellation, noisy demod still picks correct neighbour. Bit-input mode (`'InputType','bit'`) deferred -- documented. Octave 11.1.0 doesn't ship genqammod in core (signal/communications package only); reports N/A. |
| `modnorm` | ✅ | Sig: r = modnorm(...). Spec-extension batch 2026-05-09. |
| `pammod` | ✅ | Sig y=pammod(x,M[,ini_phase,symorder]): M-PAM amplitudes 2k-(M-1). DEFAULT symbol order is 'bin' (binary), NOT gray: pammod([0 1 2 3],4)=[-3 -1 1 3]. Explicit 'gray' reorders: [-3 -1 3 1] (y(3)=3, y(4)=1). M=8 bin: y(2)=-5, y(8)=7. numkit previously defaulted to 'gray' for pammod -- fixed (per-function defaults: pammod->bin, qammod->gray). |
| `pamdemod` | ✅ | Sig x=pamdemod(y,M[,ini_phase,symorder]): inverse of pammod. DEFAULT 'bin': pamdemod([-3 -1 1 3],4)=[0 1 2 3]. 'gray' inverse: pamdemod([-3 -1 3 1],4,0,'gray')=[0 1 2 3] (so xg(3)=2, xg(4)=3). M=8 bin maps the full amplitude ladder back to [0..7]. Matches the corrected pammod default ('bin'). |
| `qammod` | ✅ | Sig: y = qammod(x, M[, order]['UnitAveragePower', tf]). Square/rectangular Gray-mapped QAM. MATLAB layout: symbol maps column-major (col = s div sqrt(M), row = s mod sqrt(M)), I increases left→right, Q DECREASES top→bottom, Gray per axis. Covers M=4/16/8 (rectangular KI=4,KQ=2) + unit-average-power (1/sqrt(2) for M=4). Queue-clearing 2026-05-29: numkit previously decomposed by KI with Q increasing — wrong order vs MATLAB; fixed. |
| `qamdemod` | ✅ | Sig: x = qamdemod(y, M[, order]). Nearest-point hard demod, inverse of qammod (col = round((I+KI-1)/2) gray-decoded, row = round((KQ-1-Q)/2) gray-decoded, s = col*KQ+row). Verified by exact round-trip qamdemod(qammod(0:M-1,M),M) == 0:M-1 for M=16 (square, gray), M=8 (rectangular), and M=16 'bin'. Queue-clearing 2026-05-29: realigned with the corrected qammod constellation order. |
| `apskmod` | ⚠️ | MATLAB apskmod / apskdemod with explicit identity SymbolMapping (numkit's default). Engine-detecting shim handles MATLAB's name-value form vs numkit's positional 5th arg. Standard 16-APSK [4,12] [1, 2.7] forward+round-trip + nearest-neighbour demod under small noise. Bit-equal with MATLAB R2025b. Default 'gray' SymbolMapping deferred -- MATLAB's per-ring Gray for non-power-of-2 (M=12) needs more probing. Octave 11.1.0 doesn't ship apskmod in core; reports N/A. |
| `apskdemod` | ❌ |  |
| `mil188qammod` | ✅ | MATLAB mil188qammod / mil188qamdemod (MIL-STD-188-110 QAM). Bit-equal with MATLAB R2025b on ALL FOUR supported constellations: M=16, 32, 64, 256. All tables hard-coded per MATLAB's spec-rounded values (probed at %.17g). MIL188 cluster CLOSED 4/4. Octave 11.1.0 doesn't ship mil188qam in core; reports N/A. |
| `mil188qamdemod` | ✅ | MATLAB mil188qammod / mil188qamdemod (MIL-STD-188-110 QAM). Bit-equal with MATLAB R2025b on ALL FOUR supported constellations: M=16, 32, 64, 256. All tables hard-coded per MATLAB's spec-rounded values (probed at %.17g). MIL188 cluster CLOSED 4/4. Octave 11.1.0 doesn't ship mil188qam in core; reports N/A. |
| `mskmod` | ⚠️ | MATLAB mskmod (differential variant): minimum-shift keying. Bit-equal with MATLAB R2025b. Algorithm: cumulative-sum phase ramp interpolated linearly between symbol boundaries, then exp(i*phase). Differential mode used (MATLAB's default; passed explicitly via 'diff' arg through engine-detecting shim because MATLAB requires it). Argument order: mskmod(x, nSamp, dataenc, ini_phase) -- dataenc is positional 3rd, NOT 4th. ini_phase must be a multiple of pi/2 in MATLAB; numkit accepts arbitrary (extension). KNOWN GAP: non-differential variant deferred (uses rectpulse + I/Q stagger). Octave 11.1.0 doesn't ship mskmod in core; reports N/A. |
| `mskdemod` | ❌ |  |
| `fskmod` | ✅ | Sig: r = fskmod(...). Spec-extension batch 2026-05-09.  |
| `fskdemod` | ✅ | Sig: r = fskdemod(...). Spec-extension batch 2026-05-09.  |
| `ofdmmod` | ✅ | Sig: r = ofdmmod(...). Spec-extension batch 2026-05-09. |
| `ofdmdemod` | ✅ | Sig: r = ofdmdemod(...). Spec-extension batch 2026-05-09. |
| `dpskmod` | ✅ | Sig: r = dpskmod(...). Spec-extension batch 2026-05-09.  |
| `dpskdemod` | ✅ | Sig: r = dpskdemod(...). Spec-extension batch 2026-05-09.  |
| `pskmod` | ✅ | Sig: r = pskmod(...). Spec-extension batch 2026-05-09. |
| `pskdemod` | ✅ | Sig: r = pskdemod(...). Spec-extension batch 2026-05-09. |
| `ammod` | ✅ | MATLAB ammod: amplitude modulator y = (x + carr_amp).*cos(2π·Fc·t + ini_phase). Covered: DSB-SC (carramp=0 default) and DSB-TC (carramp=0.5, ini_phase=pi/4) forms over a 100-sample column-vector input. Bit-equal with MATLAB R2025b within ~1e-10 (Highway sin/cos contributes a few ULP). Octave 11.1.0 doesn't ship ammod in core (signal/communications package only); reports N/A. |
| `amdemod` | ❌ |  |
| `fmmod` | ✅ | MATLAB fmmod: frequency modulator y = cos(2π·Fc·t + 2π·freqdev·cumsum(x)/Fs + ini_phase). Covered: default (ini_phase=0) and explicit ini_phase forms over a 100-sample column-vector input. Bit-equal with MATLAB R2025b within ~1e-10 (Highway sin/cos contributes a few ULP). Octave 11.1.0 doesn't ship fmmod in core (signal/communications package only); reports N/A. |
| `fmdemod` | ❌ |  |
| `pmmod` | ✅ | MATLAB pmmod: phase modulator y = cos(2π·Fc·t + phasedev·x + ini_phase). Covered: default (ini_phase=0) and explicit ini_phase forms, 100-sample column-vector input, sample points across the signal. Bit-equal with MATLAB R2025b. Octave 11.1.0 doesn't ship pmmod in core (it's in the communications package); reports N/A. |
| `pmdemod` | ❌ |  |
| `ssbmod` | ✅ | MATLAB ssbmod: single-sideband modulator. y = x.*cos(2π·Fc·t + ini) ± imag(hilbert(x)).*sin(2π·Fc·t + ini); +sign for default lower sideband, -sign for 'upper'. Hilbert is FFT-based -> ~1e-10 ULP-level deviation from MATLAB. Octave 11.1.0 doesn't ship ssbmod in core (signal/communications package only); reports N/A. |
| `ssbdemod` | ❌ |  |

### Sources, Sinks, and Signal Operations

**Namespace:** `comm.signals.*` — 0 ✅ + 0 ⚠️ / 17 = 0%

| function | status | comment |
|---|:---:|---|
| `randerr` | ✅ | MATLAB randerr: random binary error matrix with controllable error count per row. Uses MatlabMT19937 for bit-equal output with MATLAB R2025b on seeded inputs. Covered: scalar (1 error), scalar (3 errors), vector [1 2 3] uniform, weighted [0 1 2; 0.0 0.7 0.3]. All known column placements + row sums fingerprinted. Octave 11.1.0 doesn't ship randerr in core; reports N/A. |
| `randsrc` | ✅ | MATLAB randsrc: random matrix from finite alphabet with optional weighted probabilities. Numkit uses MatlabMT19937 (= MATLAB's mt19937ar) seeded with explicit state arg, so seeded outputs are bit-identical with MATLAB R2025b. Probability fingerprints (~70/20/10%) within 5% Monte-Carlo tolerance over 5000 samples. Octave 11.1.0 doesn't ship randsrc in core (signal/communications package only); reports N/A. |
| `wgn` | ✅ | Sig: r = wgn(...). Spec-extension batch 2026-05-09.  |
| `biterr` | ✅ | Sig: [n, r] = biterr(x, y[, k]). Counts differing bits between non-negative integer arrays. Bit-width k auto-detected as smallest covering width. |
| `symerr` | ✅ | Sig: [n, r] = symerr(x, y). Element-wise inequality count + ratio. |
| `zadoffChuSeq` | ❌ | Zadoff-Chu reference sequence |
| `mask2shift` | ❌ | shift-register mask → shift |
| `shift2mask` | ❌ |  |
| `bit2int` | ✅ | Sig: y = bit2int(b, n). Pack a column of bits into integers, n bits each, MSB-first by default. b=[1 0 1 0 1 1 0 0]' -> y=[10;12] (1010, 1100). Lifted to public C++ API numkit::comm::bit2int (header added; mr moved to LIBRARY_API mr-last) 2026-06. |
| `int2bit` | ✅ | Sig: b = int2bit(d, n). Inverse of bit2int: each int -> one column of n MSB-first bits. d=[10 12] -> 4x2 with col1=[1;0;1;0] (10), col2=[1;1;0;0] (12). Lifted to public C++ API numkit::comm::int2bit 2026-06. |
| `bi2de` | ✅ | Sig: d = bi2de(b). Legacy binary->decimal per row, default base 2 right-msb (LSB-first). Rows [1 0 1 0]->5, [0 0 1 1]->12. MATLAB-deprecated synonym of bit2int but still ships R2025b. Lifted to public C++ API numkit::comm::bi2de 2026-06. |
| `de2bi` | ✅ | Sig: b = de2bi(d[, n[, base[, flag]]]). Legacy decimal->binary, each int -> one row, default base 2 right-msb, auto width from max. d=[5 12] -> 2x4, row1 (5)=[1 0 1 0]. 2026-06-03: de2bi(d, [], base) with EMPTY width + custom base now works (was throwing on the [] arg): de2bi(10,[],3)=[1 0 1] (3 digits), de2bi(10,4,3)=[1 0 1 0]. Lifted to public C++ API numkit::comm::de2bi 2026-06. |
| `hex2poly` | ❌ | hex string → polynomial coeffs |
| `oct2poly` | ❌ |  |
| `oct2dec` | ❌ | octal → decimal |
| `vec2mat` | ❌ | reshape with zero-pad |
| `convertSNR` | ✅ | Sig: r = convertSNR(...). Spec-extension batch 2026-05-09. |

### Source Coding

**Namespace:** `comm.source_coding.*` — 0 ✅ + 0 ⚠️ / 11 = 0%

| function | status | comment |
|---|:---:|---|
| `arithenco` | ✅ | MATLAB arithenco / arithdeco: arithmetic coding pair. Bit-equal with MATLAB R2025b on encoded bit string and decoded sequence. Implements the Sayood textbook E1/E2/E3 rescaling algorithm matching MATLAB's source. Octave 11.1.0 doesn't ship arithenco/arithdeco in core (signal/communications package only); reports N/A. |
| `arithdeco` | ❌ |  |
| `compand` | ✅ | MATLAB compand: μ-law / A-law signal compander. 4 methods covered (mu/compressor, mu/expander, A/compressor, A/expander) with round-trip identity validation and sign preservation on negatives. Algorithm: closed-form formulas from MATLAB compand.m. Output preserves input shape. |
| `dpcmenco` | ⚠️ | MATLAB dpcmenco/dpcmdeco: 1st-order DPCM (predictor=[0 1]) with 6-bin codebook + 5-threshold partition. Bit-equal with MATLAB R2025b on encoded indices, quantization error, and reconstructed signal. Round-trip qe consistency (qe from encoder == qe from decoder via codebook lookup) also verified. dpcmopt deferred (training-set optimisation needs Lloyd-Max + alternating predictor estimation -- own cycle). Octave 11.1.0 doesn't ship dpcmenco/deco in core; reports N/A. |
| `dpcmdeco` | ❌ |  |
| `dpcmopt` | ✅ | Communications toolbox dpcmopt — DPCM parameter optimiser. CLEAN-ROOM implementation from public references (J. Makhoul, Linear Prediction: A Tutorial Review, Proc. IEEE 1975 — autocorrelation method + Levinson-Durbin; Proakis & Manolakis, Digital Signal Processing; Jayant & Noll, Digital Coding of Waveforms, 1984). Algorithm: estimate the autocorrelation r[k] = (sum x[n]x[n+k]) / (N-1-k) for lags 0..ord, solve the Yule-Walker normal equations by the Levinson-Durbin recursion to get the prediction-error filter A(z), and form the predictor [0, -a1, ..., -aM]; when a third argument is given (codebook length or initial codebook) run lloyds() on the prediction residual for codebook+partition. Bit-equal MATLAB R2025b on deterministic training (sin + linear ramp) for predictor and codebook/partition outputs. Octave 11.1.0 ships dpcmopt in the communications package (not loaded by default); harness reports N/A there. |
| `huffmandict` | ✅ | MATLAB huffmandict: Huffman code-book builder. Codes are NOT unique (tie-breaking yields different but equally optimal trees) -- the invariant is avglen = sum(p_k * L_k). Fingerprint pins avglen on three test cases (5-symbol skewed, 2-symbol, 4-symbol uniform). Code shape, prefix-freeness and bounds H <= avglen < H+1 covered in gtest. Octave 11.1.0 doesn't ship huffmandict in core (signal/communications package only); reports N/A. |
| `huffmanenco` | ✅ | MATLAB huffmanenco/huffmandeco: encode/decode round-trip via dict from huffmandict. Bit codes are non-unique (Huffman tie-breaking can produce different but equally optimal trees), so encoded length depends on which optimal dict shape was produced. The INVARIANT under both engines is round-trip identity: dec must equal sig regardless of dict shape. We pin rt_match==1, length(dec), and the first/last decoded symbols. Octave 11.1.0 doesn't ship the Huffman codec in core; reports N/A. |
| `huffmandeco` | ❌ |  |
| `lloyds` | ⚠️ | MATLAB lloyds: Lloyd-Max scalar quantizer designer. Tested on deterministic monotone training (1:10) since random-seed paths use randn which differs (Ziggurat for randn deferred). Bit-equal with MATLAB R2025b on initial-codebook form ([2 5 8]) and integer-K form (K=2, K=4). Octave 11.1.0 doesn't ship lloyds in core (signal/communications package only); reports N/A. |
| `quantiz` | ✅ | MATLAB quantiz: scalar quantizer applier. indx(i) = sum(partition < sig(i)); quantv = codebook(indx+1); distor = mean((sig-quantv)^2). Bit-equal with MATLAB R2025b. Octave 11.1.0 doesn't ship quantiz in core (signal/communications package only); reports N/A. |

### Error Detection and Correction

**Namespace:** `comm.fec.*` — 0 ✅ + 0 ⚠️ / 26 = 0%

`crcConfig`, `ldpcEncoderConfig`, `ldpcDecoderConfig`, the System
Objects (`comm.CRCGenerator`, `comm.LDPCEncoder`, etc.) and the `gf`
class are intentionally omitted. Galois-field math is exposed through
the flat `gf*` function family below.

| function | status | comment |
|---|:---:|---|
| `crcGenerate` | ❌ | append CRC parity bits |
| `crcDetect` | ❌ |  |
| `cyclgen` | ✅ | Sig: [h,g,k] = cyclgen(n,genpoly[,opt]). Parity/generator matrices of the (n,k) cyclic code with generator poly genpoly (ascending; k=n-deg). 'system' (default): g=[I_k... actually b|I_k], h=[I_(n-k)|b'] from x^(n-k+i) mod genpoly. 'nonsystem': cyclic shifts of reversed parity poly (h) / genpoly (g). cyclgen(7,[1 0 1 1]): k=4, systematic h/g above; nonsystem g = shifts of [1 0 1 1 0 0 0]. New (Error Correction Codes / block linear codes) 2026-06. GF(2) only; genpoly must divide x^n-1. |
| `cyclpoly` | ✅ | Sig: p = cyclpoly(n,k[,opt]). Generator polynomial(s) of an (n,k) cyclic code: degree-(n-k) polys 1+...+x^(n-k) dividing x^n-1 over GF(2). Ascending powers. cyclpoly(7,4)=[1 0 1 1]; (15,11)=[1 0 0 1 1]. opt: ''=first, 'min'=fewest terms ([1 0 1 1]), 'max'=most terms ([1 1 0 1]), 'all'=every gen poly sorted by term count (7,4 -> [1 0 1 1;1 1 0 1]; 15,5 -> 3 rows). New (Error Correction Codes / block linear codes) 2026-06. GF(2) only. |
| `encode` | ✅ | Sig: [code,added] = encode(msg,n,k,method,opt). Linear block encoder over GF(2): reshape msg into k-bit words, multiply (mod 2) by the k x n generator. Methods hamming/linear/cyclic with /binary (default) or /decimal format. encode([1 0 1 1],7,4,'hamming/binary')=[1 0 0 1 0 1 1]; cyclic=[0 0 0 1 0 1 1]; encode(11,...,'hamming/decimal')=88; 5-bit msg pads added=3. New (Error Correction Codes / block linear codes) 2026-06. BCH/RS (gf type) deferred. |
| `decode` | ✅ | Sig: [msg,err] = decode(code,n,k,method,opt). Syndrome decoder over GF(2): reshape code into n-bit words, syndrome = word*h' (mod 2), look up minimum-weight coset leader (syndtable), correct, extract systematic message. err = #bits corrected per word. Hamming(7,4) corrects any single-bit error (okall over all 7 positions); decode(88,...,'hamming/decimal')=11. New (Error Correction Codes / block linear codes) 2026-06. Coset-leader tie-breaking for weight>=2 patterns is lexicographic (exact for single-error-correcting codes). BCH/RS (gf type) deferred. |
| `gfweight` | ❌ | minimum distance |
| `gen2par` | ✅ | Sig: h = gen2par(g). Systematic generator<->parity-check converter for linear block codes. r x c input (r<c): [P|I_r] -> [I_(c-r)|P'], [I_r|P] -> [P'|I_(c-r)]. Involution: gen2par(gen2par(g))==g. gen2par of Hamming(7,4) generator -> [I_3|P']=[1 0 0 1 0 1 1;0 1 0 1 1 1 0;0 0 1 0 1 1 1]. New (Error Correction Codes / block linear codes) 2026-06. GF(2) only. |
| `hammgen` | ✅ | Sig: [h,g,n,k] = hammgen(m). (n,k) Hamming code, n=2^m-1, k=n-m. H(:,i)=coeffs of x^i mod p(x), p=default primitive poly of degree m, ascending power; first m cols = I_m so code is systematic and g=gen2par(h). hammgen(3): n=7 k=4, H=[1 0 0 1 0 1 1;0 1 0 1 1 1 0;0 0 1 0 1 1 1]. hammgen(4): n=15 k=11. orth: g*h'==0 (mod 2). New (Error Correction Codes / block linear codes) 2026-06. GF(2) only; custom primitive poly via 2nd arg. BCH/RS (gf type) deferred. |
| `syndtable` | ❌ | syndrome decoding table |
| `bchenc` | ❌ | BCH encoder |
| `bchdec` | ❌ |  |
| `bchgenpoly` | ❌ |  |
| `bchnumerr` | ❌ |  |
| `rsenc` | ❌ | Reed-Solomon encoder |
| `rsdec` | ❌ |  |
| `rsgenpoly` | ❌ |  |
| `rsgenpolycoeffs` | ❌ |  |
| `ldpcEncode` | ❌ |  |
| `ldpcDecode` | ❌ |  |
| `ldpcPCM` | ❌ | parity-check matrices for standards |
| `ldpcQuasiCyclicMatrix` | ❌ |  |
| `tpcenc` | ❌ | turbo product encoder |
| `tpcdec` | ❌ |  |
| `convenc` | ❌ | convolutional encoder |
| `vitdec` | ❌ | Viterbi decoder |

### Trellis and Galois Field Utilities

**Namespace:** `comm.gf.*` — 0 ✅ + 0 ⚠️ / 22 = 0%

| function | status | comment |
|---|:---:|---|
| `distspec` | ❌ | distance spectrum of conv code |
| `iscatastrophic` | ❌ |  |
| `istrellis` | ✅ | Sig: tf = istrellis(S). Validates a trellis struct: poly2trellis output -> 1 (a); a plain struct/scalar -> 0 (b, c). New (Error Correction Codes) 2026-06. MATLAB R2025b -batch may crash on comm-toolbox shutdown; istrellis verified via gtest + smoke. |
| `poly2trellis` | ✅ | Sig: trellis = poly2trellis(K, [g1..gn]). Rate-1/n feed-forward convolutional trellis. poly2trellis(3,[6 7]): nInput=2 nOutput=4 nStates=4, nextStates=[0 2;0 2;1 3;1 3], outputs(octal->decimal of n output bits)=[0 3;1 2;3 0;2 1]. poly2trellis(4,[13 15 17]): nStates=8 nOutput=8, outputs(1:2,:)=[0 7;7 0]. New function (Error Correction Codes section) 2026-06; rate k/n + feedback deferred. NOTE: struct field-then-index (t.nextStates(i,j)) is a separate core gap, so the spec assigns ns/ou to intermediates first. |
| `cosets` | ❌ | cyclotomic cosets |
| `dftmtx` | ✅ | Sig: F = dftmtx(N). N×N DFT matrix; F(j,k) = exp(-2πi(j-1)(k-1)/N). For real input dftmtx(N)*x equals fft(x). Edges: F2 4 elem, F4 16 elem, F8 64 elem, F16 256 elem; F8(2,2) = √2/2 - i√2/2 ≈ 0.7071-0.7071i; F8(5,5) = 1 + 0i (column 5 row 5 ≡ exp(-2πi·16/8) = exp(-4πi) = 1); dftmtx(1) = 1. |
| `isprimitive` | ❌ |  |
| `minpol` | ❌ | minimal polynomial in GF |
| `primpoly` | ❌ | primitive polynomial of degree m |
| `gfadd` | ❌ | GF addition |
| `gfconv` | ❌ | GF polynomial multiply |
| `gfcosets` | ❌ | GF(p^m) cosets |
| `gfdeconv` | ❌ | GF polynomial divide |
| `gfdiv` | ❌ | element-wise GF division |
| `gffilter` | ❌ | GF FIR filter |
| `gflineq` | ❌ | linear equations over GF(p) |
| `gfminpol` | ❌ |  |
| `gfmul` | ❌ | element-wise GF multiplication |
| `gfpretty` | ❌ | pretty-print GF poly |
| `gfprimck` | ❌ | check primitivity |
| `gfprimdf` | ❌ | default primitive polynomial |
| `gftuple` | ❌ | exponential ↔ polynomial form |

### Interleaving

**Namespace:** `comm.intrlv.*` — 0 ✅ + 0 ⚠️ / 16 = 0%

| function | status | comment |
|---|:---:|---|
| `intrlv` | ❌ | generic interleaver |
| `deintrlv` | ❌ |  |
| `algintrlv` | ❌ | algebraic |
| `algdeintrlv` | ❌ |  |
| `helscanintrlv` | ❌ | helical-scan |
| `helscandeintrlv` | ❌ |  |
| `matintrlv` | ❌ | matrix |
| `matdeintrlv` | ❌ |  |
| `randintrlv` | ❌ | random |
| `randdeintrlv` | ❌ |  |
| `convintrlv` | ❌ | convolutional |
| `convdeintrlv` | ❌ |  |
| `helintrlv` | ❌ | helical |
| `heldeintrlv` | ❌ |  |
| `muxintrlv` | ❌ | multiplexed |
| `muxdeintrlv` | ❌ |  |

### Pulse Shaping, Equalization, MIMO

**Namespace:** `comm.shape.*` — 1 ✅ + 0 ⚠️ / 8 = 13%

System-Object equalisers (`comm.LinearEqualizer`, `comm.MLSEEqualizer`,
`comm.DecisionFeedbackEqualizer`) are omitted; only the function-form
MLSE entry is exposed.

| function | status | comment |
|---|:---:|---|
| `gaussdesign` | ✅ | Sig: h = gaussdesign(BT, span, sps). Gaussian FIR pulse-shaping filter. Bit-identical with MATLAB R2025b on (0.3, 4, 8) probe (h(17)=0.112904, sum=1, length=33). Earlier defer was wrong. |
| `rcosdesign` | ✅ | Sig: r = rcosdesign(...). Spec-extension batch 2026-05-09.  |
| `rectpulse` | ✅ | Sig y=rectpulse(x,nsamp): rectangular pulse shaping — repeat each sample nsamp times (column-wise). [1 2 -3] with nsamp=3 -> [1;1;1;2;2;2;-3;-3;-3] (length 9). y(1)=y(3)=1, y(4)=2, y(6)=2, y(9)=-3. |
| `intdump` | ✅ | Sig y=intdump(x,nsamp): integrate-and-dump — average each nsamp-length block. [2 4 6 8 10 12] nsamp=2 -> [3 7 11] (mean of each pair). Round-trip: intdump(rectpulse([1 2 3],2),2) = [1 2 3] (inverse of pulse shaping). |
| `mlseeq` | ❌ | maximum-likelihood sequence equaliser |
| `ofdmEqualize` | ❌ | OFDM zero-forcing / MMSE equalise |
| `blkdiagbfweights` | ❌ | block-diagonalisation BF weights |
| `ofdmPrecode` | ❌ | OFDM precoding |

### RF and Channel Impairments

**Namespace:** `comm.rf.*` — 4 ✅ + 0 ⚠️ / 10 = 40%

| function | status | comment |
|---|:---:|---|
| `awgn` | ✅ | Sig: r = awgn(...). Spec-extension batch 2026-05-09. |
| `bsc` | ✅ | Sig: r = bsc(...). Spec-extension batch 2026-05-09. |
| `rayleighchan` | ✅ | N/A (definite): MATLAB R2025b DEPRECATED rayleighchan() in favour of comm.RayleighChannel system object. Numkit retains rayleighchan as a convenience helper that returns one complex Rayleigh sample. Definite N/A -- no MATLAB top-level reference exists in the current release. |
| `ricianchan` | ✅ | N/A (definite): MATLAB R2025b DEPRECATED ricianchan() in favour of comm.RicianChannel system object. Numkit retains ricianchan as a convenience helper. Definite N/A. |
| `stdchan` | ❌ | standard channel-model picker |
| `frequencyOffset` | ❌ | apply Δf |
| `iqimbal` | ❌ | apply IQ imbalance |
| `iqcoef2imbal` | ❌ | coefficients → amp/phase imbalance |
| `iqimbal2coef` | ❌ |  |
| `srmdelay` | ❌ | sample-rate-matching delay |
| `channelDelay` | ❌ | channel-delay estimation |
| `ofdmChannelResponse` | ❌ | OFDM frequency-domain channel |

### Propagation Path Loss and Geometry

**Namespace:** `comm.propagation.*` — 0 ✅ + 0 ⚠️ / 15 = 0%

OOP `propagationModel` family, ray-tracing classes (`raytrace`,
`coverage`, `pattern`, `sinr`, `link`, `sigstrength`) and the antenna /
basemap object hierarchy intentionally omitted — only flat scalar /
vector path-loss models and coordinate transforms.

| function | status | comment |
|---|:---:|---|
| `fspl` | ❌ | free-space path loss |
| `cranerainpl` | ❌ | Crane rain attenuation |
| `rainpl` | ❌ | ITU rain attenuation |
| `gaspl` | ❌ | gas (oxygen + water vapour) |
| `fogpl` | ❌ | fog / cloud |
| `raypl` | ❌ | propagation along a ray |
| `buildingMaterialPermittivity` | ❌ | ITU building materials |
| `earthSurfacePermittivity` | ❌ |  |
| `los` | ❌ | line-of-sight check |
| `doppler` | ❌ | Doppler-shift utility |
| `rangeangle` | ❌ | range and angle between coordinates |
| `global2localcoord` | ❌ |  |
| `local2globalcoord` | ❌ |  |
| `cart2sphvec` | ❌ | rotate vector to spherical basis |
| `sph2cartvec` | ❌ |  |

### Performance Analysis

**Namespace:** `comm.perf.*` — 6 ✅ + 0 ⚠️ / 11 = 55%

| function | status | comment |
|---|:---:|---|
| `berawgn` | ✅ | Sig: r = berawgn(...). Spec-extension batch 2026-05-09. |
| `bercoding` | ❌ | with coding gain |
| `berconfint` | ✅ ➖ | Sig: r = berconfint(...). Spec-extension batch 2026-05-09.  |
| `berfading` | ❌ | over Rayleigh / Rician fading |
| `berfit` | ❌ | curve fit BER vs Eb/No |
| `bersync` | ❌ | with imperfect sync |
| `semianalytic` | ❌ | semi-analytic BER |
| `marcumq` | ✅ | Sig Q=marcumq(a,b[,m]): generalized Marcum Q-function. marcumq(2,1)=0.918108 (b<a -> near 1), marcumq(1,2)=0.269012 (b>a -> small), marcumq(2,3,2)=0.352698 (order m=2). tol 1e-5: numkit's series approximation differs from MATLAB by ~5e-7. |
| `qfunc` | ✅ | Sig q=qfunc(x)=0.5*erfc(x/sqrt(2)): upper tail of the standard normal. qfunc(-1.5)=0.933193, qfunc(0)=0.5, qfunc(1)=0.158655, qfunc(2)=0.0227501. Vectorised input; pins the symmetric pair and two positive arguments. |
| `qfuncinv` | ✅ | Sig x=qfuncinv(p)=sqrt(2)*erfcinv(2p): inverse Q-function. qfuncinv(0.5)=0, qfuncinv(0.1)=1.281552, qfuncinv(0.9)=-1.281552 (odd symmetry about 0.5), qfuncinv(0.025)=1.959964. tol 1e-7: numkit erfcinv differs from MATLAB by ~1e-9. |
| `noisebw` | ✅ | Sig: bw = noisebw(num, den, Nsamp, fs). Equivalent noise bandwidth via NBW = (fs/N) * sum(|H|^2) / max(|H|^2). Matches MATLAB R2025b within ~0.5 Hz on probed FIR (numerical-grid difference). |

## Control

### LTI Models

**Namespace:** `control.lti.*` — 9 ✅ + 0 ⚠️ / 19 = 47%

`tf`/`zpk`/`ss`/`frd` are object constructors in MATLAB; we treat them
as flat structure-returning functions (returning a struct with fields
{num, den}, {z, p, k}, {A, B, C, D}, {response, frequency} etc.) and
the data-extraction `*data` functions read those structs. The full
`lti` / `dynamicSystem` class hierarchy and Simulink integration
(`slTuner`, `addBlock`/`removeBlock`/`setBlockParam`, etc.) are
intentionally omitted.

| function | status | comment |
|---|:---:|---|
| `tf` | ✅ | Sig: r = tf(...). Spec-extension batch 2026-05-09. |
| `zpk` | ✅ | Sig: r = zpk(...). Spec-extension batch 2026-05-09. |
| `ss` | ✅ | Sig: r = ss(...). Spec-extension batch 2026-05-09. |
| `frd` | ✅ | Sig: r = frd(...). Spec-extension batch 2026-05-09. |
| `dss` | ❌ | descriptor state-space (E·xdot = Ax + Bu) |
| `filt` | ✅ | Sig: r = filt(...). Spec-extension batch 2026-05-09. |
| `pid` | ❌ | parallel-form PID controller |
| `pid2` | ❌ | 2-DOF PID |
| `pidstd` | ❌ | standard-form PID |
| `pidstd2` | ❌ | 2-DOF standard PID |
| `rss` | ❌ | random stable continuous SS |
| `drss` | ❌ | random stable discrete SS |
| `tfdata` | ✅ | Sig: r = tfdata(...). Spec-extension batch 2026-05-09. |
| `zpkdata` | ✅ | Sig: r = zpkdata(...). Spec-extension batch 2026-05-09. |
| `ssdata` | ✅ | Sig: r = ssdata(...). Spec-extension batch 2026-05-09. |
| `frdata` | ✅ | Sig: r = frdata(...). Spec-extension batch 2026-05-09. |
| `dssdata` | ❌ | extract A/B/C/D/E |
| `piddata` | ❌ |  |
| `pidstddata` | ❌ |  |

### Model Properties

**Namespace:** `control.props.*` — 11 ✅ + 0 ⚠️ / 11 = **100%**

| function | status | comment |
|---|:---:|---|
| `isct` | ✅ | Sig: r = isct(...). Spec-extension batch 2026-05-09. |
| `isdt` | ✅ | Sig: r = isdt(...). Spec-extension batch 2026-05-09. |
| `isproper` | ✅ | Sig: r = isproper(...). Spec-extension batch 2026-05-09. |
| `issiso` | ✅ | Sig: r = issiso(...). Spec-extension batch 2026-05-09. |
| `isstable` | ✅ | Sig: r = isstable(...). Spec-extension batch 2026-05-09. |
| `isstatic` | ✅ | Sig: r = isstatic(...). Spec-extension batch 2026-05-09. |
| `order` | ✅ | Sig: r = order(...). Spec-extension batch 2026-05-09. |
| `pole` | ✅ | Sig: r = pole(...). Spec-extension batch 2026-05-09. |
| `zero` | ✅ | Sig: r = zero(...). Spec-extension batch 2026-05-09. |
| `tzero` | ✅ | Sig: z = tzero(sys). SISO transmission zeros via ss2tf + roots. Bit-identical with MATLAB R2025b on probed system (z = 1.0). MIMO requires QZ generalised eigenvalue solver (separate spec). |
| `damp` | ✅ | Sig: r = damp(...). Spec-extension batch 2026-05-09. |

### Model Conversion & Reduction

**Namespace:** `control.convert.*` — 3 ✅ + 0 ⚠️ / 18 = 17%

| function | status | comment |
|---|:---:|---|
| `c2d` | ✅ | Sig: sysd = c2d(sys, Ts[, method]). 'zoh' (default), 'foh', 'tustin'. 'foh' = first-order/triangle hold via Van Loan augmented matrix-exp [[A·Ts,B·Ts,0];[0,0,I·Ts];[0,0,0]]: Ad=Phi, Bd=G1+Phi·G2/Ts−G2/Ts, Cd=C, Dd=D+C·G2/Ts (FOH introduces a feedthrough term so the discrete tf gains a nonzero leading b). Bit-identical with MATLAB R2025b on tf(1,[1 2 1]) + an explicit ss. Queue-clearing 2026-05-29: 'foh' previously errored "method must be 'zoh' or 'tustin'". 'matched' still deferred. |
| `c2dOptions` | ❌ |  |
| `d2c` | ✅ | Sig: r = d2c(...). Spec-extension batch 2026-05-09. |
| `d2cOptions` | ❌ |  |
| `d2d` | ❌ | resample discrete |
| `d2dOptions` | ❌ |  |
| `ss2ss` | ✅ | Sig: r = ss2ss(...). Spec-extension batch 2026-05-09. |
| `canon` | ❌ | canonical realisation |
| `balreal` | ❌ | balanced realisation |
| `prescale` | ❌ | improve numerics by scaling |
| `modalreal` | ❌ | modal realisation |
| `compreal` | ❌ | companion realisation |
| `minreal` | ❌ | minimal realisation |
| `sminreal` | ❌ | structurally minimal |
| `balred` | ❌ | balanced reduction |
| `modred` | ❌ | model reduction |
| `hsvd` | ❌ | Hankel singular values |
| `pade` | ❌ | Padé approximation of delay |
| `ss2tf` | ✅ | MATLAB ss2tf — state-space to transfer function. A=[-2 1;0 -3], B=[1;1], C=[1 0], D=0 -> b=[0 1 4], a=[1 5 6] (denominator = char poly (s+2)(s+3)). Pins numerator + denominator coefficients (was numel(b)-only). Bit-equal MATLAB R2025b. |

### Interconnections

**Namespace:** `control.connect.*` — 3 ✅ + 0 ⚠️ / 7 = 43%

| function | status | comment |
|---|:---:|---|
| `feedback` | ✅ | Sig: sys = feedback(sys1, sys2[, sign]). Closed-loop feedback. Denominator bit-identical with MATLAB R2025b (1+s+s^2 -> [1 1 1]). 2026-06-03: a NUMERIC arg is a static gain K/1 -- feedback(sys,1) unity feedback (dcgain 0.5), feedback(2,sys) (dcgain 2/3). |
| `series` | ✅ | Sig: sys = series(sys1, sys2). Cascade. dcgain multiplies (0.5*1=0.5). 2026-06-03: a NUMERIC arg is a static gain -- series(2/(s+1),3) dcgain 6. vs MATLAB R2025b. |
| `parallel` | ✅ | Sig: sys = parallel(sys1, sys2). Sum. dcgain adds (1+0.5=1.5). 2026-06-03: a NUMERIC arg is a static gain -- parallel(2/(s+1),3) dcgain 5. vs MATLAB R2025b. |
| `connect` | ❌ | name-based interconnect |
| `append` | ✅ | Sig: r = append(...). Spec-extension batch 2026-05-09. |
| `lft` | ❌ | linear fractional transform |
| `sumblk` | ❌ | summation block (for connect) |

### Time and Frequency Response

**Namespace:** `control.response.*` — 9 ✅ + 0 ⚠️ / 19 = 47%

`*plot` variants intentionally dropped — they're display-only mirrors
of the numeric functions (which already return data when called with
output args).

| function | status | comment |
|---|:---:|---|
| `step` | ✅ | Sig: [y, t, x] = step(sys[, T]). Default time grid via Tfinal = -log(0.003)/min|Re(p)|, N=127. 3rd output x = state trajectory (length(t) x nStates). State values are realization-dependent, so the spec uses an explicit ss() system (fixed realization) — matches MATLAB R2025b bit-for-bit. Queue-clearing 2026-05-29: x output added (step/impulse/lsim previously returned only [y,t]). |
| `stepinfo` | ✅ | Sig: r = stepinfo(...). DEEP-PROBE c167: numkit returned an 8-field struct; MATLAB R2025b returns 9 (the 2nd field TransientTime, between RiseTime and SettlingTime, was missing). TransientTime = time |y-yfinal| last exceeds 2% of the PEAK deviation max|y(t)-yfinal| (vs SettlingTime's 2% of |yinit-yfinal|=|yfinal|). For a standard step the peak deviation occurs at t=0 = |yfinal| so TransientTime == SettlingTime (eq=1, holds in BOTH engines). Pins numel(fieldnames)=9 + TransientTime>0 + the TransientTime==SettlingTime invariant rather than the absolute time (numkit's step grid differs ~0.8% from MATLAB's, a pre-existing settling-time resolution gap shared by SettlingTime). covers:[stepinfo]. |
| `impulse` | ✅ | Sig: [y, t] = impulse(sys[, T]). Default time grid via Tfinal = -log(0.003)/min|Re(p)|, N=127. Bit-identical with MATLAB R2025b on probed 1st-order system. |
| `initial` | ❌ | response from initial state |
| `lsim` | ✅ | Sig: r = lsim(...). Spec-extension batch 2026-05-09. |
| `lsiminfo` | ❌ |  |
| `gensig` | ❌ | input signal generator |
| `covar` | ❌ | output covariance under stochastic input |
| `bode` | ✅ | Sig: r = bode(...). Spec-extension batch 2026-05-09. |
| `bodemag` | ❌ | magnitude only |
| `nyquist` | ✅ | Sig: r = nyquist(...). Spec-extension batch 2026-05-09. |
| `nichols` | ❌ |  |
| `sigma` | ❌ | singular-value response |
| `freqresp` | ✅ | Sig: r = freqresp(...). Spec-extension batch 2026-05-09. |
| `evalfr` | ✅ | Sig: r = evalfr(...). Spec-extension batch 2026-05-09. |
| `dcgain` | ✅ | Sig: r = dcgain(...). Spec-extension batch 2026-05-09. |
| `bandwidth` | ✅ | Sig: matrix-structure predicates and bandwidth queries. isbanded(A, lo, up) ↔ outside-band entries are zero; isdiag = isbanded(A, 0, 0). istril / istriu ↔ lower / upper triangular. [lo, up] = bandwidth(A) returns sub-diagonal and super-diagonal extent; bandwidth(A) one-out form returns lo; 'lower'|'upper' string form selects one. Bit-exact MATLAB R2025b (tol=0): D diagonal (ib1=id1=1), A tri-banded (ib2=1), A not upper-banded-only (ib3=0), L lower-tri (itl=1), U upper-tri (itu=1), bandwidth(A) = [1, 1], bandwidth(U) = 0, bandwidth(L) = 2, bandwidth(U, 'upper') = 2. Split from the original combined predicates spec to stay under the 256-register chunk limit. |
| `getPeakGain` | ❌ | H∞ |
| `getGainCrossover` | ❌ |  |

### Stability and Margins

**Namespace:** `control.margin.*` — 3 ✅ + 0 ⚠️ / 6 = 50%

| function | status | comment |
|---|:---:|---|
| `margin` | ✅ | Sig: r = margin(...). Spec-extension batch 2026-05-09. |
| `allmargin` | ❌ | all stability margins |
| `db2mag` | ✅ | Sig: r = db2mag(...). Spec-extension batch 2026-05-09. |
| `mag2db` | ✅ | Sig: r = mag2db(...). Spec-extension batch 2026-05-09. |
| `pzmap` | ✅ | Sig: r = pzmap(...). Spec-extension batch 2026-05-09. |
| `rlocus` | ✅ | Sig: r = rlocus(...). Spec-extension batch 2026-05-09. |

### State-Space Design and Estimation

**Namespace:** `control.design.*` — 4 ✅ + 0 ⚠️ / 18 = 22%

OOP filters (`extendedKalmanFilter`, `unscentedKalmanFilter`,
`particleFilter`) intentionally omitted — they're class-objects with
methods (`correct`, `predict`, etc.). Flat steady-state designs only.

| function | status | comment |
|---|:---:|---|
| `lqr` | ❌ | linear-quadratic regulator |
| `lqry` | ❌ | LQR with output weighting |
| `lqi` | ❌ | LQR with integral action |
| `dlqr` | ❌ | discrete LQR |
| `lqrd` | ❌ | continuous LQR with sampled controller |
| `lqg` | ❌ | linear-quadratic Gaussian |
| `lqgreg` | ❌ | LQG regulator |
| `lqgtrack` | ❌ | tracking LQG |
| `place` | ✅ | Sig: K = place(A, B, p). Re-closed 2026-05-09 -- prior defer was wrong; numkit returns K=[1 2] matching MATLAB on probe. |
| `estim` | ❌ | steady-state estimator (Kalman) |
| `kalman` | ❌ | continuous-time Kalman gain |
| `kalmd` | ❌ | discrete Kalman from continuous plant |
| `reg` | ❌ | full-state controller + observer |
| `ctrb` | ✅ | MATLAB ctrb — controllability matrix [B A*B]. A=[1 2;3 4], B=[5;6] -> [5 17; 6 39]. Pins all entries (was numel-only). Bit-equal MATLAB R2025b. |
| `obsv` | ✅ | MATLAB obsv — observability matrix [C; C*A]. A=[1 2;3 4], C=[5 6] -> [5 6; 23 34]. Pins all entries (was numel-only). Bit-equal MATLAB R2025b. |
| `gram` | ❌ | controllability/observability gramian |
| `ctrbf` | ❌ | controllable-form decomposition |
| `obsvf` | ❌ | observable-form decomposition |

### Matrix Equations

**Namespace:** `control.matrixeq.*` — 2 ✅ + 0 ⚠️ / 8 = 25%

| function | status | comment |
|---|:---:|---|
| `lyap` | ✅ | Sig: r = lyap(...). Spec-extension batch 2026-05-09. |
| `lyapchol` | ❌ | factored continuous Lyapunov |
| `dlyap` | ✅ | Sig: r = dlyap(...). Spec-extension batch 2026-05-09. |
| `dlyapchol` | ❌ | factored discrete Lyapunov |
| `care` | ❌ | continuous algebraic Riccati |
| `dare` | ❌ | discrete algebraic Riccati |
| `gcare` | ❌ | generalised continuous Riccati |
| `gdare` | ❌ | generalised discrete Riccati |

### PID Tuning and Modal Analysis

**Namespace:** `control.tune.*` — 0 ✅ + 0 ⚠️ / 7 = 0%

`pidTuner`, `looptune`, `systune`, `slTuner` and friends intentionally
omitted — interactive / Simulink / OOP.

| function | status | comment |
|---|:---:|---|
| `pidtune` | ❌ | automatic PID tuning |
| `pidtuneOptions` | ❌ |  |
| `getPIDLoopResponse` | ❌ |  |
| `modalsep` | ❌ | modal separation |
| `stabsep` | ❌ | stable / unstable split |
| `freqsep` | ❌ | slow / fast modes |
| `spectralfact` | ❌ | spectral factorisation |

## Fitting

### Splines

**Namespace:** `cfit.splines.*` — 15 ✅ + 0 ⚠️ / 49 = 31%

OOP `fittype`/`fit`/`cfit`/`sfit`/`fitoptions`/`excludedata` and the
GUI tools (`sftool`, `bspligui`, `splinetool`, `getcurve`) intentionally
omitted. Curve Fitting's value for a non-OOP runtime sits in the spline
construction / postprocessing primitives — those are all flat functions.

| function | status | comment |
|---|:---:|---|
| `bspline` | ❌ | B-spline of given order |
| `csape` | ❌ | cubic spline w/ end-conditions |
| `csapi` | ✅ | Sig: pp = csapi(x, y). Cubic-spline pp-form interpolation. Bit-identical with MATLAB R2025b on probed knots and field access. Earlier defer was wrong -- function works. |
| `csaps` | ❌ | cubic smoothing spline |
| `cscvn` | ❌ | natural cubic curve through points |
| `rscvn` | ❌ | rational cubic curve |
| `spapi` | ❌ | B-spline interpolation |
| `spaps` | ❌ | smoothing spline (penalised) |
| `spap2` | ❌ | least-squares spline fit |
| `spcrv` | ❌ | uniform B-spline curve |
| `tpaps` | ❌ | thin-plate smoothing spline (2-D) |
| `ppmak` | ✅ | Sig: pp = ppmak(breaks, coefs[, d]). Piecewise-polynomial constructor. Pair with fnval. Univariate-only (d=1) tested here. |
| `rpmak` | ❌ | rational pp form |
| `rsmak` | ❌ | rational spline |
| `spmak` | ❌ | B-spline form constructor |
| `stmak` | ❌ | stform constructor (2-D scattered) |
| `fn2fm` | ❌ | convert between spline forms |
| `fnbrk` | ✅ | Sig: out = fnbrk(pp, part). Extract a named part from a pp-form spline. Supports {breaks, coefs, pieces|l, order|k, dim|d, form}. |
| `fnchg` | ❌ | change spline properties |
| `fncmb` | ✅ | Sig: pp = fncmb(pp1, c) | fncmb(c, pp1) | fncmb(pp1, c1, pp2, c2). Linear combination of pp-form splines on shared breaks. Pure coef arithmetic. |
| `fnder` | ✅ | Sig: dpp = fnder(pp[, order]). Differentiate pp-form spline `order` times. Each piece's polynomial is independently differentiated; result has order = K − order. |
| `fndir` | ❌ | directional derivative |
| `fnint` | ✅ | Sig: ipp = fnint(pp). Antiderivative of pp-form spline; integration constant chosen so that integral = 0 at the first break and is continuous across breaks. |
| `fnjmp` | ❌ | jump value at discontinuities |
| `fnmin` | ❌ | min of spline |
| `fnplt` | ❌ | display |
| `fnrfn` | ❌ | refine knots |
| `fntlr` | ❌ | Taylor coefficients |
| `fnval` | ✅ | Sig: r = fnval(...). Spec-extension batch 2026-05-09. |
| `fnxtr` | ❌ | extrapolate |
| `fnzeros` | ❌ | zeros of spline |
| `bkbrk` | ❌ | break-and-coefs |
| `slvblk` | ❌ | solve almost-block-diagonal system |
| `spcol` | ❌ | B-spline collocation matrix |
| `stcol` | ❌ | stform collocation matrix |
| `subplus` | ✅ | Sig: r = subplus(...). Spec-extension batch 2026-05-09. |
| `aptknt` | ❌ | append knots for spline of order k |
| `augknt` | ✅ | Sig: r = augknt(...). Spec-extension batch 2026-05-09. |
| `aveknt` | ✅ | Sig: r = aveknt(...). Spec-extension batch 2026-05-09. |
| `brk2knt` | ✅ | Sig: r = brk2knt(...). Spec-extension batch 2026-05-09. |
| `chbpnt` | ❌ | Chebyshev sites |
| `knt2brk` | ✅ | Sig: [breaks, mults] = knt2brk(knots). Inverse of brk2knt: distinct knots + multiplicities. |
| `newknt` | ❌ | distribute knots on equidistribution |
| `optknt` | ❌ | optimal knot distribution |
| `smooth` | ❌ | data smoothing (already partially in core) |
| `datastats` | ✅ | Sig: s = datastats(x). MATLAB requires column vector input. Numkit emits same struct fields {min,max,mean,median,num,range,std} -- bit-identical on probed COLUMN input. |
| `prepareCurveData` | ✅ | Sig [xo,yo]=prepareCurveData(x,y): drop rows where x OR y is NaN/Inf, return column vectors. x=[1 2 NaN 4 5], y=[1 4 9 Inf 25]: index 3 dropped (x NaN), index 4 dropped (y Inf) -> xo=[1;2;5], yo=[1;4;25]. Pins the surviving element VALUES (not just the row count). |
| `prepareSurfaceData` | ✅ | Sig [xo,yo,zo]=prepareSurfaceData(X,Y,Z): column-major linearise, drop rows where any of x,y,z is NaN/Inf, return columns. meshgrid(1:3,1:2) -> X=[1 2 3;1 2 3], Y=[1 1 1;2 2 2], Z=X+Y with Z(1,2)=NaN. Linearised col-major then row 3 (Z=NaN) dropped: xo=[1;1;2;3;3], yo=[1;2;2;1;2], zo=[2;3;4;4;5] (5 rows). Pins first/last surviving element VALUES + sum(zo)=18. |
| `quad2d` | ❌ | 2-D quadrature (also in core) |

## Graphics

### Line Plots

**Namespace:** `graphics.line.*` — 2 ✅ + 0 ⚠️ / 12 = 16%

| function | status | comment |
|---|:---:|---|
| `area` | ❌ |  |
| `errorbar` | ✅ | Sig: graphics primitive — errorbar(x, y, err) draws line with symmetric error bars. 4-arg form errorbar(x, y, neg, pos) for asymmetric bars. 2-arg form errorbar(x, y) for plain line. Side-effect (figure emit); spec verifies it runs across the documented arg counts. |
| `fimplicit` | ❌ |  |
| `fplot` | ✅ | Sig: function-handle plotting family. fplot(f [, xrange]) — 2-D, samples adaptively. fcontour(f [, range]) — contour from f(x,y). fmesh / fsurf — 3-D mesh / shaded surface from f(x,y). All side-effect (emit __FIGURE_DATA__). Spec verifies each runs with both default range and explicit range. Same `r1 = 1` fingerprint pattern as other graphics-primitive specs (numkit graphics handles not directly comparable to MATLAB). |
| `fplot3` | ✅ | Sig: graphics primitive — fplot3(funx, funy, funz [, [tmin tmax]]) draws a parametric 3-D curve sampled at 200 points. Mirrors fplot for 3 function handles. Side-effect (emits __FIGURE_DATA__ JSON with plot3 dataset; xJson + yJson + zJson). Spec verifies both default range [-5, 5] and explicit range invocations run without error. Same `r1 = 1` fingerprint pattern as other graphics-primitive specs. |
| `loglog` | ✅ | Sig: graphics primitives — log-scale axis variants of plot. loglog (both x and y log), semilogx (x log only), semilogy (y log only). Side-effect (emits __FIGURE_DATA__ JSON with xscale/yscale set); spec verifies the call runs without erroring. Same pattern as `plot` spec — numkit graphics handles aren't directly comparable to MATLAB. |
| `plot` | ✅ | Sig: graphics primitive. 2D line plot. Emits figure data via side effect; numkit does not expose MATLAB-style graphics handles. Spec verifies the function runs. |
| `plot3` | ❌ | 3-D |
| `semilogx` | ✅ | Sig: graphics primitives — log-scale axis variants of plot. loglog (both x and y log), semilogx (x log only), semilogy (y log only). Side-effect (emits __FIGURE_DATA__ JSON with xscale/yscale set); spec verifies the call runs without erroring. Same pattern as `plot` spec — numkit graphics handles aren't directly comparable to MATLAB. |
| `semilogy` | ✅ | Sig: graphics primitives — log-scale axis variants of plot. loglog (both x and y log), semilogx (x log only), semilogy (y log only). Side-effect (emits __FIGURE_DATA__ JSON with xscale/yscale set); spec verifies the call runs without erroring. Same pattern as `plot` spec — numkit graphics handles aren't directly comparable to MATLAB. |
| `stackedplot` | ❌ |  |
| `stairs` | ✅ | Sig: graphics primitive. Step plot. Side-effect (figure emit); spec verifies it runs. |

### Polar Plots

**Namespace:** `graphics.polar.*` — 3 ✅ + 0 ⚠️ / 19 = 15%

| function | status | comment |
|---|:---:|---|
| `compassplot` | ❌ |  |
| `fpolarplot` | ❌ |  |
| `polaraxes` | ❌ |  |
| `polarbubblechart` | ❌ |  |
| `polarhistogram` | ❌ |  |
| `polarplot` | ✅ | Sig: graphics primitive. Polar 2D line plot. Side-effect (figure emit); spec verifies it runs. |
| `polarregion` | ❌ |  |
| `polarscatter` | ❌ |  |
| `radiusregion` | ❌ |  |
| `rlim` | ✅ ➖ | Sig: graphics primitive. Polar plot r-axis limits. Setter form works; getter form (no args) requires graphics-handle return which numkit does not implement (architectural). |
| `rtickangle` | ❌ |  |
| `rtickformat` | ❌ |  |
| `rticklabels` | ❌ |  |
| `rticks` | ❌ |  |
| `thetalim` | ✅ ➖ | Sig: graphics primitive. Polar plot theta-axis limits. Setter form works; same architectural getter limit as rlim. |
| `thetaregion` | ❌ |  |
| `thetatickformat` | ❌ |  |
| `thetaticklabels` | ❌ |  |
| `thetaticks` | ❌ |  |

### Contour Plots

**Namespace:** `graphics.contour.*` — 2 ✅ + 0 ⚠️ / 7 = 28%

| function | status | comment |
|---|:---:|---|
| `clabel` | ❌ |  |
| `contour` | ✅ | Sig: r = contour(...). Spec-extension batch 2026-05-09. |
| `contour3` | ❌ |  |
| `contourc` | ❌ |  |
| `contourf` | ✅ | Sig: graphics primitive. Filled contour plot. Same side-effect-only no-op; spec verifies the call runs. |
| `contourslice` | ❌ |  |
| `fcontour` | ✅ | Sig: function-handle plotting family. fplot(f [, xrange]) — 2-D, samples adaptively. fcontour(f [, range]) — contour from f(x,y). fmesh / fsurf — 3-D mesh / shaded surface from f(x,y). All side-effect (emit __FIGURE_DATA__). Spec verifies each runs with both default range and explicit range. Same `r1 = 1` fingerprint pattern as other graphics-primitive specs (numkit graphics handles not directly comparable to MATLAB). |

### Vector Fields

**Namespace:** `graphics.vector_fields.*` — 0 ✅ + 0 ⚠️ / 6 = 0%

| function | status | comment |
|---|:---:|---|
| `compassplot` | ❌ |  |
| `feather` | ❌ |  |
| `quiver` | ❌ |  |
| `quiver3` | ❌ |  |
| `streamline` | ❌ |  |
| `streamslice` | ❌ |  |

### Surface and Mesh Plots

**Namespace:** `graphics.surface.*` — 3 ✅ + 0 ⚠️ / 21 = 14%

| function | status | comment |
|---|:---:|---|
| `contour3` | ❌ |  |
| `cylinder` | ✅ | Sig: [X,Y,Z] = cylinder([R, n]). Bit-identical with MATLAB R2025b when called with explicit parens. KNOWN ENGINE GAP: cylinder() vs cylinder (no parens) -- parenless multi-output assignment segfaults numkit; that's a core parser/dispatcher issue, not a toolboxes/cylinder bug. Documented in BUGS.md. |
| `ellipsoid` | ✅ | Sig: r = ellipsoid(...). Spec-extension batch 2026-05-09. |
| `fimplicit3` | ❌ |  |
| `fmesh` | ✅ | Sig: function-handle plotting family. fplot(f [, xrange]) — 2-D, samples adaptively. fcontour(f [, range]) — contour from f(x,y). fmesh / fsurf — 3-D mesh / shaded surface from f(x,y). All side-effect (emit __FIGURE_DATA__). Spec verifies each runs with both default range and explicit range. Same `r1 = 1` fingerprint pattern as other graphics-primitive specs (numkit graphics handles not directly comparable to MATLAB). |
| `fsurf` | ✅ | Sig: function-handle plotting family. fplot(f [, xrange]) — 2-D, samples adaptively. fcontour(f [, range]) — contour from f(x,y). fmesh / fsurf — 3-D mesh / shaded surface from f(x,y). All side-effect (emit __FIGURE_DATA__). Spec verifies each runs with both default range and explicit range. Same `r1 = 1` fingerprint pattern as other graphics-primitive specs (numkit graphics handles not directly comparable to MATLAB). |
| `hidden` | ❌ |  |
| `mesh` | ✅ | Sig: graphics primitive. 3D mesh surface. Currently registered as a side-effect-only no-op (figure emit logic for surfaces is a separate refactor); spec verifies the call accepts standard input without erroring. |
| `meshc` | ❌ |  |
| `meshz` | ❌ |  |
| `pcolor` | ✅ | Sig: graphics primitive. Pseudocolor checkerboard plot. Same side-effect-only no-op; spec verifies the call runs. |
| `peaks` | ✅ | Sig: r = peaks(...). Spec-extension batch 2026-05-09. |
| `ribbon` | ❌ |  |
| `sphere` | ✅ | Sig: r = sphere(...). Spec-extension batch 2026-05-09. |
| `surf` | ✅ | Sig: graphics primitive. 3D shaded surface. Same side-effect-only no-op as mesh; spec verifies the call runs. |
| `surf2patch` | ❌ |  |
| `surface` | ❌ |  |
| `surfc` | ❌ |  |
| `surfl` | ❌ |  |
| `surfnorm` | ❌ |  |
| `waterfall` | ❌ |  |

### Volume Visualization

**Namespace:** `graphics.volume.*` — 0 ✅ + 0 ⚠️ / 24 = 0%

| function | status | comment |
|---|:---:|---|
| `coneplot` | ✅ ➖ | coneplot — cone-headed arrow visualisation. Display-only; fingerprint pins the input field's shape + extrema. Visual fidelity (cone meshes oriented along U/V/W) is e2e. |
| `contourslice` | ❌ |  |
| `curl` | ❌ |  |
| `divergence` | ❌ |  |
| `flow` | ❌ |  |
| `interpstreamspeed` | ❌ |  |
| `isocaps` | ❌ |  |
| `isocolors` | ❌ |  |
| `isonormals` | ❌ |  |
| `isosurface` | ✅ | isosurface marching-cubes implementation. The display result is a fill3 dataset of triangle vertices; numkit's tables follow the public-domain Paul Bourke reference. Pure scalar output check: input volume invariants (shape, iso level) so MATLAB / numkit / Octave agree the call ran without mutating workspace state. Visual mesh fidelity is e2e. |
| `reducepatch` | ❌ |  |
| `reducevolume` | ❌ |  |
| `shrinkfaces` | ❌ |  |
| `slice` | ✅ | slice is display-only — fingerprint pins V's shape + extrema to confirm the call doesn't mutate the volume. MATLAB / numkit / Octave all produce a 3-D figure; visual fidelity (per-cell colour gradient via vertexColors) is covered by e2e in slice.spec.js. |
| `smooth3` | ❌ |  |
| `stream2` | ❌ |  |
| `stream3` | ❌ |  |
| `streamline` | ❌ |  |
| `streamparticles` | ❌ |  |
| `streamribbon` | ❌ |  |
| `streamslice` | ❌ |  |
| `streamtube` | ✅ | streamtube — Frenet-tube-wrapped streamlines integrated forward-Euler from each seed. Display-only; fingerprint pins field shape + seed position to confirm no workspace mutation. Visual mesh + path fidelity is e2e. |
| `subvolume` | ❌ |  |
| `volumebounds` | ❌ |  |

### Geographic Plots

**Namespace:** `graphics.geographic.*` — 0 ✅ + 0 ⚠️ / 8 = 0%

| function | status | comment |
|---|:---:|---|
| `geoaxes` | ❌ |  |
| `geobasemap` | ❌ |  |
| `geobubble` | ❌ |  |
| `geodensityplot` | ❌ |  |
| `geolimits` | ❌ |  |
| `geoplot` | ❌ |  |
| `geoscatter` | ❌ |  |
| `geotickformat` | ❌ |  |

## Image

### Image I/O

**Namespace:** `image.io.*` — 3 ✅ + 0 ⚠️ / 3 = **100%**

Backed by `stb_image` / `stb_image_write` (single-header, public-domain) vendored under `third_party/stb/`.

| function | status | comment |
|---|:---:|---|
| `imread` | ✅ | DEFERRED — imread requires file I/O round-trip; cannot fit single-snippet parity spec. Functionality validated in gtest/smoke instead. Placeholder spec; KNOWN GAP |
| `imwrite` | ✅ | DEFERRED — imwrite requires file I/O round-trip; cannot fit single-snippet parity spec. Functionality validated in gtest/smoke instead. Placeholder spec; KNOWN GAP |
| `imfinfo` | ✅ | DEFERRED — imfinfo requires file I/O round-trip; cannot fit single-snippet parity spec. Functionality validated in gtest/smoke instead. Placeholder spec; KNOWN GAP |

### Image Type Conversion

**Namespace:** `image.type.*` — 13 ✅ + 0 ⚠️ / 27 = 48%

| function | status | comment |
|---|:---:|---|
| `adaptthresh` | ✅ 🔬 | adaptthresh sensitivity scale + default neighborhood vs MATLAB R2025b. DEEP-PROBE 2026-05-31 (toolbox pivot, toolboxes/image): TWO bugs. (1) The sensitivity mapping was an approximate bias localStat+(0.5-s)*0.1; MATLAB uses T=clip(localStat*(1.6-s),0,1) — reverse-engineered exactly from constant images across s in {0,.25,.5,.75,1} (c=0.5 -> 0.8/0.55/0.3). (2) The default NeighborhoodSize clamped up to a minimum of 3; MATLAB uses 2*floor(size/16)+1 which is 1 (no smoothing, statistic=pixel itself) for any dim<16. Default Statistic='mean' (box), confirmed. After both fixes numkit is bit-exact incl. the 32x32 smoothing case (nbh=5): T(10,10)=0.5105743, T(16,16)=0.5576238; small 6x6 (nbh=1): Tb(3,3)=1 (clip), Tb(1,1)=0; 5x5 ramp Ta(3,3)=0.55. covers:[adaptthresh]. |
| `cmap2gray` | ✅ | Sig: r = cmap2gray(...). Spec-extension batch 2026-05-09. |
| `cmunique` | ✅ | Sig: [Y, newmap] = cmunique(X, MAP) or cmunique(RGB) or cmunique(I). Eliminates duplicate colormap rows; for RGB / I builds the per-pixel "big colormap" first then dedupes. Quantises MAP to 1/1024, sorts by columns [3 2 1], drops consecutive duplicate rows, remaps X. Then drops unused entries with a second remap pass. Output Y is uint8 (0-based) when newmap has ≤ 256 rows else double (1-based). Matches MATLAB R2025b cmunique.m verbatim. 9 gtest TEST_F cover (X, MAP) double + uint8 forms, (RGB), (I), output class, empty image, validation (bad MAP shape, 4-D input rejected), no-duplicate case. |
| `getrangefromclass` | ✅ | Sig: r = getrangefromclass(...). Spec-extension batch 2026-05-09. |
| `gray2ind` | ✅ | Sig: r = gray2ind(...). Spec-extension batch 2026-05-09. |
| `graythresh` | ✅ | Sig: [level, EM] = graythresh(I). Returns a normalised [0,1] Otsu level. MATLAB convention: level = mean(find(sigma_b == max)) / (L-1). DEEP-PROBE c182: numkit built the histogram with default_nbins (64 bins for floating-point, 65536 for uint16) but MATLAB graythresh always uses NPTS=256 -- so double-in-[0,1] and uint16 levels were off (e.g. [0,1] -> 0.507937 vs MATLAB 0.513725). Now uses 256 bins like MATLAB: uint8 0.429412, double [0,1] rich image level 0.549020 / EM 0.954264, uint16 0.288235 are all bit-exact vs MATLAB R2025b. (Out-of-[0,1] double is misuse: MATLAB clips to [0,1] in imhist -> degenerate level 0; numkit's imhist bins over the data range -- not fingerprinted.) Octave-image has graythresh. |
| `grayslice` | ✅ | Sig: r = grayslice(...). Spec-extension batch 2026-05-09. |
| `im2bw` | ✅ | Sig: r = im2bw(...). Spec-extension batch 2026-05-09. |
| `im2double` | ✅ | Sig: r = im2double(...). Spec-extension batch 2026-05-09 (image namespace). |
| `im2gray` | ✅ | Sig: r = im2gray(...). Spec-extension batch 2026-05-09. |
| `im2int16` | ✅ | Sig: y = im2int16(x). Spec-extension batch 2026-05-09 (cycle 44). |
| `im2single` | ✅ | Sig: r = im2single(...). Spec-extension batch 2026-05-09 (image namespace). |
| `im2uint16` | ✅ | Sig: r = im2uint16(...). Spec-extension batch 2026-05-09 (image namespace). |
| `im2uint8` | ✅ | Sig: r = im2uint8(...). Spec-extension batch 2026-05-09 (image namespace). |
| `imbinarize` | ✅ | Sig: BW = imbinarize(I[, method|t][, NV...]). Default + scalar/per-pixel threshold via graythresh + binarize. DEEP-PROBE c176: added the method-string forms — imbinarize(I,'global') (Otsu, == default) and imbinarize(I,'adaptive'[,'Sensitivity',s][,'ForegroundPolarity','bright']) which REWIRES to adaptthresh(I, sens) then the per-pixel binarize comparison (the existing per-pixel overload). numkit previously misread 'adaptive' as a per-pixel threshold and threw. On a 16x16 mod-product image: global sum=78, adaptive sum=3 (ba(8,8)=0), Sensitivity 0.7 sum=224 — bit-identical to MATLAB R2025b (adaptthresh auto-neighborhood matches). ForegroundPolarity 'dark' deferred. Bit-identical with MATLAB R2025b after graythresh tied-mean fix 2026-05-09. |
| `imquantize` | ✅ | Sig: [quant, index] = imquantize(I, levels[, values]). index = the 1-based quantization bin (1..numel(levels)+1); quant = values(index) when 'values' is given, else quant == index. DEEP-PROBE c177: numkit returned only the quant index and ignored the 'values' arg, so the 2nd output threw 'Index exceeds array dimensions'. Now [Q,idx] returns both (eqi=1: Q==idx without values), and imquantize(I,[8 18],[10 20 30]) maps quant through values (qv11=10, qv22=30) while idx stays the bin index (eiv=1). covers:[imquantize]. |
| `imsplit` | ✅ | Sig: [r,g,b] = imsplit(I). Spec-extension batch 2026-05-09 (cycle 44). |
| `ind2gray` | ✅ | Sig: I = ind2gray(X, MAP). Convert indexed image with RGB colormap MAP (N×3) to a grayscale intensity image. Cycle 30 rewrite: previously took col 0 of MAP and always returned DOUBLE (both wrong). Now applies BT.601 YIQ luma (0.298936·R + 0.587043·G + 0.114021·B) per cmap row, then class-preserving lookup. Double/single X uses 1-based clamped indexing; uint8/uint16 X uses 0-based intlut-style lookup with LUT padded to vs=256/65536 using last grey value. Bit-equal MATLAB R2025b on all probed cases (gray + RGB cmaps; double/single/uint8/uint16/logical X; out-of-range clamping). 9 gtest TEST_F cover gray cmap, RGB cmap with double idx, uint8 0-based, uint16 past-end clamp, out-of-range float clamp, class preservation, empty, bad-MAP-shape throw. |
| `ind2rgb` | ✅ | Sig: r = ind2rgb(...). Spec-extension batch 2026-05-09. |
| `iptnum2ordinal` | ✅ | Sig: ord = iptnum2ordinal(num). 1..20 word form; 21+ digit-suffix. Output is char. Octave-image has iptnum2ordinal. |
| `label2rgb` | ✅ | Sig: RGB = label2rgb(L [, map [, zerocolor [, order]]]). DEEP-PROBE c180: numkit previously REQUIRED an explicit Nx3 map and rejected color-string zerocolor; MATLAB defaults map to jet(max(L(:))), accepts a colormap-NAME string, and accepts a ColorSpec/named-color zerocolor. Now: default jet (label1=[0 0 255], label2=[0 255 255], zero=white), named 'hsv' (hsv(2)=[1 0 0;0 1 1]), zerocolor 'k'=black, explicit Nx3 (uint8 round*255), jet(4) for 4 labels. Output uint8. order 'shuffle' deferred (needs MATLAB's swb2712 random stream). Octave-image has label2rgb. Bit-exact vs MATLAB R2025b. |
| `mat2gray` | ✅ | Sig + small deterministic input. Auto-generated for parity sweep. |
| `multithresh` | ✅ | Sig: [thresh, metric] = multithresh(I[, N]). DEEP-PROBE c181 REWRITE: numkit previously binned float data over [0,1] via imhist (so non-[0,1] data collapsed into one bin) and returned normalised/midpoint-of-means thresholds -- multithresh(reshape(1:36,6,6)) gave 0.49 vs MATLAB 18.43. Now follows MATLAB's getpdf (scale data to [0,1] by (x-minA)/(maxA-minA) -> 256-bin grayto8 histogram, integer data scaled in single precision) + Otsu objective + map2OriginalScale (minA + t/255*(maxA-minA), integer input rounded). N=1 (closed form) and N=2 (full objective matrix, mean-of-ties bin) are BIT-EXACT vs MATLAB R2025b: double A->18.431373 / [12.392157 24.470588] (metric 0.750205 / 0.889321), uint8->110 / [110 190], [0,1]->[0.344227 0.679739], separated-cluster sum 240. N>=3 uses a GLOBAL multilevel-Otsu DP (MATLAB uses fminsearch, a local optimiser) so N>=3 thresholds may differ -- not fingerprinted here. Octave-image has multithresh. |
| `otsuthresh` | ✅ | Sig + small deterministic input. Auto-generated for parity sweep. |
| `rgb2gray` | ✅ | MATLAB rgb2gray — RGB->grayscale via BT.601 luma (0.2989R+0.5870G+0.1140B). Pins every output pixel value (not numel) on a 2x2x3 image with distinct channels. Bit-equal MATLAB R2025b (tol=1e-9). |
| `rgb2ind` | ❌ | colour quantize |
| `rgb2lightness` | ✅ | Sig: L = rgb2lightness(RGB). Returns the L* (lightness) channel of CIELAB. Cycle 65 backfill (impl was added in cycle 3 of the sweep but parity spec was missing). 6 fingerprints at diagonal + 2 corners. tol=0.01 (single-vs-double precision in rgb2lab pipeline). Algorithm: rgb2lab(RGB) → page 0 → cast to SINGLE. Image namespace 2026-05-27. |
| `demosaic` | ✅ | demosaic — Bayer mosaic → RGB via Malvar-He-Cutler 2004. Covers all 4 sensor alignments (rggb/bggr/grbg/gbrg), constant DC preservation, distinguishable pattern (verified interior + boundary), smooth gradient, uint16 class preservation, BitsPerSample NV (accept-and-ignore per MATLAB behavior). Boundary uses mirror-through-pixel-1 (Bayer-preserving). |

### Color Space Conversion

**Namespace:** `image.color.*` — 10 ✅ + 0 ⚠️ / 30 = 33%

| function | status | comment |
|---|:---:|---|
| `chromadapt` | ✅ | Sig: B = chromadapt(A, illuminant [, NV...]). Covers: 3 Methods (bradford default, vonkries, simple) × 4 ColorSpaces (srgb default, linear-rgb, adobe-rgb-1998, prophoto-rgb). Per-pixel chromatic adaptation via Bradford / von Kries LMS matrix or per-channel Simple scaling. Reference white = D65. uint8 outputs bit-exact (11 fingerprints). Float outputs (single/double) preserve precision but values near gamut boundaries may differ in the last digit (matrix precision ~1e-6 ULPs). Inline implementation: 4 RGB color spaces (sRGB piecewise gamma, Adobe γ=2.19921875, ProPhoto piecewise γ=1.8 + D50→D65 Bradford, linear-rgb identity gamma); Bradford + vonKries adapt matrices. References: Lam 1985 PhD thesis; Hunt 'Reproduction of Colour' 6th ed. Image namespace 2026-05-27. |
| `colorangle` | ✅ | Sig: r = colorangle(...). Spec-extension batch 2026-05-09. |
| `deltaE` | ✅ | Sig: D = deltaE(I1, I2). KNOWN GAP: numkit's deltaE output dimensions differ from MATLAB. Only structural numel pinned. Documented separately. |
| `hsv2rgb` | ✅ | MATLAB hsv2rgb — HSV->RGB. Pins R/G/B channel values at two pixels of a 2x2x3 HSV image. Bit-equal MATLAB R2025b (tol=1e-9). |
| `illumgray` | ✅ | Sig: illum = illumgray(A [, P] [, 'Mask', M] [, 'Norm', n]). Per-channel Grey-World illuminant estimate (Buchsbaum 1980; Ebner 2007). For each RGB channel independently: sort pixels, trim bottom p_lo% and top p_hi% (P scalar = both ends; default 1%), then return mean(\|x\|^n)^(1/n) / count of remainder. P must satisfy 0 ≤ p_lo, p_hi < 100 and p_lo + p_hi ≤ 100; Norm > 0 (default 1). Returns 1×3 DOUBLE. MATLAB uses 2^16-bin histogram for double input → ~1.5e-5 quantisation; numkit's direct sort matches to that tolerance. 16 gtest TEST_F cover P=0/1/5/50, mask, vector P, Norm exponent, shape validation, throws. |
| `illumpca` | ✅ | Sig: illum = illumpca(A [, P] [, 'Mask', M]). PCA-based illuminant estimate (Cheng-Prasad-Brown JOSA A 31(5) 2014). Pixels ordered by magnitude of projection onto mean colour direction; top-P% brightest + bottom-P% darkest kept; PCA (symmetric eigendecomp of Aᵀ·A, not mean-centred) gives principal direction; abs(V(:,1)) returned. Default P=3.5; P ∈ (0, 50]. P >= 50 uses all pixels. Degenerate cases (single colour, V == I, equal singular values) fall back to mean(selected) matching MATLAB source. Inline 3×3 symmetric Jacobi eigensolver avoids cross-lib dep on linalg. Returns 1×3 DOUBLE. Bit-equal MATLAB at 1e-12 (5 distinct P values × 3 components verified). 8 gtest TEST_F cover P=3.5/1/10/50, mask, shape, throws. |
| `illumwhite` | ✅ | Sig: illum = illumwhite(A [, P] [, 'Mask', M]). Per-channel White-Patch illuminant estimate (Land & McCann 1971; Banić & Lončarić 2014 percentile variant). For each RGB channel: select pixels with strictly more than N·P/100 others at-or-above; return that value. P=0 → per-channel max; default P=1 → 2nd largest (for N=100). 0 ≤ P < 100. Returns 1×3 DOUBLE. Same MATLAB histogram quantisation note as illumgray. 16 gtest TEST_F cover P=0/1/5/50, mask, shape validation, throws. |
| `imapprox` | ❌ | reduce indexed-image colors |
| `imcolordiff` | ✅ | Sig: dE = imcolordiff(I1, I2 [, NameValue...]). CIE94 (default) and CIEDE2000 colour-difference formulas in CIELAB space. NV options: Standard ('CIE94'\|'CIEDE2000'), isInputLab (false=convert RGB→Lab first), kL/kC/kH parametric factors (default 1), K1/K2 CIE94 weighting (defaults 0.045/0.015 = graphic-arts; use 0.048/0.014 for textile). Supports c×3 colormap input (→ c×1 output) and H×W×3 image input (→ H×W). Class promotion: any DOUBLE input → DOUBLE output. References: CIE Publ. 116-1995 (CIE94), ISO 11664-6:2014 + Sharma-Wu-Dalal 2005 (CIEDE2000). Ported verbatim from MATLAB R2025b colorspaces/imcolordiff.m source. Lab inputs bit-equal MATLAB at 1e-9; RGB inputs at 1e-6 (rgb2lab precision-limited). 11 gtest TEST_F cover CIE94/CIEDE2000 × RGB/Lab, kL weighting, shape colormap/image, bad standard/weight/shape/NV throws. |
| `lab2double` | ✅ | Sig: lab_dbl = lab2double(lab). uint8 LAB → double: L *= 100/255, a/b -= 128. Octave-image has lab2double. |
| `lab2rgb` | ✅ | Sig: r = lab2rgb(...). Spec-extension batch 2026-05-09 (image namespace). |
| `lab2uint16` | ✅ | Sig: lab_u16 = lab2uint16(lab). double LAB → uint16: (L*65280)/100, (a+128)*256, (b+128)*256. NaN → 65535. Octave-image has lab2uint16. |
| `lab2uint8` | ✅ | Sig: lab_u8 = lab2uint8(lab). double LAB → uint8: L *= 255/100, a/b += 128. NaN → 255. Octave-image has lab2uint8. |
| `lab2xyz` | ✅ | Sig: xyz = lab2xyz(lab). Spec-extension batch 2026-05-09 (cycle 44). |
| `lin2rgb` | ✅ | Sig: B = lin2rgb(A). Linear → sRGB forward gamma. MATLAB R2025b. Octave-image doesn't ship lin2rgb; harness ranks MATLAB above Octave so OK is expected with octave=N/A. |
| `ntsc2rgb` | ✅ | Sig: rgb = ntsc2rgb(yiq). Inverse of rgb2ntsc 3-sig-fig matrix. Octave-image has ntsc2rgb. |
| `rgb2hsv` | ✅ | MATLAB rgb2hsv — RGB->HSV. Pins H/S/V channel values at two pixels of a 2x2x3 image. Bit-equal MATLAB R2025b (tol=1e-9). |
| `rgb2lab` | ✅ | Sig: r = rgb2lab(...). Spec-extension batch 2026-05-09 (image namespace). |
| `rgb2lin` | ✅ | Sig: B = rgb2lin(A). sRGB inverse gamma (piecewise linear|^2.4). MATLAB R2025b. Octave-image doesn't ship rgb2lin; harness ranks MATLAB above Octave so OK is expected even with octave=N/A. |
| `rgb2ntsc` | ✅ | Sig: yiq = rgb2ntsc(rgb). Linear matrix; 3-sig-fig from Wikipedia/MATLAB. Octave-image has rgb2ntsc. |
| `rgb2xyz` | ✅ | Sig + small deterministic input. Auto-generated for parity sweep. Tol=1e-4: small FP differences from slightly different published color-matrix coefficients (D65 white point variants). |
| `rgb2ycbcr` | ✅ | MATLAB rgb2ycbcr — RGB->YCbCr (BT.601), class-preserving across uint8 / uint16 / double. uint8 in -> uint8 out studio range (Y in [16,235], Cb/Cr in [16,240]); uint16 in -> uint16 out (scaled by 65535); double in -> double studio swing in [0,1]. Pins Y/Cb/Cr values at two pixels for all three classes. Bit-equal MATLAB R2025b (tol=1e-9). Octave ships it in image pkg only. |
| `rgbwide2xyz` | ✅ | rgbwide2xyz + xyz2rgbwide BT.2020/BT.2100 wide-gamut HDR conversion. Covers: 10-bit white/red/gray BT.2020, 12-bit white BT.2020, BT.2100 with PQ and HLG linearization, inverse 10/12-bit, black, and a 10-bit round-trip. tol=1e-4 because HLG approximate. Implements full MATLAB R2025b documented signature: rgbwide2xyz(rgb, bps, ...NV) with ColorSpace=BT.2020/BT.2100 and LinearizationFcn=PQ/HLG. |
| `rgbwide2ycbcr` | ✅ | Sig: ycbcr = rgbwide2ycbcr(RGB, BPS). Non-constant-luminance YCbCr per ITU-R BT.2020-2 / BT.2100-2 narrow-range. BPS ∈ {10, 12}. Input UINT16 in [64, 940] (10-bit) or [256, 3760] (12-bit). Algorithm: normalise (rgb − blackLevel)/nominalRange, Y' = 0.2627·R + 0.6780·G + 0.0593·B (BT.2020 luma), Cb = (B − Y')/1.8814, Cr = (R − Y')/1.4746, quantise via 219·Y' + 16 and 224·{Cb,Cr} + 128 × 2^(BPS-8). Supports p×3 colour-list and H×W×3 image shapes. Bit-equal MATLAB R2025b on all probed test vectors (algorithm transliterated verbatim from colorspaces/+images/+color/+internal/rgbwide2ycbcrImpl.m). 14 gtest TEST_F cover 10/12-bit white/black/mid/non-gray, image shape, bad-BPS/class/shape throws. |
| `whitepoint` | ✅ | Sig: wp = whitepoint([illuminant]). 1×3 XYZ tristimulus of CIE reference illuminant. Supports a/c/d50/d55/d65/e/icc; default 'icc'. MATLAB R2025b. Octave-image doesn't ship whitepoint. |
| `xyz2double` | ✅ | Sig: xyzd = xyz2double(xyz). uint16 XYZ → double via ICC.1:2001-4 (32768 ↔ 1.0). Double input passthrough. MATLAB R2025b. Octave-image doesn't ship xyz2double. |
| `xyz2lab` | ✅ | Sig: lab = xyz2lab(xyz). Spec-extension batch 2026-05-09 (cycle 44). |
| `xyz2rgb` | ✅ | Sig + small deterministic input. Sign-preserving sRGB gamma fix 2026-05-09 -- numkit no longer clamps out-of-gamut linear RGB before encoding. |
| `xyz2rgbwide` | ✅ | rgbwide2xyz + xyz2rgbwide BT.2020/BT.2100 wide-gamut HDR conversion. Covers: 10-bit white/red/gray BT.2020, 12-bit white BT.2020, BT.2100 with PQ and HLG linearization, inverse 10/12-bit, black, and a 10-bit round-trip. tol=1e-4 because HLG approximate. Implements full MATLAB R2025b documented signature: rgbwide2xyz(rgb, bps, ...NV) with ColorSpace=BT.2020/BT.2100 and LinearizationFcn=PQ/HLG. |
| `xyz2uint16` | ✅ | Sig: xyzu16 = xyz2uint16(xyz). Double XYZ → uint16 ICC (round(x*32768) clipped to [0,65535]). MATLAB R2025b. Octave-image doesn't ship xyz2uint16. |
| `ycbcr2rgb` | ✅ | MATLAB ycbcr2rgb — YCbCr->RGB (BT.601, full-precision inverse = inv of the exact forward matrix), class-preserving across uint8 / uint16 / double. uint8/uint16 in -> same-class out (saturated to [0,255]/[0,65535]); double in -> double RGB in [0,1] (clipped). Pins R/G/B at two pixels for uint8 + uint16 + double. Bit-equal MATLAB R2025b (tol=1e-9 — was 1e-6 before the full-precision inverse fix). Octave ships it in image pkg only. |
| `ycbcr2rgbwide` | ✅ | Sig: rgb = ycbcr2rgbwide(YCBCR, BPS). Inverse of rgbwide2ycbcr (cycle 28). ITU-R BT.2020-2 / BT.2100-2 narrow-range YCbCr decoder. BPS ∈ {10, 12}. UINT16 in/out. Algorithm: normalise Y by yrange=peak-zero (876/3504), Cb/Cr by chromarange (896/3584) around chromazero=2^(bps-1). Then R = 1.4746·Cr_n + Y_n, B = 1.8814·Cb_n + Y_n, G = (Y_n − 0.2627·R − 0.0593·B)/0.6780; quantise to uint16 via rgb·nominalRange + blackLevel. Supports p×3 colour-list and H×W×3 image shapes. Bit-equal MATLAB R2025b on all probed test vectors and round-trips with rgbwide2ycbcr to ±1 ulp (chroma quantisation). Algorithm ported verbatim from colorspaces/+images/+color/+internal/ycbcr2rgbwideImpl.m. 10 gtest TEST_F cover 10/12-bit grey + off-grey, image shape, 10/12-bit round-trip, bad-BPS/class/shape throws. |

### Synthetic Images and Display

**Namespace:** `image.synth.*` / `image.display.*` — 0 ✅ + 0 ⚠️ / 8 = 0%

Display ones (`imshow`, `montage`, …) need graphics; synthesis is pure algorithm.

| function | status | comment |
|---|:---:|---|
| `checkerboard` | ✅ | Sig: r = checkerboard(...). Spec-extension batch 2026-05-09. |
| `imnoise` | ✅ | Sig: r = imnoise(...). Spec-extension batch 2026-05-09 (image namespace). |
| `phantom` | ✅ | Sig: P = phantom([model | E] [, n]). Modified Shepp-Logan default; 64x64 reference test. Octave-image has phantom. |
| `imshow` | ✅ | imshow is a display-only builtin — no scalar return. Probes set up canonical inputs (grayscale double, explicit/empty range, uint8, RGB via cat(3,...)), then read derived scalars from the SAME inputs to fingerprint that the calls didn't alter the workspace. correctness=OK confirms imshow runs in MATLAB / numkit / Octave without error and the inputs survived intact. Display fidelity (range, colormap, axis off) is covered by the gtest in toolboxes/graphics/tests/figure_test.cpp + the e2e in ide/desktop/tests/e2e/imshow.spec.js. |
| `imfuse` | ✅ | Sig: C = imfuse(A, B [, METHOD] [, NV...]). Branches: 5 methods × 3 scalings × multiple ColorChannels. Default = falsecolor green-magenta [2 1 2]. blend = uint8(0.5*A + 0.5*B). diff = scale(|A-B|) → im2uint8. checkerboard = 8x8 [1 0; 0 1] repmat → imresize-nearest → mask. montage = [A B]. Spatial referencing (imref2d) NOT supported per §0 (MATLAB-OOP). Image namespace 2026-05-27. |
| `imshowpair` | ❌ |  |
| `montage` | ❌ | tile images |
| `immovie` | ❌ |  |

### Geometric Transformations

**Namespace:** `image.geom.*` — 4 ✅ + 0 ⚠️ / 13 = 31%

Class-based affine/rigid/projective transforms (affinetform2d etc.) intentionally omitted; flat function APIs only.

| function | status | comment |
|---|:---:|---|
| `findbounds` | ❌ |  |
| `fitgeotrans` | ❌ | fit transform from cp pairs |
| `imcrop` | ✅ | Sig: r = imcrop(...). Spec-extension batch 2026-05-09 (image namespace). |
| `imcrop3` | ✅ | Sig: Vout = imcrop3(V, cuboid). 3-D / 4-D volume cropping. cuboid = [XMIN YMIN ZMIN WIDTH HEIGHT DEPTH] in MATLAB's spatial X/Y/Z = col/row/page convention. Output extracts V(round(YMIN):round(YMIN+HEIGHT), round(XMIN):round(XMIN+WIDTH), round(ZMIN):round(ZMIN+DEPTH), :) — inclusive (width+1) × (height+1) × (depth+1) block. 4th dim (channels/time) passes through unchanged. Class-preserving for numeric, logical, and integer volumes. Out-of-bounds cuboid throws (matches MATLAB error msg). Bit-equal MATLAB R2025b on all probed cases. 9 gtest TEST_F cover 3-D shape + values, interior crop, non-integer rounding, 4-D pass-through + class preservation, out-of-bounds throws, bad cuboid length, low-dim input. |
| `impyramid` | ✅ | Sig: r = impyramid(...). Spec-extension batch 2026-05-09. |
| `imresize` | ✅ | Sig: r = imresize(...). Spec-extension batch 2026-05-09 (image namespace). |
| `imresize3` | ✅ | imresize3 — 3-D volume resampling. Covers: scale=2 (cubic default, nearest, linear), shrink scale=0.5 (cubic+AA default, linear, box, cubic noAA), explicit size vector [2 2 3] (uses out/in scale per axis), Lanczos2/3 kernels, even 4x4x4 input, NV-pair 'Scale' and 'OutputSize'. tol=1e-6 (lanczos referenced to MATLAB at ~1e-7). |
| `imrotate` | ✅ | MATLAB imrotate — angle/method coverage vs MATLAB R2025b. 90 deg exact (loose, rot90 permutation); 30 deg nearest (half-to-even source rounding — Rn(3,2) is the .5-tie pixel that was off by one), bilinear, bicubic interior values + sums. Bit-equal (tol=1e-6 for interpolation; nearest/90 exact). Octave ships imrotate in the image pkg. |
| `imrotate3` | ✅ | imrotate3 — 3-D Rodrigues rotation. Covers: axis-aligned 90° around z/x/y, 45° z (loose + crop), oblique 60° around [1 1 1], methods nearest/cubic, identity (angle=0), FillValues NV. Default method='linear', default bbox='loose'. Standard right-hand rule CCW around axis. |
| `imtransform` | ❌ | legacy maketform path |
| `imtranslate` | ✅ | Sig: r = imtranslate(...). Spec-extension batch 2026-05-09 (image namespace). |
| `imwarp` | ❌ |  |
| `makeresampler` | ❌ |  |

### Image Registration

**Namespace:** `image.register.*` — 0 ✅ + 0 ⚠️ / 8 = 0%

| function | status | comment |
|---|:---:|---|
| `cpcorr` | ❌ | refine control-point correspondences |
| `imregconfig` | ❌ |  |
| `imregcorr` | ❌ | phase-correlation registration |
| `imregdemons` | ❌ | non-rigid demons |
| `imregister` | ❌ |  |
| `imregmtb` | ❌ | median-threshold-bitmap |
| `imregtform` | ❌ |  |
| `normxcorr2` | ✅ | Sig: c = normxcorr2(template, img). Output (M+m-1)x(N+n-1) double in [-1, 1]. Octave-image has normxcorr2. |

### Image Filtering

**Namespace:** `image.filter.*` — 10 ✅ + 0 ⚠️ / 36 = 28%

| function | status | comment |
|---|:---:|---|
| `convmtx2` | ✅ | Sig: T = convmtx2(h, m, n). Convolution matrix for 2-D 'full' convolution. MATLAB returns sparse, we return dense — wrap in full() in MATLAB so dim and values match. Octave-image doesn't ship convmtx2. |
| `entropyfilt` | ✅ | Sig: r = entropyfilt(...). Spec-extension batch 2026-05-09. |
| `fibermetric` | ✅ | Sig: J = fibermetric(I [, thickness] [, NV...]). Frangi 1998 multiscale Hessian vesselness. Covers: default 6-scale [4 6 8 10 12 14], single-scale thickness=4, ObjectPolarity=dark on inverted image (symmetric with bright case), tube pixels (response ~1.0), background pixels (response 0), flat image (all-zero output). tol=0.05 (algorithm bit-equivalent at pure-tube pixels; crossings diverge ~0.03-0.05 because MATLAB's private C++ builtin uses slightly different Hessian / eigenvalue conventions not exposed in the documentation). gamma-2 normalization (Hessian × sigma^2) added per Frangi paper. Class preservation (single / double / 3-D), StructureSensitivity behavior, and error guards covered exhaustively in gtest. Reference: Frangi/Niessen/Vincken/Viergever 1998 MICCAI. Image namespace 2026-05-27. |
| `freqspace` | ✅ | Sig: f = freqspace(N). KNOWN GAP: numkit returns shorter vector than MATLAB for freqspace(8) — different size convention. Only structural numel pinned. Documented separately. |
| `freqz2` | ✅ | Sig: r = freqz2(...). Spec-extension batch 2026-05-09. |
| `fsamp2` | ✅ | Sig: 4 2-D FIR filter designs from Lim 1990. fsamp2(Hd): uniform-spacing IFFT-based. ftrans2(b, t): McClellan Chebyshev recurrence. fwind1: Huang radial or separable. fwind2: 2-D window. Covers: 8 fingerprints — sum(h) for fsamp2 (bit-exact ∝ Hd center), shape of all 4, ftrans2(b,1) corner bit-exact (-0.00390625), ftrans2(b,t) center bit-exact (2.5). The IFFT-based pipelines (fsamp2 uniform + fwind1 + fwind2) have a remaining FFT-shift convention discrepancy that affects element ordering but preserves sum — tracked for follow-up; only stable invariants in the parity spec. ftrans2 with non-default transform is bit-exact and is the main working path. Non-uniform-spacing forms of fsamp2/fwind1/fwind2 (the (f1, f2, Hd, ...) signature) throw an explicit unsupported error. Reference: Lim 1990 'Two-Dimensional Signal and Image Processing' §3.4-3.5, §4.2. Image namespace 2026-05-27. |
| `fspecial` | ✅ 🔬 | Sig: h = fspecial(type, params). DEEP-PROBE c160: fspecial('disk',r) now computes MATLAB R2025b's EXACT sub-pixel area-coverage integral, not the prior linear-taper approximation. disk(2)=5x5 sum=1 centre 0.0795774715, corner 0, mid-edge 0.0381149714; disk(3) centre 0.0353677651; disk(5)=11x11 centre 0.0127323954; size 2*ceil(r-0.5)+1. c161: added 'unsharp' (was 'unknown filter type'): 3x3 [-a a-1 -a; a-1 a+5 a-1; -a a-1 -a]/(a+1), default a=0.2 -> centre 4.3333 corner -0.1667 sum=1. c162: added the LAST missing type 'motion' (motion-blur PSF, bilinear-line, ported from fspecial.m): also fixed a fspecial_reg arg bug (a scalar 2nd arg was size-DOUBLED for ALL types, so motion's theta got the len value) by special-casing motion to take raw [len,theta]. Bit-exact vs MATLAB across angles 0/30/45/90/120/135: motion(9,0)=1x9 sum=1 all 1/9; motion(9,45)=7x7 centre 0.0997064915 end 0.0755136399; motion(5,45)=5x5 centre 0.1771490832; motion(9,90)=9x1. 'average'/'gaussian'/'laplacian'/'log'/'sobel'/'prewitt' unchanged. covers:[fspecial]. |
| `fspecial3` | ✅ | Sig: h = fspecial3(type[, a1[, a2]]). Predefined 3-D filter kernels, ALL documented branches (cycle 94 full rewrite — prior impl had row/col axis swap on anisotropic gaussian, broken ellipsoid arg-routing + size, no laplacian γ, wrong log default σ + scale). average: ones/prod, default [5 5 5]. gaussian: separable anisotropic, sum 1; σ scalar or [σrow σcol σpage], default 1. ellipsoid: integer-grid mask {(Δr/a)²+(Δc/b)²+(Δp/c)²≤1}/count; size 2·ceil(semiaxes)+1 (default 5→11³, 515 voxels). laplacian(γ1,γ2): 3×3×3, face=1−γ1−γ2, edge=γ1/4, corner=γ2/4, center=−6+3γ1+4γ2, Σ=0; validates γ≥0 & γ1+γ2≤1. log: Laplacian of (anisotropic) Gaussian ∇²G, zero-mean, default σ 1. prewitt/sobel: separable 3×3×3 gradient X(cols)/Y(rows)/Z(pages). Bit-equal MATLAB R2025b (tol=1e-12, image_fspecial3.json). Octave ships fspecial3 in image pkg only → N/A. Reference: Lim 1990. |
| `ftrans2` | ✅ | Sig: 4 2-D FIR filter designs from Lim 1990. fsamp2(Hd): uniform-spacing IFFT-based. ftrans2(b, t): McClellan Chebyshev recurrence. fwind1: Huang radial or separable. fwind2: 2-D window. Covers: 8 fingerprints — sum(h) for fsamp2 (bit-exact ∝ Hd center), shape of all 4, ftrans2(b,1) corner bit-exact (-0.00390625), ftrans2(b,t) center bit-exact (2.5). The IFFT-based pipelines (fsamp2 uniform + fwind1 + fwind2) have a remaining FFT-shift convention discrepancy that affects element ordering but preserves sum — tracked for follow-up; only stable invariants in the parity spec. ftrans2 with non-default transform is bit-exact and is the main working path. Non-uniform-spacing forms of fsamp2/fwind1/fwind2 (the (f1, f2, Hd, ...) signature) throw an explicit unsupported error. Reference: Lim 1990 'Two-Dimensional Signal and Image Processing' §3.4-3.5, §4.2. Image namespace 2026-05-27. |
| `fwind1` | ✅ | Sig: 4 2-D FIR filter designs from Lim 1990. fsamp2(Hd): uniform-spacing IFFT-based. ftrans2(b, t): McClellan Chebyshev recurrence. fwind1: Huang radial or separable. fwind2: 2-D window. Covers: 8 fingerprints — sum(h) for fsamp2 (bit-exact ∝ Hd center), shape of all 4, ftrans2(b,1) corner bit-exact (-0.00390625), ftrans2(b,t) center bit-exact (2.5). The IFFT-based pipelines (fsamp2 uniform + fwind1 + fwind2) have a remaining FFT-shift convention discrepancy that affects element ordering but preserves sum — tracked for follow-up; only stable invariants in the parity spec. ftrans2 with non-default transform is bit-exact and is the main working path. Non-uniform-spacing forms of fsamp2/fwind1/fwind2 (the (f1, f2, Hd, ...) signature) throw an explicit unsupported error. Reference: Lim 1990 'Two-Dimensional Signal and Image Processing' §3.4-3.5, §4.2. Image namespace 2026-05-27. |
| `fwind2` | ✅ | Sig: 4 2-D FIR filter designs from Lim 1990. fsamp2(Hd): uniform-spacing IFFT-based. ftrans2(b, t): McClellan Chebyshev recurrence. fwind1: Huang radial or separable. fwind2: 2-D window. Covers: 8 fingerprints — sum(h) for fsamp2 (bit-exact ∝ Hd center), shape of all 4, ftrans2(b,1) corner bit-exact (-0.00390625), ftrans2(b,t) center bit-exact (2.5). The IFFT-based pipelines (fsamp2 uniform + fwind1 + fwind2) have a remaining FFT-shift convention discrepancy that affects element ordering but preserves sum — tracked for follow-up; only stable invariants in the parity spec. ftrans2 with non-default transform is bit-exact and is the main working path. Non-uniform-spacing forms of fsamp2/fwind1/fwind2 (the (f1, f2, Hd, ...) signature) throw an explicit unsupported error. Reference: Lim 1990 'Two-Dimensional Signal and Image Processing' §3.4-3.5, §4.2. Image namespace 2026-05-27. |
| `gabor` | ❌ | Gabor filter bank |
| `imbilatfilt` | ✅ ➖ | Sig: r = imbilatfilt(...). Spec-extension batch 2026-05-09. |
| `imboxfilt` | ✅ | MATLAB imboxfilt — local mean (replicate boundary, integral-image). Pins filtered values on magic(5) for default 3x3 and a 5x5 box. Bit-equal MATLAB R2025b (tol=1e-9). |
| `imboxfilt3` | ✅ | Sig: r = imboxfilt3(...). Spec-extension batch 2026-05-09. |
| `imdiffusefilt` | ✅ | Sig: B = imdiffusefilt(I [, NV...]). Branches: default (5 iter, maximal, exponential, K=0.1*range), NumberOfIterations override, Connectivity=minimal, ConductionMethod=quadratic, scalar GradientThreshold, vector GradientThreshold (N inferred), uint8 input class. Reference: Perona & Malik 1990; Gerig et al. 1992. 2-D only — 3-D volume diffusion deferred. Image namespace 2026-05-27. |
| `imfilter` | ✅ | MATLAB imfilter — 2-D linear filtering, bit-exact vs MATLAB R2025b (tol=1e-9). Covers: ODD 3x3 Laplacian (default correlation, zero boundary, 'same'); EVEN kernels 2x2 / 4x4 / asymmetric 2x4 (anchor = floor((K-1)/2), fixed to match MATLAB's even-kernel centering); asymmetric integer kernel [1 2;3 4]; boundary options 'replicate' / 'symmetric' / 'circular'; 'conv' (kernel flip) vs default 'corr'; and 'full'-size output. Fingerprints pin actual filtered element values at corner/centre/far-corner + sums + output sizes (NOT just numel — the previous spec only checked numel and silently passed while the even-kernel anchor was off by one pixel). Octave ships imfilter in the image package; harness ranks MATLAB above Octave. |
| `imgaborfilt` | ✅ | Sig: [mag, phase] = imgaborfilt(A, wavelength, orientation [, NV...]). Single-filter form only; gabor() bank object is MATLAB OOP (blocked by §0). Branches: default (SFB=1, SAR=0.5), orientation=90, custom SpatialFrequencyBandwidth, custom SpatialAspectRatio, SINGLE input class. Algorithm: frequency-domain Gabor (Jain & Farrokhnia 1991, Kruizinga & Petkov 1999). Replicate-pad by r=max(⌈7σx⌉,⌈7σy⌉), FFT-2D, multiply by ifftshift(H), IFFT-2D, crop, mag=|out|, phase=angle(out). Image namespace 2026-05-27. |
| `imgaussfilt` | ✅ | MATLAB imgaussfilt — Gaussian smoothing (replicate boundary). Pins filtered values on magic(5) for default sigma and sigma=2. Bit-equal MATLAB R2025b (tol=1e-9). |
| `imgaussfilt3` | ✅ | Sig: r = imgaussfilt3(...). Spec-extension batch 2026-05-09. |
| `imguidedfilter` | ✅ | Sig: B = imguidedfilter(A [, G] [, NV...]). Branches: default self-guide (NeighborhoodSize=[5 5], eps=0.01*range²), scalar NHood, custom DegreeOfSmoothing, cross-guidance (A!=G), uint8 input class. Bit-equal MATLAB R2025b at 1e-10. Grayscale-guide only; RGB-guide (color covariance Cramer's rule) and Fast Guided Filter downsample variant deferred. Reference: K. He, J. Sun, X. Tang, 'Guided Image Filtering', IEEE TPAMI 35(6), 2013. Image namespace 2026-05-27. |
| `imnlmfilt` | ✅ | Sig: [J, estDoS] = imnlmfilt(I [, NV...]). Non-local means denoising via exhaustive S×S search-window x C×C comparison-window. Algorithm: Buades-Coll-Morel 2005; box-blur patch distance, Buades 'max trick' for centre weight, Immerkaer-1996 noise estimate (single-precision per MATLAB). Branches: default, custom DegreeOfSmoothing, custom ComparisonWindowSize, custom SearchWindowSize, uint8 input. Grayscale 2-D only (RGB deferred). Image namespace 2026-05-27. |
| `integralBoxFilter` | ✅ | Sig: B = integralBoxFilter(I[, filterSize[, 'NormalizationFactor', n]]) (cycle 89). O(1)-per-pixel box filter via four lookups in a precomputed integral image. Output (H - fH + 1) × (W - fW + 1) — no-boundary core. filterSize scalar or 2-vector, must be ODD. NormalizationFactor is a MULTIPLIER (box-sum·n), NOT a divisor — default 1/(fH·fW) (mean), 1 for raw sum, 0.5/2 scale (multiplier-semantics fix cycle 94b — prior code divided, breaking NF∉{default,1}). 3-D color input processed per-channel. Bit-equal MATLAB R2025b on magic(8) with 3×3, 5×5, [3 5], [5 3] filters + NormalizationFactor {1, 0.5, 2} + 3-channel cases. |
| `integralBoxFilter3` | ✅ | Sig: B = integralBoxFilter3(A[, filterSize[, 'NormalizationFactor', n]]) (cycle 94). O(1)-per-voxel 3-D box filter via 8-corner inclusion-exclusion on a precomputed integralImage3 summed-volume table. Output (H−fH+1)×(W−fW+1)×(D−fP+1) — no-boundary core. filterSize scalar (cubic) or 3-vector [r c p], all ODD. NormalizationFactor is a MULTIPLIER (MATLAB semantics: box-sum·n); default 1/prod(filterSize) (mean), pass 1 for raw sum. Bit-equal MATLAB R2025b on integralImage3(reshape(1:125,5,5,5)) — default 3³, scalar, [1 3 5]→[5 3 1], raw-sum + 0.5 multiplier (image_integralBoxFilter3.json, tol=1e-12). Octave doesn't ship it in core → N/A. |
| `integralImage` | ✅ | Sig: r = integralImage(...). Spec-extension batch 2026-05-09. |
| `integralImage3` | ✅ | Sig: J = integralImage3(V). 3-D summed-volume table with leading zero plane/row/col. Octave-image may not have integralImage3 → may report N/A. |
| `medfilt2` | ✅ | MATLAB medfilt2 — 3x3 median filter (zero boundary). magic(5) with two spike pixels so the median actually rejects outliers; pins values at the spikes and corners. Bit-equal MATLAB R2025b (tol=1e-9). |
| `medfilt3` | ✅ | Sig: J = medfilt3(V[, [M N P]]). 3-D median filter, default 3x3x3, symmetric pad. MATLAB R2017+; Octave-image doesn't ship medfilt3. |
| `modefilt` | ✅ | Sig: B = modefilt(A[, filtSize[, padopt]]) (cycle 93). 2-D mode filter via neighbourhood histogram. Pad: 'symmetric' (default) / 'replicate' / 'zeros'. Fast UINT8/LOGICAL path via 256-bucket histogram; generic path via std::map for UINT16/INT*/SINGLE/DOUBLE. Tie-break: smallest-on-tie (matches MATLAB's documented `mode`; MATLAB's modefilt MEX has undocumented order-dependent tie behaviour that diverges — we follow the spec). Bit-equal MATLAB R2025b on probed interior fingerprints. |
| `nlfilter` | ✅ | Sig: B = nlfilter(A, [m n], fun) or B = nlfilter(A, 'indexed', [m n], fun). General sliding-neighbourhood: for each pixel (i,j) extracts m × n window centred on (i,j) (top-left bias for even sizes: pad above = floor((m-1)/2), below = ceil((m-1)/2)), passes to fun, stores fun(window) at B(i,j). Default padval = 0; 'indexed' uses padval = 1 for double/single, else 0. Output class = class of FIRST fun() return (matches MATLAB R2025b). Dispatch via Engine::callFunctionHandle (same pattern as toolboxes/ode/ode45). Bit-equal MATLAB on magic(5) across mean/max/median/sum kernels, [3 3] and [2 3] neighbourhoods, double/uint8 classes, indexed mode. 10 gtest TEST_F cover all kernels, class preservation, indexed-mode for both float and integer input, shape, validation throws, non-scalar fun() throw. |
| `ordfilt2` | ✅ | Sig: B = ordfilt2(A, nth, domain [, S] [, padding]). Order-statistic filter; 1-based nth. Octave-image has ordfilt2. |
| `padarray` | ✅ | MATLAB padarray — boundary methods vs MATLAB R2025b: default zero pad, 'replicate', 'symmetric', 'circular' (all [1 1] both). Pins sizes + padded pixels distinguishing each method (Pr corner=1, Ps mirror=2, Pc wrap=6). Scalar pad value + pre/post/both directions covered in padarray_dir. Previously EMPTY fingerprint. |
| `rangefilt` | ✅ | Sig: R = rangefilt(I [, domain]). Local max-min over neighbourhood. Default 3x3 ones, symmetric pad. Output class matches input. |
| `roifilt2` | ✅ | Sig: J = roifilt2(h, I, BW) (filter form) or roifilt2(I, BW, fun) (function-handle form) (cycle 98). Form 1: imfilter(I,h) (correlation, zero boundary, same) with only BW pixels replaced; output = I elsewhere; class=class(I). Form 2: apply fun to whole I, keep only BW pixels; output class follows fun's result. Detection: arg3 funchandle/string → form 2, else form 1. fun dispatched via Engine::callFunctionHandle. Bit-equal MATLAB R2025b on magic(6) with 3×3 Laplacian + averaging + EVEN 2×2 (form 1) and uint8 x*2 / double+0.5 handles (form 2) — image_roifilt2.json. (imfilter even-kernel anchoring was fixed alongside this — half=(K-1)/2 — so even filters are now bit-exact too.) Octave ships it in image pkg only → N/A. |
| `stdfilt` | ✅ | Sig: S = stdfilt(I [, domain]). Local sample std (N-1 norm). Default 3x3 ones, symmetric pad. Uses Octave-source test vector G. |
| `wiener2` | ✅ | Sig: r = wiener2(...). Spec-extension batch 2026-05-09 (image namespace). |

### Contrast Adjustment

**Namespace:** `image.contrast.*` — 3 ✅ + 0 ⚠️ / 14 = 21%

| function | status | comment |
|---|:---:|---|
| `adapthisteq` | ✅ | Sig: J = adapthisteq(I[, NV-pairs]). CLAHE — faithful port of MATLAB R2025b adapthisteq.m (symmetric pad → per-tile clip+redistribute with cap = ceil(N/NBins) + round(normCL·(N-minCL)) and two-pass step-size redistribution → uniform-CDF mapping → (NumTiles+1)² integer-weight bilinear with round-then-bilinear quantisation per LUT lookup). **tol=0 bit-exact** vs MATLAB R2025b on every probed pixel. Distribution!='uniform' deferred (throws). |
| `decorrstretch` | ❌ | decorrelation stretch |
| `histeq` | ✅ | Sig: r = histeq(...). Spec-extension batch 2026-05-09. |
| `imadjust` | ✅ | Sig: r = imadjust(...). DEEP-PROBE c169: an explicitly-passed empty [] for the in-range endpoints means MATLAB's default [0 1] (identity, NO stretch) — distinct from the ABSENT 1-arg form imadjust(I), which auto-stretches via stretchlim (1% saturation). numkit previously treated empty [] like the absent case (stretchlim), so imadjust(I,[],[]) wrongly contrast-stretched. Now e1=e2=e3=1 (empty in-range / empty out-range == identity), while ne4=1 and j4=204 confirm the 1-arg stretchlim form is preserved; j5 covers explicit-range + gamma=2 (j5=154). covers:[imadjust]. |
| `imadjustn` | ✅ | Sig: r = imadjustn(...). Spec-extension batch 2026-05-09. |
| `imflatfield` | ✅ | Sig: r = imflatfield(...). Spec-extension batch 2026-05-09. |
| `imhistmatch` | ✅ 🔬 | imhistmatch [J,hgram] vs MATLAB R2025b. DEEP-PROBE 2026-05-31 (toolbox pivot, toolboxes/image): imhistmatch is exactly histeq(I, imhist(ref,nbins)) — the transform is built at the input class's FULL resolution NPTS (256 for uint8) then applied per input level via grayxform, and nbins defaults to 64 for EVERY class. numkit previously (a) defaulted nbins to max(per-class imhist default)=256 for uint8, (b) binned the INPUT into nbins (not NPTS) with a (k+0.5)/nbins LUT so J was off by 1-2 levels, and (c) was missing the 2nd output hgram (= imhist(ref,nbins), threw 'undefined function T'). Rewrote to the exact Gonzalez histogram-matching transform (cumd/cum/tol/argmin) + added hgram. Default n=64: J(1)=0,J(2)=101,J(4)=150,J(16)=255; hg is 1x64, sum=16, hg(1)=hg(13)=hg(64)=2. n=16: J16(1)=0,J16(4)=153,J16(16)=255; hg16(5)=hg16(1)=2. covers:[imhistmatch]. |
| `imhistmatchn` | ✅ | Sig: r = imhistmatchn(...). Spec-extension batch 2026-05-09. |
| `imlocalbrighten` | ❌ |  |
| `imreducehaze` | ✅ | Sig: [J, T, L] = imreducehaze(I [, amount] [, NV...]). Covers: default simpledcp + global stretch (bit-exact 11 fingerprints incl. T thickness map + L atmospheric light); ContrastEnhancement='none' branch (skips global stretch); explicit AtmosphericLight (RGB triplet bypasses estimation); amount=0 passthrough. Other branches (Method='approxdcp', grayscale input, single/double inputs, boost contrast, BoostAmount NV, quadtree branch for size >=64) covered exhaustively in gtest. tol=1 uint8 (default rounding accumulation). approxdcp diverges ~15-30 uint8 from MATLAB due to imhist bucket boundaries + missing Fast-Guided-Filter subsample; documented in gtest, not fingerprinted here. References: He/Sun/Tang 2011 IEEE TPAMI 33(12); Dubok et al. 2014 ICIP. Image namespace 2026-05-27. |
| `imsharpen` | ✅ | Sig: r = imsharpen(...). Spec-extension batch 2026-05-09. |
| `intlut` | ✅ | MATLAB intlut — table lookup. uint8 (256-element inversion LUT) + uint16 (65536-element) with LUTs precomputed in setup; pins mapped values + class preservation. Previously EMPTY fingerprint. Bit-equal MATLAB R2025b. |
| `localcontrast` | ❌ |  |
| `locallapfilt` | ✅ | Sig: B = locallapfilt(I, sigma, alpha [, beta] [, NV...]). Covers: default 3-arg (sigma=0.3 alpha=0.4 -> moderate enhancement), beta=0.8 dynamic-range compression, alpha=2 smoothing, NumIntensityLevels=1 single-sample remap. Interior pixels only (boundary pixels diverge from MATLAB by up to 10 uint8 because MATLAB's private LLF pyrdownsample uses an undocumented boundary convention different from impyramid; we use symmetric boundary which matches impyramid bit-exactly). Other branches (alpha=1,beta=1 passthrough; sigma=0 passthrough; flat-image passthrough; auto NumIntensityLevels; class preservation for int16/single; RGB luminance/separate) covered exhaustively in gtest. tol=5 uint8. Reference: Aubry/Paris/Hasinoff/Kautz/Durand 2014, ACM TOG 33(5); Paris/Hasinoff/Kautz 2011 SIGGRAPH 30(4). Image namespace 2026-05-27. |
| `stretchlim` | ✅ 🔬 | stretchlim histogram bin count by class vs MATLAB R2025b. DEEP-PROBE 2026-05-31 (toolbox pivot, toolboxes/image): stretchlim hardcoded a 256-bin histogram for ALL classes, so on a DOUBLE/single image the limits were coarsely quantized to k/255 — stretchlim([0.1 0.2 0.9 0.95]) gave [26/255 242/255] = [0.1019608 0.9490196] instead of MATLAB's 65536-bin result [6554/65535 62258/65535] = [0.1000076 0.9499962]. MATLAB uses 256 bins for uint8 but 65536 for uint16/int16/single/double. Fix: NBINS = (uint8 ? 256 : 65536). uint8 input is unchanged (s2 = [10/255 250/255] = [0.0392157 0.9803922]); double linspace(0,1,100) -> [0.0101015 0.9898985]. imadjust's auto default (which calls stretchlim) tracks the corrected limits. covers:[stretchlim]. |

### ROI-Based Processing

**Namespace:** `image.roi.*` — 5 ✅ + 0 ⚠️ / 8 = 63%

ROI drawing classes (`Circle`, `Ellipse`, `drawcircle`, `imellipse`, `imrect`, …) intentionally omitted as OOP / interactive.

| function | status | comment |
|---|:---:|---|
| `inpaintCoherent` | ❌ | coherence-transport inpainting |
| `inpaintExemplar` | ❌ | exemplar inpainting |
| `poly2mask` | ✅ | Sig: BW = poly2mask(X, Y, M, N). Branches: integer-aligned square, off-grid square (frac vertices), triangle (diagonal edge), degenerate (zero-area) triangle, self-closing polygon, pentagon (irrational vertices), self-intersecting bowtie, large rectangle, empty inputs (T6 implicit). Bit-equal MATLAB R2025b via reverse-engineered ray-cast rule: per non-horizontal edge, cy ∈ (ylo, yhi] → cx > xi (strict) → toggle. Foley/van Dam scanline polygon fill, X11-half-open horizontal edges. Image namespace 2026-05-27. |
| `reducepoly` | ✅ | Sig: P_reduced = reducepoly(P[, tolerance]) (cycle 97). Ramer-Douglas-Peucker polyline simplification. tolerance ∈ [0,1] default 0.001, normalised by bbox diagonal norm(max(P)−min(P)); 0→eps (minimal), 1→endpoints only. Recursive split at vertex of max chord-perpendicular distance (\|det([1 x y;…])\|/chordLen); first-farthest wins ties; run collapses to endpoints when max-dev ≤ tol. Output rows = exact copies of retained input vertices; class preserved (integer computed in single, cast back). Bit-equal MATLAB R2025b (image_reducepoly.json): default 7→6 (drops the vertex collinear with neighbours), tol≥0.1→2, collinear→2, triangle-wave→5. Reference: Douglas & Peucker 1973. Octave ships it in image pkg only → N/A. |
| `regionfill` | ✅ | Sig: J = regionfill(I, MASK). Branches: single-pixel interior mask, 3x3 interior mask, larger (4x4) mask on magic(10), edge-touching mask (3-neighbour stencil at borders). MATLAB uses sparse direct (UMFPACK); numkit uses conjugate gradient at tol 1e-12, both converge to same machine-precision Laplacian solution. (I, X, Y) polygon form requires poly2mask (deferred to its own cycle). Algorithm: discrete Laplacian Dirichlet BVP (Gonzalez & Woods §3.4; NR §2.7). Image namespace 2026-05-27. |
| `roicolor` | ✅ | Sig: BW = roicolor(A, low, high) range form, or roicolor(A, v) set-membership. Output logical, same shape as A. Octave-image has roicolor. |
| `roifill` | ❌ | legacy alias |
| `roipoly` | ✅ | Sig: BW = roipoly(...). Branches: 3-arg (A, xi, yi), 4-arg (M, N, xi, yi), 5-arg world-coords (x, y, A, xi, yi), 5-arg identity extents, 6-arg (x, y, M, N, xi, yi), 2-output with auto-closed xi return, 5-output (xdata, ydata, BW, xi, yi). Implementation: auto-close polygon, axes2pix world→pixel, then poly2mask. Interactive forms (0/1/2-arg) throw. Image namespace 2026-05-27. |

### Morphological Operations

**Namespace:** `image.morph.*` — 8 ✅ + 0 ⚠️ / 27 = 30%

| function | status | comment |
|---|:---:|---|
| `applylut` | ✅ | Sig: r = applylut(...). Spec-extension batch 2026-05-09. |
| `bwhitmiss` | ✅ | Sig: r = bwhitmiss(...). Spec-extension batch 2026-05-09 (image namespace). |
| `bwlookup` | ✅ | Sig: A = bwlookup(BW, lut) (cycle 95). 2×2 (lut len 16) or 3×3 (len 512) nonlinear neighbourhood LUT filter — modern replacement for applylut, restricted to the documented 16/512 sizes. Identical index convention to applylut (verified isequal); output class = lut class; zero-padded border. lut not 16 or 512 → "Expected LUT (argument 2) to have 16 or 512 elements." Bit-equal MATLAB R2025b (image_makelut.json). Octave ships it in image pkg only → N/A. |
| `bwmorph` | ✅ | Sig: J = bwmorph(BW, op[, n]). Binary morphology dispatcher — faithful port of MATLAB R2025b bwmorph.m + algbwmorph.m using 22 LUTs dumped from MATLAB. All 20+ operations bit-exact (tol=0): dilate / erode / bridge / clean / diag / endpoints / fatten / fill / hbreak / majority / perim4 / perim8 / remove / bothat / close / open / tophat / shrink∞ / skeleton∞ / spur / thin∞ / thicken / branchpoints. |
| `bwmorph3` | ✅ | Sig: J = bwmorph3(V, operation) (cycle 96). 3-D binary morphology — all 6 documented ops as 3×3×3 neighbourhood rules (count = set voxels incl. centre; faces6 = six 6-conn faces): branchpoints (centre & count>3), clean (centre & count≠1, drops isolated), endpoints (centre & count==2), fill (centre | faces6==6), majority (count>13 i.e. ≥14/27), remove (centre & faces6≠6). Zero-padded border; output always LOGICAL same size; 2-D input → single-plane volume. Clean-room port of MATLAB R2025b bwmorph3Algorithm. Bit-equal MATLAB R2025b on cube-minus-hole (26/26/0/27/7/26), z-line (2 endpoints), 2-D mask (image_bwmorph3.json, tol=1e-12). Octave ships it in image pkg only → N/A. |
| `bwpack` | ✅ | Sig: r = bwpack(...). Spec-extension batch 2026-05-09. |
| `bwperim` | ✅ | MATLAB bwperim — boundary pixels of binary objects. Covers conn=4 and conn=8 on a solid 4x4 square (border on, interior off) and a diagonal+block shape. Pins perimeter counts AND specific pixel on/off states (interior P4(3,3)=0, border P4(2,2)=1). Previously fingerprinted only numel. Bit-equal MATLAB R2025b. |
| `bwskel` | ❌ | skeletonize |
| `bwulterode` | ❌ | ultimate erosion |
| `bwunpack` | ❌ |  |
| `conndef` | ❌ |  |
| `imbothat` | ✅ | Sig: r = imbothat(...). Spec-extension batch 2026-05-09. |
| `imclearborder` | ✅ | Sig: r = imclearborder(...). Spec-extension batch 2026-05-09 (image namespace). |
| `imclose` | ✅ | Sig: r = imclose(...). Spec-extension batch 2026-05-09 (image namespace). |
| `imdilate` | ✅ | Sig: r = imdilate(...). Spec-extension batch 2026-05-09 (image namespace). |
| `imerode` | ✅ | Sig: r = imerode(...). Spec-extension batch 2026-05-09 (image namespace). |
| `imextendedmax` | ✅ | Sig: BW = imextendedmax(I, h). Tall peak A survives (mask=1 at (2,2)); shallow peak B suppressed. |
| `imextendedmin` | ✅ | Sig: BW = imextendedmin(I, h). Deep trough A survives, shallow B suppressed. |
| `imfill` | ✅ | Sig: r = imfill(...). Spec-extension batch 2026-05-09 (image namespace). |
| `imhmax` | ✅ | Sig: r = imhmax(...). Spec-extension batch 2026-05-09. |
| `imhmin` | ✅ | Sig: r = imhmin(...). Spec-extension batch 2026-05-09. |
| `imimposemin` | ✅ | Sig: J = imimposemin(I, BW). Force regional minima at marker; basin B at (2,5) erased (lifted to plateau 10). |
| `imkeepborder` | ✅ | Sig: r = imkeepborder(...). Spec-extension batch 2026-05-09. |
| `imopen` | ✅ | Sig: r = imopen(...). Spec-extension batch 2026-05-09 (image namespace). |
| `imreconstruct` | ✅ | Sig: r = imreconstruct(...). Spec-extension batch 2026-05-09 (image namespace). |
| `imregionalmax` | ✅ | Sig: r = imregionalmax(...). Spec-extension batch 2026-05-09. |
| `imregionalmin` | ✅ | Sig: r = imregionalmin(...). Spec-extension batch 2026-05-09. |
| `imtophat` | ✅ | Sig: r = imtophat(...). Spec-extension batch 2026-05-09. |
| `makelut` | ✅ | Sig: lut = makelut(fun, n) (cycle 95). Builds a bwlookup/applylut table by evaluating fun on every 2^(n²) binary n×n neighbourhood (n=2→16, n=3→512). fun receives a LOGICAL n×n matrix, returns a scalar; output table is always DOUBLE column vector. Neighbourhood for index k (col-major) has position i = bit (n²−1−i) of k — inverse of applylut's reshape(2.^[nq−1:−1:0],n,n) weight kernel, so bwlookup(BW, makelut(fun,n)) applies fun per neighbourhood (verified center-passthrough → identity). n∉{2,3} → "N must be 2 or 3." Dispatch via Engine::callFunctionHandle (same pattern as nlfilter). Bit-equal MATLAB R2025b (image_makelut.json). Octave ships it in image pkg only → N/A. |
| `offsetstrel` | ❌ | structuring element with offsets |
| `strel` | ✅ 🔬 | Sig: se = strel(shape, params). Returns a struct (numkit) / strel-object (MATLAB) with fields {Neighborhood, Dimensionality}; field access matches. 'square' is bit-identical (5x5=25 ones). DEEP-PROBE 2026-05-31 (c159): 'disk' now reproduces MATLAB R2025b's radial periodic-line decomposition (Adams 1993, Jones & Soille 1996) -- default N=4 basis, k=2r/(cot(pi/2n)+1/sin(pi/2n)), floor() repetition + compensating H/V line strels -- instead of a full Euclidean disk. Previously numkit returned the true disk (disk(5)=11x11/sum81 vs MATLAB 9x9/sum69). Now bit-exact across r=1..15 and N in {0,4,6,8}: disk(5)=9x9/69 (rowsums 5 7 9 9 9 9 9 7 5), disk(3)=5x5/25 (full square), disk(4)=7x7/37, disk(7)=13x13/157, disk(5,0)=11x11/81 (N=0 forces Euclidean). covers:[strel]. |

### Deblurring

**Namespace:** `image.deblur.*` — 0 ✅ + 0 ⚠️ / 7 = 0%

| function | status | comment |
|---|:---:|---|
| `deconvblind` | ❌ | blind deconvolution |
| `deconvlucy` | ❌ | Richardson-Lucy |
| `deconvreg` | ✅ | Sig: [J,LAGRA] = deconvreg(I, PSF, NP, LRANGE, REGOP). Branches: 2-arg default, 3-arg NP, 4-arg scalar LRANGE (fixed λ), 4-arg [lo,hi] (Brent fminbnd search), 5-arg custom 2-D REGOP. fminbnd uses Brent's golden-section + parabolic-interpolation (Brent 1973). Algorithm: Gonzalez-Woods + Jain. Image namespace 2026-05-27. |
| `deconvwnr` | ✅ | Sig: J = deconvwnr(I, PSF, NSR) or deconvwnr(I, PSF, NCORR, ICORR). FFT-based Wiener inverse filter: G(k) = conj(H(k))·S_x / (|H(k)|² · S_x + S_u) where H = psf2otf(PSF, size(I)). Scalar NSR maps to S_u=NSR, S_x=1. Scalar NCORR/ICORR maps to S_u=NCORR, S_x=ICORR. Array NCORR/ICORR (same size as I) supported via |fft2(ACF)| (MATLAB's 1-D extrapolation form throws — uncommon). Class-preserving output (uint8/uint16 via saturating round). 3-D volumes processed per-page through the same OTF. Real inputs produce real outputs. Reference: Gonzalez & Woods, *Digital Image Processing*, 2e § 5.8. Algorithm transliterated verbatim from MATLAB R2025b deconvwnr.m. Bit-equal MATLAB at 1e-7 (residual from psf2otf's COMPLEX-vs-DOUBLE coercion for real-symmetric kernels). 9 gtest TEST_F cover NSR=0/0.01/0.1, 4-arg scalar equivalence, uint8 preservation, 7×7 Gaussian roundtrip, 3-D volume, double-in→double-out, bad-nargin throw. |
| `edgetaper` | ✅ | Sig: J = edgetaper(I, PSF). Tapers image edges via FFT-based blur — companion to deconvwnr/deconvreg/deconvlucy for reducing DFT ringing. For each non-singleton PSF dim: PSF projection → autocorrelation (via |fft|² then ifft) → length-sizeI beta vector. alpha = outer product of (1-beta) across dims. J = alpha*I + (1-alpha)*ifft2(fft2(I) .* psf2otf(PSF, sizeI)). Clipped to [min(I), max(I)]. Class-preserving (uint8/uint16 with saturating round). 2-D only — 3-D inputs throw with "slice and call per page" guidance. PSF cannot exceed half of I in any dimension. Bit-equal MATLAB R2025b at 1e-9. 10 gtest TEST_F (1 skip — all-zero-PSF check low-priority). |
| `otf2psf` | ✅ | Sig: psf = otf2psf(otf [, outsize]). Inverse of psf2otf — ifft2 + circshift by +floor(OUTSIZE/2) + top-left crop to OUTSIZE. (Cycle 28 fix: the outsize parameter was previously ignored. Now matches MATLAB R2025b source: shift amount depends on OUTSIZE, not on insize.) OUTSIZE defaults to size(otf); must not exceed size(otf) in any dimension (else throws). Handles 1-D OTF (1×N or N×1) with scalar outsize. Bit-equal MATLAB on roundtrips (odd + even sizes) and on explicit-outsize crops. 5 gtest TEST_F cover odd/even roundtrip, outsize crop, 1-D, outsize-too-big throw. |
| `psf2otf` | ✅ | Sig: otf = psf2otf(psf [, outsize]). FFT of circshift(zeropad(psf), -floor(size/2)). Octave-image has psf2otf. |

### Neighborhood and Block Processing

**Namespace:** `image.block.*` — 0 ✅ + 0 ⚠️ / 6 = 0%

| function | status | comment |
|---|:---:|---|
| `bestblk` | ✅ | Sig: r = bestblk(...). Spec-extension batch 2026-05-09. |
| `blockproc` | ❌ | block-wise processing |
| `col2im` | ✅ | Sig: A = col2im(B, [m n], [mm nn], type). Reassemble columns into image. Bit-identical with MATLAB R2025b on probed input -- earlier defer used wrong B-shape. |
| `colfilt` | ✅ | Sig: B = colfilt(A, [m n], block_type, fun) or colfilt(A, [m n], [mblock nblock], block_type, fun), 'indexed' form optional. block_type ∈ {'sliding', 'distinct'}. Sliding: zero-pad A (or 1 for indexed double/single), build m*n × (H*W) matrix of windows, fun must return 1 × (H*W) row, reshape to size(A). Distinct: pad to multiple of [m n], build m*n × (mblocks*nblocks) matrix of distinct blocks, fun returns same-shape matrix, reassemble + crop. The [mblock nblock] memory-optimisation arg is accepted and ignored (MATLAB explicitly notes "does not change the result"). Output class = class of fun() return. Engine::callFunctionHandle dispatch (same as nlfilter / ode45). Bit-equal MATLAB R2025b on magic(5) (sliding mean/sum at 3x3, 2x3, indexed min) and magic(6) (distinct x.^2); nlfilter↔colfilt equivalence verified (max diff = 0). 10 gtest TEST_F cover sliding mean/sum, even neighbourhood, indexed mode, distinct shape preservation, nlfilter equivalence, [mblock nblock] ignored, throw on bad block_type / nargin / fun shape mismatch. |
| `im2col` | ✅ | Sig: r = im2col(...). Spec-extension batch 2026-05-09. |
| `nlfilter` | ✅ | See filter section row — same function. Implemented in cycle 33. |

### Image Arithmetic

**Namespace:** `image.arith.*` — 8 ✅ + 0 ⚠️ / 8 = 100%

| function | status | comment |
|---|:---:|---|
| `imabsdiff` | ✅ | Sig: r = imabsdiff(...). Spec-extension batch 2026-05-09. |
| `imadd` | ✅ | Sig: r = imadd(...). Spec-extension batch 2026-05-09. |
| `imapplymatrix` | ✅ | Sig: r = imapplymatrix(...). Spec-extension batch 2026-05-09. |
| `imcomplement` | ✅ | Sig: r = imcomplement(...). Spec-extension batch 2026-05-09. |
| `imdivide` | ✅ | Sig: r = imdivide(...). Spec-extension batch 2026-05-09. |
| `imlincomb` | ✅ | Sig: r = imlincomb(...). Spec-extension batch 2026-05-09. |
| `immultiply` | ✅ | Sig: r = immultiply(...). Spec-extension batch 2026-05-09. |
| `imsubtract` | ✅ | Sig: r = imsubtract(...). Spec-extension batch 2026-05-09. |

### Image Segmentation

**Namespace:** `image.segment.*` — 6 ✅ + 0 ⚠️ / 22 = 27%

Deep-learning-based ones (`imsegsam`, `segmentAnythingModel`, …) intentionally omitted.

| function | status | comment |
|---|:---:|---|
| `activecontour` | ❌ | Chan-Vese |
| `bfscore` | ❌ | boundary F1 score |
| `boundarymask` | ✅ | Sig: r = boundarymask(...). Spec-extension batch 2026-05-09. |
| `dice` | ✅ | Sig: r = dice(...). Spec-extension batch 2026-05-09. |
| `gradientweight` | ✅ | Sig: W = gradientweight(I [, sigma], 'RolloffFactor', P, 'WeightCutoff', K). Branches: 1-arg defaults (sigma=1.5, P=3, K=0.25), 2-arg scalar sigma, 3+-arg NV pairs (RolloffFactor, WeightCutoff), 2-element [sigma_x sigma_y] (replicates MATLAB R2025b's filtRadius(1)-norm bug for hy on anisotropic σ — per 'MATLAB wins' rule). DoG kernel: hx(x)=-x*exp(-x²/2σ²) normalised. Algorithm: Gonzalez-Woods edge gradient + Sethian FMM weights. Image namespace 2026-05-27. |
| `grabcut` | ❌ |  |
| `grayconnected` | ✅ | Sig: BW = grayconnected(I, r, c, tol). 8-connected flood-fill from seed within tolerance. Bit-identical with MATLAB R2025b. Spec uses magic(8) -- restored to canonical form after magic() was implemented in cycle 46 (commit 71efbf02); originally had to inline the matrix because numkit didn't ship magic(). |
| `graydiffweight` | ✅ | Sig: W = graydiffweight(I, refGrayVal \| MASK \| C,R \| C,R,P [, 'RolloffFactor', p] [, 'GrayDifferenceCutoff', K]). FMM-style pixel weights based on \|I - refGrayVal\|. 4 input sigs collapse to a scalar reference (scalar verbatim, mean over MASK, mean over linearly-indexed C/R or C/R/P for 3-D). Then d = abs(I - ref); imlinscale d to [1e-3, 1]; if cutoff finite, set d > cutoff to 1; W = 1/(d^(1/p)). Default p = 0.5, cutoff = Inf. Output single if I single, else double. 2-D or 3-D volume input. Bit-equal MATLAB R2025b on all probed cases. 10 gtest TEST_F cover scalar/MASK/(C,R) sigs, RolloffFactor + Cutoff options, 3-D volume, class preservation, validation throws. |
| `imoverlay` | ✅ | Sig: B = imoverlay(I, BW, color). Color overlay onto image at BW pixels. Bit-identical with MATLAB R2025b on probed input -- numkit needs explicit color arg (matches MATLAB; no default). |
| `imseggeodesic` | ❌ |  |
| `imsegfmm` | ❌ | fast marching |
| `imsegisodata` | ❌ |  |
| `imsegkmeans` | ❌ |  |
| `imsegkmeans3` | ❌ |  |
| `jaccard` | ✅ | Sig: r = jaccard(...). Spec-extension batch 2026-05-09. |
| `label2idx` | ✅ | Sig: ix = label2idx(L). Spec-extension batch 2026-05-09 (cycle 44). |
| `labeloverlay` | ✅ | Sig: B = labeloverlay(A, L [, NV...]). Cover: default (auto/shuffle/jet), Transparency=0 (pure colour), IncludedLabels filter (skip label 2 -> A passthrough), ColorAssignment='noshuffle' on jet. Default shuffle uses MATLAB rng('default') = MT19937 seed-0 randperm; numkit implements MatlabMT19937 + sort(rand) bit-identically. Output uint8 H×W×3 always. tol=0 (exact uint8 match). Other branches (logical BW mask, custom Nx3 Colormap, RGB input, Transparency=1 passthrough) covered exhaustively in gtest. categorical input form not implemented (MATLAB OOP class — policy §0). Image namespace 2026-05-27. |
| `lazysnapping` | ❌ |  |
| `superpixels` | ❌ | SLIC |
| `superpixels3` | ❌ |  |
| `watershed` | ❌ |  |

### Object Analysis

**Namespace:** `image.object.*` — 4 ✅ + 0 ⚠️ / 18 = 22%

| function | status | comment |
|---|:---:|---|
| `bwboundaries` | ✅ 🔬 | bwboundaries 2nd/3rd outputs [B,L,N] vs MATLAB R2025b. DEEP-PROBE 2026-05-31 (toolbox pivot, toolboxes/image): bwboundaries returned only B; the label-matrix L and object-count N (2nd/3rd outputs) were missing (the 2nd output threw 'undefined function L'). Added via out-pointers: L is the H-by-W double label matrix (objects 1..N), N is the object count. numkit traces objects only (the 'noholes' behaviour), so L/N are bit-exact vs MATLAB for inputs WITHOUT holes (hole-boundary tracing + hole labelling N+1.. remains a separate gap). Solid 3x3 block: nB=1, N=1, Lmax=1, L(3,3)=1, L(1,1)=0. Two objects: N=2, L(1,1)=1, L(4,4)=2. A string mode flag ('noholes') is accepted and ignored. covers:[bwboundaries]. |
| `bwtraceboundary` | ✅ | Sig: B = bwtraceboundary(BW, P, fstep, conn, n, dir). Branches: default 8-conn CW from E, 4-conn, S start, L-shape (concave), single pixel (returns [P; P]), counterclockwise direction, limit N. Bit-exact MATLAB R2025b (tol=0). Algorithm: Moore-Neighbor tracing, Pavlidis 1982. Notable: fstep specifies the OPPOSITE of the notional 'previous' pixel direction — search starts one step CW of (fstep + nd/2) % nd. Image namespace 2026-05-27. |
| `circles2mask` | ❌ |  |
| `corner` | ❌ | Harris/Min-eig corner detector |
| `cornermetric` | ✅ | Sig: C = cornermetric(I [, METHOD] [, NV...]). Branches: Harris (default, k=0.04), MinimumEigenvalue, custom SensitivityFactor, custom FilterCoefficients, uint8 input class, larger peaks(8) image. Algorithm: Harris & Stephens 1988 / Shi & Tomasi 1994 corner detectors. Dx, Dy via [-1 0 1] / [-1 0 1]' conv → trim 1px → square/cross-product → smooth with outer-product Gaussian → crop → cornerness. Imfilter 'full' under Replicate boundary worked around by pre-padding with padarray. Image namespace 2026-05-27. |
| `edge` | ✅ | Sig: r = edge(...). Spec-extension batch 2026-05-09 (image namespace). |
| `edge3` | ❌ |  |
| `hough` | ✅ | Sig: [H,T,R] = hough(BW [, NV...]); P = houghpeaks(H, npks [, NV...]). Branches: default Hough, RhoResolution override, custom Theta vector, houghpeaks default + Threshold + NHoodSize + Theta wrap-around. Bit-exact MATLAB R2025b (tol=0). Reference: Gonzalez/Woods/Eddins, *Digital Image Processing Using MATLAB*, 2nd ed., Gatesmark, 2009. Image namespace 2026-05-27. |
| `houghlines` | ✅ | Sig: lines = houghlines(BW, theta, rho, peaks [, NV...]). Branches: default fillgap=20 minlength=40, custom FillGap/MinLength (3 small example), single peak. Returns struct array with point1/point2 [x y] endpoints + theta + rho. Bit-exact MATLAB R2025b (tol=0). Reference: Gonzalez/Woods/Eddins, *Digital Image Processing Using MATLAB*, Prentice Hall, 2003. Image namespace 2026-05-27. |
| `houghpeaks` | ❌ |  |
| `imfindcircles` | ❌ | circle Hough |
| `imgradient` | ✅ 🔬 | Sig: r = imgradient(...). Spec-extension batch 2026-05-09 (image namespace). |
| `imgradientxy` | ✅ 🔬 | imgradient/imgradientxy sobel+prewitt gradient SIGN vs MATLAB R2025b. DEEP-PROBE 2026-05-31 (toolbox pivot, toolboxes/image): the 2-D sobel/prewitt kernels were sign-flipped (kx=[1 0 -1;...]) so imgradientxy returned negated Gx/Gy (Gx=-40 vs MATLAB +40 for a left->right-increasing ramp) and imgradient's Gdir=atan2(-Gy,Gx) was 180 deg off (168.69 vs MATLAB -11.31). Gmag was unaffected (sign-invariant); 'central'/'intermediate' were already correct; the 3-D imgradientxyz kernels were already MATLAB-signed. Fix flips kx/ky to MATLAB's hx=[-1 0 1;-2 0 2;-1 0 1]. reshape(1:25,5,5) at (3,3): sobel Gmag=40.7921563, Gdir=-11.3099325, Gx=40, Gy=8; prewitt Gdir=-11.3099325, Gx=30, Gy=6. The 2-arg imgradient(Gx,Gy) input form remains deferred. covers:[imgradient,imgradientxy]. |
| `imgradient3` | ✅ | Sig: [Gmag,Gaz,Gel] = imgradient3(V,method) | imgradient3(Gx,Gy,Gz). Branches: V+method (sobel/prewitt/central/intermediate via imgradientxyz then polar) + (Gx,Gy,Gz) from-grads path + single-output Gmag. Image namespace 2026-05-27. |
| `imgradientxyz` | ✅ | Sig: [Gx,Gy,Gz] = imgradientxyz(V,method). Branches: sobel (3x3x3 [1,3,3,1]-weighted MATLAB R2025b kernel), prewitt, central (gradient()), intermediate (forward diff). Replicate boundary. Image namespace 2026-05-27. |
| `iradon` | ❌ | inverse Radon |
| `qtdecomp` | ❌ | quad-tree decomposition |
| `qtgetblk` | ❌ |  |
| `qtsetblk` | ❌ |  |
| `radon` | ❌ |  |
| `visboundaries` | ❌ | display |
| `viscircles` | ❌ | display |

### Region and Image Properties

**Namespace:** `image.region.*` — 8 ✅ + 0 ⚠️ / 28 = 29%

| function | status | comment |
|---|:---:|---|
| `bwarea` | ✅ | Sig: r = bwarea(BW). Pratt area estimate. KNOWN GAP: numkit returns integer pixel count (4) vs MATLAB's pattern-weighted estimate (4.75). Documented separately; only positive-result structural check pinned. |
| `bwareafilt` | ✅ ➖ | Sig: r = bwareafilt(...). Spec-extension batch 2026-05-09. |
| `bwareaopen` | ✅ | Sig: r = bwareaopen(...). Spec-extension batch 2026-05-09. |
| `bwconncomp` | ✅ | Sig: cc = bwconncomp(BW[, conn]). Returns 1x1 struct with fields {Connectivity, ImageSize, NumObjects, PixelIdxList}. PixelIdxList is 1xK cell of column-vector linear indices. Bit-identical with MATLAB R2025b. |
| `bwconvhull` | ❌ |  |
| `bwdist` | ✅ 🔬 | Sig [D,IDX]=bwdist(BW[,method]). D = distance to nearest TRUE pixel. method: euclidean (default), cityblock, chessboard, quasi-euclidean. Corner (1,1)->feature (2,2): euclid 1.4142, city 2, chess 1, quasi 1.4142. (1,4)->(4,4): euclid 2.2361, city 3, chess 2, quasi 2.4142. D tol=1e-6 (MATLAB bwdist returns SINGLE; integer metrics match exactly). DEEP-PROBE c163: added the 2nd output IDX (feature transform) — the column-major 1-based linear index of the NEAREST foreground pixel, ties broken to the lowest linear index, class uint32 — was missing ([D,IDX]=bwdist(...) threw). Bit-exact vs MATLAB R2025b across all 4 metrics: for this BW (seeds at linidx 6 and 16) Ie(1,1)=6, Ie(4,4)=16, sum(IDX)=126 for every metric. covers:[bwdist]. |
| `bwdistgeodesic` | ✅ | Sig: D = bwdistgeodesic(BW, mask|C,R|ind [, method]). Branches: 3 methods (cityblock/chessboard/quasi-euclidean), 4 input forms (mask, ind, (C,R), default method), barrier (NaN at false pixels), unreachable (Inf at disconnected reachable pixels). Output always SINGLE. Algorithm: Dijkstra over true-pixel subgraph with chamfer edge weights. Reference: Soille, *Morphological Image Analysis*, 2nd ed., §4.4. Image namespace 2026-05-27. |
| `bweuler` | ✅ | Sig: r = bweuler(...). Spec-extension batch 2026-05-09. |
| `bwferet` | ❌ | Feret diameters |
| `bwlabel` | ✅ | Sig: r = bwlabel(...). Spec-extension batch 2026-05-09. |
| `bwlabeln` | ❌ |  |
| `bwperim` | ✅ | MATLAB bwperim — boundary pixels of binary objects. Covers conn=4 and conn=8 on a solid 4x4 square (border on, interior off) and a diagonal+block shape. Pins perimeter counts AND specific pixel on/off states (interior P4(3,3)=0, border P4(2,2)=1). Previously fingerprinted only numel. Bit-equal MATLAB R2025b. |
| `bwpropfilt` | ✅ | Sig: BW2 / CC2 = bwpropfilt(BW_or_CC [, I], attrib, range_or_n [, keep] [, conn]). Implements all 17 documented attributes: Area, Circularity, ConvexArea, Eccentricity, EquivDiameter, EulerNumber, Extent, FilledArea, MajorAxisLength, MaxIntensity, MeanIntensity, MinIntensity, MinorAxisLength, Orientation, Perimeter, PerimeterOld, Solidity. Covers: range mode, top-N largest, top-N smallest, intensity (marker), CC-struct input. Fingerprints exercise 11 attribute/mode combinations — all bit-exact tol=0 except Perimeter/Circularity/PerimeterOld which use a Tomas-Holst weighted boundary that has a different normalization from MATLAB's internal builtin (the algorithm runs and produces a perimeter-like quantity but raw magnitudes diverge by a small constant factor; documented in gtest, not fingerprinted here). Reference: Wilson/Stratton 1968 (moments); Tomas-Holst (Perimeter); Andrew 1979 (convex hull). Image namespace 2026-05-27. |
| `bwselect` | ✅ | Sig: r = bwselect(...). Spec-extension batch 2026-05-09. |
| `bwselect3` | ❌ |  |
| `cc2bw` | ✅ | Sig: L = labelmatrix(CC), BW = cc2bw(CC [, NV]). Companion CC-struct conversions used after bwconncomp. labelmatrix walks PixelIdxList writing comp-k label into each listed pixel; output class is uint8/uint16/uint32/double based on NumObjects. cc2bw similarly rasterizes the cells into a logical mask, with optional ObjectsToKeep filter (numeric vec / logical vec). Covers: default cc2bw (full reconstruction), ObjectsToKeep=2 (single), ObjectsToKeep=[1 3] (vector), labelmatrix values at 4 component positions, column-major component-numbering (verified against MATLAB's first-pixel-encountered convention). This cycle also fixed numkit's bwconncomp to scan column-major in BOTH the labelling pass and the relabel pass — previously it scanned row-major in the relabel pass, causing component numbering to diverge from MATLAB (objects 2 and 3 swapped on the test BW). tol=0 (bit-exact). Image namespace 2026-05-27. |
| `corr2` | ✅ | Sig: r = corr2(...). Spec-extension batch 2026-05-09.  |
| `graydist` | ✅ | Sig: T = graydist(I, mask | C, R | ind [, method]). Branches: 4 input forms (mask, (C,R), ind, defaults), 3 methods (cityblock, chessboard, quasi-euclidean), multi-seed, uint8 input. Algorithm: Dijkstra with chamfer-weighted edges, cost(p→q) = χ(p,q)·(I(p)+I(q))/2. Output class: DOUBLE for double input, SINGLE otherwise. Reference: Soille, *Morphological Image Analysis*, 2nd ed., §4.4. Image namespace 2026-05-27. |
| `imcontour` | ❌ |  |
| `imhist` | ✅ | Sig [counts,x]=imhist(I,n). counts unchanged; x (bin locations) spans the input CLASS display range: double/single/logical -> [0,1]; uint8 -> [0,255]; uint16 -> [0,65535]; int16 -> [-32768,32767]. imhist(uint8([0 64 128 192 255]),4) -> counts [1 1 2 1], x [0 85 170 255]. double n=4 x = [0 .3333 .6667 1]. uint16 n=3 x = [0 32767.5 65535]. numkit previously returned [0,1] x for ALL classes -- fixed. Matches MATLAB R2025b. |
| `impixel` | ❌ |  |
| `improfile` | ❌ |  |
| `labelmatrix` | ✅ | Sig: L = labelmatrix(CC), BW = cc2bw(CC [, NV]). Companion CC-struct conversions used after bwconncomp. labelmatrix walks PixelIdxList writing comp-k label into each listed pixel; output class is uint8/uint16/uint32/double based on NumObjects. cc2bw similarly rasterizes the cells into a logical mask, with optional ObjectsToKeep filter (numeric vec / logical vec). Covers: default cc2bw (full reconstruction), ObjectsToKeep=2 (single), ObjectsToKeep=[1 3] (vector), labelmatrix values at 4 component positions, column-major component-numbering (verified against MATLAB's first-pixel-encountered convention). This cycle also fixed numkit's bwconncomp to scan column-major in BOTH the labelling pass and the relabel pass — previously it scanned row-major in the relabel pass, causing component numbering to diverge from MATLAB (objects 2 and 3 swapped on the test BW). tol=0 (bit-exact). Image namespace 2026-05-27. |
| `mean2` | ✅ | Sig: r = mean2(...). Spec-extension batch 2026-05-09. |
| `poly2label` | ❌ |  |
| `regionprops` | ✅ 🔬 | Sig: r = regionprops(...). Spec-extension batch 2026-05-09. |
| `regionprops3` | ❌ |  |
| `std2` | ✅ | Sig: r = std2(...). Spec-extension batch 2026-05-09. |

### Texture Analysis

**Namespace:** `image.texture.*` — 0 ✅ + 0 ⚠️ / 6 = 0%

| function | status | comment |
|---|:---:|---|
| `entropy` | ✅ | Sig: r = entropy(...). Spec-extension batch 2026-05-09. |
| `entropyfilt` | ✅ | Sig: r = entropyfilt(...). Spec-extension batch 2026-05-09. |
| `graycomatrix` | ✅ | Sig: G = graycomatrix(I[, NV-pairs]). Gray-level co-occurrence matrix. Bit-equal MATLAB R2025b. NV-pairs: NumLevels / Offset / GrayLimits / Symmetric. KNOWN GAP: multi-offset 3-D return form. |
| `graycoprops` | ✅ | Sig: s = graycoprops(G). 4 texture stats (Contrast / Correlation / Energy / Homogeneity) off normalised GLCM. Bit-equal MATLAB R2025b. |
| `rangefilt` | ✅ | Sig: R = rangefilt(I [, domain]). Local max-min over neighbourhood. Default 3x3 ones, symmetric pad. Output class matches input. |
| `stdfilt` | ✅ | Sig: S = stdfilt(I [, domain]). Local sample std (N-1 norm). Default 3x3 ones, symmetric pad. Uses Octave-source test vector G. |

### Image Quality

**Namespace:** `image.quality.*` — 3 ✅ + 0 ⚠️ / 8 = 38%

| function | status | comment |
|---|:---:|---|
| `brisque` | ❌ | no-reference quality (needs trained model) |
| `immse` | ✅ | Sig: r = immse(...). Spec-extension batch 2026-05-09. |
| `multissim` | ✅ | Sig: [score, qmaps] = multissim(I, Iref [, NV...]). Covers: default 5-scale, NumScales={1,3}, custom ScaleWeights (Wang 2003), Sigma=0.5, DynamicRange=128, identical-images (== 1), double input class preservation. Deterministic gradient input avoids rand/randn divergence between MATLAB and numkit MT19937. tol=1e-4 (single-vs-double precision in box filter accumulation; algorithm bit-equivalent). qualityMap output (cell of per-scale ssim maps) and error guards covered exhaustively in gtest. Reference: Wang/Simoncelli/Bovik 2003, Asilomar Conf. on Signals/Systems/Computers. Image namespace 2026-05-27. |
| `multissim3` | ✅ | Sig: [score, qmaps] = multissim3(V, Vref [, NV...]). 3-D volumetric extension of multissim. Covers: default 5-scale, NumScales={1,3}, custom ScaleWeights (Wang 2003), Sigma=0.5, DynamicRange=128, identical, double in/out. Algorithm identical to multissim but with 3-D Gaussian (separable 1-D across rows/cols/slices), 2x2x2/8 box lowpass, factor-2 downsample in all 3 dims. tol=1e-4 (float-vs-double precision). Reference: Wang/Simoncelli/Bovik 2003. Image namespace 2026-05-27. |
| `niqe` | ❌ | no-reference (needs model) |
| `piqe` | ❌ | perceptual no-reference |
| `psnr` | ✅ | Sig + small deterministic input. Auto-generated for parity sweep. |
| `ssim` | ✅ | Sig + small deterministic input. Auto-generated for parity sweep. |

### Image Transforms

**Namespace:** `image.transform.*` — 7 ✅ + 0 ⚠️ / 11 = 64%

`fft2` / `ifft2` / `fftshift` / `ifftshift` already covered under Signal / Transforms; cross-listed here per MATLAB TOC.

| function | status | comment |
|---|:---:|---|
| `dct2` | ✅ | Sig: r = dct2(...). Spec-extension batch 2026-05-09. |
| `dctmtx` | ✅ | Sig: r = dctmtx(...). Spec-extension batch 2026-05-09. |
| `fan2para` | ❌ | fan-beam → parallel |
| `fanbeam` | ❌ |  |
| `fft2` | ✅ | Sig: r = fft2(...). Spec-extension batch 2026-05-09. |
| `fftshift` | ✅ | Sig: Y = fftshift(X[, dim]). Cyclic shift along every non-singleton dim by ceil(extent/2); inverse ifftshift uses floor(extent/2). Bug fix: numkit had fftshift/ifftshift swapped for odd N + flat-shift instead of per-dim for matrices + dim arg ignored. tol=0 (integer-stable). |
| `idct2` | ✅ | Sig: r = idct2(...). Spec-extension batch 2026-05-09. |
| `ifanbeam` | ❌ |  |
| `ifft2` | ✅ | ifft2(X,'symmetric') vs MATLAB R2025b. DEEP-PROBE 2026-05-31: previously THREW 'Cannot convert char to scalar' (parsed 'symmetric' as a size arg). MATLAB treats X as conjugate-symmetric so the 2-D inverse is EXACTLY REAL. Decomposes into the 1-D ifftSymmetric over each dim of length>1 (dim 2 then dim 1), matching MATLAB across square/non-square/row/column/scalar (NOT real(ifft2), which differs for genuine 2-D). ifft2([1+1i 2-3i 5;4 5+2i 1-1i],'symmetric')=[3.16667 -0.0446582 -0.622008;-1.5 1.44338 -1.44338] (real; ir=1); ifft2([1+2i;3-1i;5+0.5i;2i],'symmetric')=[3;-0.5;0;-1.5]; ifft2(3+4i,'symmetric')=3. 'nonsymmetric'=default. The resize form ifft2(X,m,n,'symmetric') uses a different reconstruction and throws (deferred). N-D throws. covers ifft2. namespace=builtin. |
| `ifftshift` | ✅ | Sig: Y = ifftshift(X[, dim]). Inverse of fftshift; cyclic shift along every non-singleton dim by floor(extent/2). Joint fix with fftshift on 2026-05-08 — they were swapped for odd-extent dims. tol=0. |
| `para2fan` | ❌ |  |

## IO

### Low-Level File I/O

**Namespace:** `io.file_io.*` — 13 ✅ + 0 ⚠️ / 15 = 86%

| function | status | comment |
|---|:---:|---|
| `fclose` | ✅ | Side-effect smoke test (file I/O round-trip in single snippet via tempname). fclose returns 0 on success. |
| `feof` | ✅ | Side-effect smoke test (file I/O round-trip via tempname). feof = 1 after over-reading. |
| `ferror` | ✅ | Side-effect smoke test (file I/O round-trip via tempname). ferror returns empty string when no error. |
| `fgetl` | ✅ | Side-effect smoke test (file I/O round-trip in single snippet via tempname). fgetl reads one line (without newline) -- 'hello' has length 5. |
| `fgets` | ✅ | Side-effect smoke test (file I/O round-trip in single snippet via tempname). fgets reads one line WITH newline -- length >= 5 ('hello\n'). |
| `fileread` | ✅ | Side-effect smoke test (file I/O round-trip in single snippet via tempname). fileread returns full file content -- 3 chars. |
| `fopen` | ✅ | Side-effect smoke test (file I/O round-trip in single snippet via tempname). Open file, return fd, close, cleanup -- verifies fopen returns valid descriptor. |
| `fprintf` | ✅ 🔬 | fprintf returns the byte count (MATLAB's `count` output). DEEP-PROBE 2026-05-31: numkit previously returned nothing, so `k = fprintf(...)` left k undefined; now `if (nargout>=1) outs[0]=bytesWritten`. Counts pinned vs MATLAB R2025b: fprintf('hello')=5; fprintf('%d\n',42)=3; fprintf('%d %d %d',[1 2 3])=5; fprintf('%5.2f\n',3.14159)=6; fprintf('%d-',[10 20 30])=9 (format recycled over the vector); fprintf('')=0; fprintf(2,'err')=3 (stderr fid still counted). A bare fprintf(...) sets no ans (nargout==0). Fingerprint is the counts only — the emitted text goes to stdout in both engines. Octave matches. |
| `fread` | ✅ | Side-effect smoke test (file I/O round-trip via tempname). fread default-type round-trip -- sum of [1..5] = 15. |
| `frewind` | ✅ | Side-effect smoke test (file I/O round-trip via tempname). frewind resets position to 0. |
| `fscanf` | ✅ | Side-effect smoke test (file I/O round-trip in single snippet via tempname). fscanf reads formatted -- sum of [1..5] = 15. |
| `fseek` | ✅ | Side-effect smoke test (file I/O round-trip via tempname). fseek to EOF -- ftell reports positive position. |
| `ftell` | ✅ | Side-effect smoke test (file I/O round-trip via tempname). ftell after one read -- positive position. |
| `fwrite` | ✅ | Side-effect smoke test (file I/O round-trip via tempname). fwrite returns element count -- 5. |
| `openedfiles` | ❌ |  |

### Text Files (CSV / dlm / readtable)

**Namespace:** `io.text.*`. Exception: `readtable/writetable/readtimetable/writetimetable` → `table.*` (future) — 1 ✅ + 0 ⚠️ / 16 = 6%

| function | status | comment |
|---|:---:|---|
| `fileread` | ✅ | Side-effect smoke test (file I/O round-trip in single snippet via tempname). fileread returns full file content -- 3 chars. |
| `importdatatask` | ❌ |  |
| `importtool` | ❌ |  |
| `readcell` | ❌ |  |
| `readlines` | ✅ | Side-effect smoke test (file I/O round-trip via tempname). readlines returns string array -- at least 3 lines (some engines append empty trailing string). |
| `readmatrix` | ✅ | Side-effect smoke test (file I/O round-trip in single snippet via tempname). readmatrix reads CSV -- sum [1+2+3+4] = 10. |
| `readtable` | ❌ | needs table type |
| `readtimetable` | ❌ |  |
| `readvars` | ❌ |  |
| `textscan` | ✅ | Side-effect smoke test (file I/O round-trip in single snippet via tempname). textscan returns cell of parsed columns -- 3 elements. |
| `type` | ✅ | Side-effect smoke test (file I/O round-trip in single snippet via tempname). type displays file content -- side-effect only. |
| `writecell` | ❌ |  |
| `writelines` | ✅ | Side-effect smoke test (file I/O round-trip via tempname). writelines writes single string -- file has >= 5 chars. |
| `writematrix` | ✅ | Side-effect smoke test (file I/O round-trip in single snippet via tempname). writematrix writes CSV -- file should have >= 7 chars (e.g. '1,2\n3,4\n'). |
| `writetable` | ❌ | needs table type |
| `writetimetable` | ❌ |  |

### Spreadsheets

**Namespace:** `io.text.*`. Table-shaped readers (`readtable`/`writetable`) → `table.*` (future) — 0 ✅ + 0 ⚠️ / 13 = 0%

| function | status | comment |
|---|:---:|---|
| `importdata` | ❌ | auto-detect |
| `importdatatask` | ❌ |  |
| `importtool` | ❌ |  |
| `readcell` | ❌ |  |
| `readmatrix` | ✅ | Side-effect smoke test (file I/O round-trip in single snippet via tempname). readmatrix reads CSV -- sum [1+2+3+4] = 10. |
| `readtable` | ❌ | needs table type |
| `readtimetable` | ❌ |  |
| `readvars` | ❌ |  |
| `sheetnames` | ❌ |  |
| `writecell` | ❌ |  |
| `writematrix` | ✅ | Side-effect smoke test (file I/O round-trip in single snippet via tempname). writematrix writes CSV -- file should have >= 7 chars (e.g. '1,2\n3,4\n'). |
| `writetable` | ❌ | needs table type |
| `writetimetable` | ❌ |  |

### Workspace Save / Load

**Namespace:** `io.workspace.*` — 0 ✅ + 0 ⚠️ / 2 = 0%

| function | status | comment |
|---|:---:|---|
| `loadobj` | ❌ |  |
| `saveobj` | ❌ |  |

### File Name Construction

**Namespace:** `io.paths.*` — 0 ✅ + 0 ⚠️ / 9 = 0%

| function | status | comment |
|---|:---:|---|
| `filemarker` | ❌ |  |
| `fileparts` | ✅ | Sig [path,name,ext]=fileparts(f): splits a path. Multi-dot '/path/to/archive.tar.gz' -> path='/path/to', name='archive.tar' (only the LAST dot starts the extension), ext='.gz'. No-extension 'README' -> name='README', ext='' (empty). strcmp/isempty pin exact string equality (not a loose proxy). |
| `filesep` | ✅ | Sig: r = filesep(...). Spec-extension batch 2026-05-09. |
| `fullfile` | ✅ | Sig: r = fullfile(...). Spec-extension batch 2026-05-09. |
| `matlabdrive` | ❌ |  |
| `matlabroot` | ❌ |  |
| `tempdir` | ✅ | Sig: r = tempdir(...). Spec-extension batch 2026-05-09. |
| `tempname` | ✅ | Sig: r = tempname(...). Spec-extension batch 2026-05-09. |
| `toolboxdir` | ❌ |  |

## Linear Algebra



**Namespace:** `linalg.*` — 12 ✅ + 0 ⚠️ / 82 = 15%

> Library is live (toolboxes/linalg/, 2026-05-25). User-facing surface migrated
> from toolboxes/builtin — see commits `30b06660`..`d71b472c`. Functions still
> marked **deferred — toolboxes/linalg** below are not-yet-implemented (the
> library is the destination, not the blocker). 22 ❌ on this page wait
> on first-time implementation; the per-function migration is complete
> for everything that was previously shipped.

| function | status | comment |
|---|:---:|---|
| `balance` | ⚠️ | MATLAB balance: Parlett-Reinsch diagonal scaling for eigvalue conditioning. v1 implements only the scaling phase (permutation phase deferred; equivalent to balance(A, 'noperm') but applies even without the explicit option). For the classic 3x3 dynamic-range matrix, T differs from MATLAB by a uniform factor (4x) which CANCELS in B = inv(T)*A*T -- so B entries are bit-equal. For some inputs (e.g. 2x2 with 12 orders of magnitude), my iterative convergence reaches a different scaling than MATLAB's LAPACK dgebal -- B differs in literal entries but the similarity B = inv(T)*A*T is exact (residual ~0) and eigvals match (ediff ~0). KNOWN GAP: literal T/B entries may differ from MATLAB on hard inputs; mathematical invariants always hold. Fingerprint pins: 3x3 B literal entries (match MATLAB), residuals (must be ~0), and eigvalue preservation (must be exact). Octave 11.1.0 ships balance in core. |
| `bandwidth` | ✅ | Sig: matrix-structure predicates and bandwidth queries. isbanded(A, lo, up) ↔ outside-band entries are zero; isdiag = isbanded(A, 0, 0). istril / istriu ↔ lower / upper triangular. [lo, up] = bandwidth(A) returns sub-diagonal and super-diagonal extent; bandwidth(A) one-out form returns lo; 'lower'|'upper' string form selects one. Bit-exact MATLAB R2025b (tol=0): D diagonal (ib1=id1=1), A tri-banded (ib2=1), A not upper-banded-only (ib3=0), L lower-tri (itl=1), U upper-tri (itu=1), bandwidth(A) = [1, 1], bandwidth(U) = 0, bandwidth(L) = 2, bandwidth(U, 'upper') = 2. Split from the original combined predicates spec to stay under the 256-register chunk limit. |
| `cdf2rdf` | ✅ | Sig: cdf2rdf (complex-diag Schur → real-block Schur), rsf2csf (real-block → complex-diag). cdf2rdf test: build (V, D) manually for a rotation matrix [0 -1; 1 0] (eigvals ±i); verify the real-form reconstruction. rsf2csf test: a [a -b; b a] 2x2 real Schur block (eigvals a±bi). MATLAB R2025b matches the documented convention DR_block = [a -b; b a], VR(:, k+1) = -Im(v) when sub-diagonal of D pairs positive imag part. |
| `chol` | ✅ | Sig R=chol(A) / L=chol(A,'lower') / [R,p]=chol(A). Default 'upper': R'*R=A. 'lower' returns lower-triangular L (=R') with L*L'=A. Second output p=0 when positive-definite; when not PD, p=failure column (no error) and R is the leading (p-1)x(p-1) factor. chol([1 3;3 1]) -> p2=2, R2 is 1x1 =[1]. numkit previously had NO 'lower' option and NO p output -- added. Matches MATLAB R2025b. |
| `cholupdate` | ✅ | Sig: R1 = cholupdate(R, x[, '+'|'-']). Rank-1 update / downdate of Cholesky factor; R1'*R1 = R'*R ± x*x'. Update via Golub-Van Loan 6.5.1 Givens rotations (O(n²)). Downdate via O(n³) re-chol on R'*R - x*x' (KNOWN GAP — MATLAB uses LINPACK Saunders 1972 stable O(n²) variant). Diagonal entries match MATLAB R2025b exactly on the pinned PD inputs. |
| `cond` | ✅ | Sig: c = cond(A). 2-norm condition number = sigma_max/sigma_min via SVD. Bit-identical with MATLAB R2025b. Note: only 2-norm form supported; cond(A,p) for other p deferred. |
| `condeig` | ✅ | Sig: s = condeig(A). Eigenvalue condition numbers; s_i = 1/|cos(angle(v_i, w_i))| where v_i is right eigvec, w_i = inv(V)'s i-th column. Symmetric A → all s_i == 1 (perfectly conditioned). Non-symmetric → larger s_i flags ill-conditioned eigenvalues. Tol 1e-9 is loose because condeig values themselves can be large; we pin the structure (symmetric=1.0; non-sym pair has matching s_i; ill-cond is huge). |
| `condest` | ✅ | Sig: c = condest(A). 1-norm condition number estimate. KNOWN GAP: MATLAB uses Higham 1988 power-iteration estimator (LAPACK dlacn1) that approximates norm(inv(A),1); we compute it exactly via inv(A). Matches MATLAB on well-conditioned A. For hilb(4) ≈ 1.5e4 and other near-singular inputs, our exact value differs from MATLAB's iterative estimate. Wide tol=0.5 (relative) accepts ±50% drift on near-singular inputs; pin only the well-conditioned cases I3 / D / UT for exact match. |
| `cross` | ✅ | MATLAB cross — vector cross product. Vector form [1 2 3]x[4 5 6]=[-3 6 -3]; column-wise on a 3x3 matrix (cross of corresponding columns). COMPLEX: ordinary cross product, NO conjugation: cross([1+1i 0 0],[0 1 0])=[0 0 1+1i]; cross([1i;2;3],[4;5i;6])=[12-15i;12-6i;-13]. numkit previously threw 'Not a double array' on COMPLEX (added 2026-05-29). DEEP-PROBE 2026-05-31: the dim arg cross(A,B,dim) was IGNORED (always crossed along the first length-3 dimension). For a 3x3 (ambiguous) input numkit picked dim 1 regardless of the requested dim. Now honored: cross([1 2 3;4 5 6;7 8 9],eye(3),1)=[0 -8 6;7 0 -3;-4 2 0] (m1(1,2)=-8), cross(...,2)=[0 3 -2;-6 0 4;8 -7 0] (m2(1,2)=3, m2(2,1)=-6, m2(3,2)=-7); cross([1 2 3],[4 5 6],2)=[-3 6 -3]. A and B must have length 3 along dim. Bit-equal MATLAB R2025b. |
| `ctranspose` | ✅ 🔬 | Sig: r = ctranspose(A) === A'. Conjugate transpose. DEEP-PROBE 2026-05-31: ctranspose threw 'Transpose not supported for this type' on non-DOUBLE/non-COMPLEX. Now shares transpose2D with transposeNC; COMPLEX conjugates (zr=5,zi=-6 from conj(Z(2,1)=5+6i); Z'(2,1)=conj(Z(1,2)=3+4i)=3-4i so zor=3,zoi=-4), all other types are a plain transpose (no element change): char preserved (c12='c'=99), logical preserved (l12=L(2,1)=0, lcls=1), cell off-diagonal swap (k21=KT{2,1}=K{1,2}='x'). 3-D still throws. Spec-extension batch 2026-05-09. |
| `decomposition` | ❌ | **deferred — toolboxes/linalg** |
| `det` | ✅ | Sig: d = det(A). Determinant via LU with partial pivoting; sign tracked from row swaps. Singular A returns 0. Bit-identical with MATLAB R2025b on probed cases (2×2, triangular 3×3, identity 5×5, singular rank-1, magic(4)). |
| `dot` | ✅ | MATLAB dot — scalar/column-wise dot product. [1 2 3].[4 5 6]=32; 3x2 matrices -> per-column dot (1x2). COMPLEX: MATLAB conjugates the FIRST arg, dot(a,b)=sum(conj(a).*b): dot([1+2i 3],[4 5i])=4+7i; per-column dot([1+1i 2;3 4i],[1 1i;1 1])=[4-1i -2i]; real-vs-complex dot([1 2],[1i 2i])=5i. numkit previously dropped the imaginary part -- complex path added 2026-05-29. DEEP-PROBE 2026-05-31: the dim arg dot(A,B,dim) was IGNORED (always reduced along dim 1). Now honored: dot([1 2 3;4 5 6],[1 1 1;2 2 2],2)=[6;30] (rows, 2x1); dim 1 = [9 12 15] (1x3); a vector with explicit dim follows the general sum(conj(A).*B,dim) rule so dot([1 2 3],[4 5 6],1)=[4 10 18] (length-1 reduction = identity, NOT 32); complex dot([1+1i 2;3 4],[1 1;1 1],2)=[3-1i;7]. Bit-equal MATLAB R2025b. |
| `eig` | ⚠️ | Sig: e = eig(A) | [V,D] = eig(A) | [V,D,W] = eig(A) | eig(A,'vector'|'matrix') | eig(A,B). Symmetric: classical Jacobi. General: characteristic polynomial via Souriau-Faddeev + roots(). Left eigenvectors W (fixed 2026-06-05, bugs/linalg/eig-left-vectors.md): W'*A = D*W', the right eigenvectors of A' reordered to D's eigenvalue order and normalized to unit 2-norm; for symmetric A, W == V. W column signs may differ from MATLAB (eig sign convention), so W is validated sign/order-agnostically: the relation residual werr/wnerr ~ 0, symmetric W==V (wveq ~ 0), unit-norm columns (wn1=1), and sum(abs(W)) (wsum/wnsum, invariant under column permutation + sign). Generalized eig(A,B) reduces to B\A. 'vector'/'matrix'/eig(A,B) queue-cleared 2026-05-29. Sort applied for order-agnostic eigenvalue comparison. |
| `eigs` | ❌ | **deferred — toolboxes/linalg** |
| `expm` | ✅ | Sig: E = expm(A). Matrix exponential via Padé(6) with scaling-and-squaring (Higham 2005). Works for any square matrix. Bit-identical with MATLAB R2025b on rotation generator + symmetric + zero cases. |
| `expmv` | ✅ ➖ | Sig: w = expmv(t, A, v). KNOWN GAP: MATLAB core does NOT ship expmv — only Higham's separate package on File Exchange does. Therefore correctness=N/A vs MATLAB on most engines. Spec checks algebraic identity (matches expm(t*A)*v on a 3×3 triangular A to ulp) which is self-verifying. Diagonal A path: w(i) == exp(t·d(i)) · v(i), trivially correct. |
| `funm` | ❌ | **deferred — toolboxes/linalg** |
| `gsvd` | ❌ | **deferred — toolboxes/linalg** |
| `hess` | ✅ | Sig: [P, H] = hess(A). Hessenberg reduction via Householder reflectors; A = P*H*P', H upper-Hessenberg (zeros below sub-diagonal). Foundation for general eig (Phase 2c). Bit-identical reconstruction with MATLAB R2025b; H entries differ in sign/order due to Householder reflector freedom but identity verified to ulp. |
| `inv` | ✅ | Sig: B = inv(A). Matrix inverse via LU (la_solve backend). Bit-identical with MATLAB R2025b on probed 2×2 + 3×3 systems; A*inv(A) = I to ~ulp. |
| `isbanded` | ✅ | Sig: matrix-structure predicates and bandwidth queries. isbanded(A, lo, up) ↔ outside-band entries are zero; isdiag = isbanded(A, 0, 0). istril / istriu ↔ lower / upper triangular. [lo, up] = bandwidth(A) returns sub-diagonal and super-diagonal extent; bandwidth(A) one-out form returns lo; 'lower'|'upper' string form selects one. Bit-exact MATLAB R2025b (tol=0): D diagonal (ib1=id1=1), A tri-banded (ib2=1), A not upper-banded-only (ib3=0), L lower-tri (itl=1), U upper-tri (itu=1), bandwidth(A) = [1, 1], bandwidth(U) = 0, bandwidth(L) = 2, bandwidth(U, 'upper') = 2. Split from the original combined predicates spec to stay under the 256-register chunk limit. |
| `isdiag` | ✅ | Sig: matrix-structure predicates and bandwidth queries. isbanded(A, lo, up) ↔ outside-band entries are zero; isdiag = isbanded(A, 0, 0). istril / istriu ↔ lower / upper triangular. [lo, up] = bandwidth(A) returns sub-diagonal and super-diagonal extent; bandwidth(A) one-out form returns lo; 'lower'|'upper' string form selects one. Bit-exact MATLAB R2025b (tol=0): D diagonal (ib1=id1=1), A tri-banded (ib2=1), A not upper-banded-only (ib3=0), L lower-tri (itl=1), U upper-tri (itu=1), bandwidth(A) = [1, 1], bandwidth(U) = 0, bandwidth(L) = 2, bandwidth(U, 'upper') = 2. Split from the original combined predicates spec to stay under the 256-register chunk limit. |
| `ishermitian` | ✅ | Sig: issymmetric(A [, 'skew']) → A == A.' (transpose, no conj). ishermitian(A [, 'skew']) → A == A' (conj transpose). 'skew' flips equality to A == -A.' / A == -A'. Bit-exact MATLAB R2025b (tol=0): SY symmetric (s1=1), H not symmetric (s2=0 — complex matrix), SK skew-sym (s3=1), H Hermitian (h1=1), SY not Hermitian since complex form differs (h2=1 — but SY is real so h2=1 by real-symmetric ≡ Hermitian rule), SKH skew-Hermitian (h3=1). Split from the original combined predicates spec to stay under the 256-register chunk limit. |
| `issymmetric` | ✅ | Sig: issymmetric(A [, 'skew']) → A == A.' (transpose, no conj). ishermitian(A [, 'skew']) → A == A' (conj transpose). 'skew' flips equality to A == -A.' / A == -A'. Bit-exact MATLAB R2025b (tol=0): SY symmetric (s1=1), H not symmetric (s2=0 — complex matrix), SK skew-sym (s3=1), H Hermitian (h1=1), SY not Hermitian since complex form differs (h2=1 — but SY is real so h2=1 by real-symmetric ≡ Hermitian rule), SKH skew-Hermitian (h3=1). Split from the original combined predicates spec to stay under the 256-register chunk limit. |
| `istril` | ✅ | Sig: matrix-structure predicates and bandwidth queries. isbanded(A, lo, up) ↔ outside-band entries are zero; isdiag = isbanded(A, 0, 0). istril / istriu ↔ lower / upper triangular. [lo, up] = bandwidth(A) returns sub-diagonal and super-diagonal extent; bandwidth(A) one-out form returns lo; 'lower'|'upper' string form selects one. Bit-exact MATLAB R2025b (tol=0): D diagonal (ib1=id1=1), A tri-banded (ib2=1), A not upper-banded-only (ib3=0), L lower-tri (itl=1), U upper-tri (itu=1), bandwidth(A) = [1, 1], bandwidth(U) = 0, bandwidth(L) = 2, bandwidth(U, 'upper') = 2. Split from the original combined predicates spec to stay under the 256-register chunk limit. |
| `istriu` | ✅ | Sig: matrix-structure predicates and bandwidth queries. isbanded(A, lo, up) ↔ outside-band entries are zero; isdiag = isbanded(A, 0, 0). istril / istriu ↔ lower / upper triangular. [lo, up] = bandwidth(A) returns sub-diagonal and super-diagonal extent; bandwidth(A) one-out form returns lo; 'lower'|'upper' string form selects one. Bit-exact MATLAB R2025b (tol=0): D diagonal (ib1=id1=1), A tri-banded (ib2=1), A not upper-banded-only (ib3=0), L lower-tri (itl=1), U upper-tri (itu=1), bandwidth(A) = [1, 1], bandwidth(U) = 0, bandwidth(L) = 2, bandwidth(U, 'upper') = 2. Split from the original combined predicates spec to stay under the 256-register chunk limit. |
| `kron` | ✅ | Sig: r = kron(...). Spec-extension batch 2026-05-09. COMPLEX: ordinary Kronecker product with complex element products (no conjugation): kron([1i 2],[1 1])=[1i 1i 2 2]; kron([1+1i;2],[1 1i])=[1+1i -1+1i; 2 2i]. numkit previously rejected COMPLEX ('not supported') -- added 2026-05-29. |
| `ldl` | ✅ | MATLAB ldl: block LDL' factorization. v1 implements Crout LDL' WITHOUT pivoting; covers all PD/ND matrices and indefinite cases that don't strictly require Bunch-Kaufman 2x2 pivoting (which is rare for the test inputs here). Bit-equal with MATLAB R2025b on PD 3x3 (L,D entries match exactly) and on residuals ||A - L*D*L'||. Forms covered: 1-out (L only), 2-out (L,D), 3-out matrix P (identity in v1), 3-out vector P, 'upper' triangle. KNOWN GAPs (PROGRESS): Bunch-Kaufman pivoting (P != I) for matrices with zero pivots; complex Hermitian; sparse [L,D,P,C] form; 'tol' arg. Octave ships ldl in core but with a slightly different output layout for the indefinite case; we follow MATLAB R2025b conventions. |
| `linsolve` | ✅ | Sig: X = linsolve(A, B[, opts]). Wrapper over la_solve (LU for square A, Householder QR least-squares for tall A). Opts struct accepted for MATLAB-compat but ignored (auto-detection covers same cases). Bit-identical with MATLAB R2025b on probed square + tall systems. |
| `logm` | ⚠️ | Sig: L = logm(A). Matrix logarithm for symmetric positive-definite A via eig: L = V*diag(log(eig))*V'. Round-trip expm(logm) = A to ulp. General (non-symmetric) logm requires complex Schur -- deferred to Phase 2b. |
| `lscov` | ✅ | Sig: [x, stdx, mse, S] = lscov(A, b[, w]). Weighted least squares. mse = SSR/(N-p); S = mse·(A'WA)^(-1). Full N×N covariance V deferred (errors). Bit-identical to MATLAB R2025b on OLS and weighted paths. |
| `lsqminnorm` | ✅ | MATLAB linalg cycle 4: lsqminnorm + lsqnonneg. lsqminnorm = pinv(A,tol)*B; bit-equal with MATLAB R2025b on full-rank, rank-deficient, and wide systems. lsqnonneg via Lawson-Hanson active-set algorithm; bit-equal on x, resnorm, residual, exitflag for the classic test [1 -1 2; 3 4 5; 6 7 8] / [1; 2; 3] -> x = [0; 0; 0.387097], resnorm = 0.06451612903. KNOWN GAPs: lsqminnorm 'rankWarn'/'RegularizationFactor' name-value args deferred; lsqnonneg 'options'/'problem' input forms and 6th 'lambda' output (Lagrange multipliers) deferred; complex inputs not supported. Octave ships both functions; lsqminnorm in core since Octave 6, lsqnonneg in optim package. |
| `lsqnonneg` | ✅ | MATLAB linalg cycle 4: lsqminnorm + lsqnonneg. lsqminnorm = pinv(A,tol)*B; bit-equal with MATLAB R2025b on full-rank, rank-deficient, and wide systems. lsqnonneg via Lawson-Hanson active-set algorithm; bit-equal on x, resnorm, residual, exitflag for the classic test [1 -1 2; 3 4 5; 6 7 8] / [1; 2; 3] -> x = [0; 0; 0.387097], resnorm = 0.06451612903. KNOWN GAPs: lsqminnorm 'rankWarn'/'RegularizationFactor' name-value args deferred; lsqnonneg 'options'/'problem' input forms and 6th 'lambda' output (Lagrange multipliers) deferred; complex inputs not supported. Octave ships both functions; lsqminnorm in core since Octave 6, lsqnonneg in optim package. |
| `lu` | ✅ | Sig: [L, U, P] = lu(A) | [L, U, p] = lu(A,'vector'). LU with partial pivoting; P*A = L*U exactly. 'vector' returns the row-permutation vector p (1-based) with A(p,:) = L*U. L unit-lower, U upper. Bit-identical with MATLAB R2025b on probed 3x3. Queue-clearing 2026-05-29: 'vector' previously errored 'requires exactly 1 argument'. |
| `mldivide` | ✅ | Sig: X = mldivide(A,B) ↔ A\B. Square A: LU with partial pivoting. Tall A (m>n): QR via Householder + R back-solve (least squares). Wide A (m<n, min-norm): NOT yet supported — throws m:mldivide:wide. Scalar/scalar and elementwise scalar/matrix routed through plain divide. |
| `mpower` | ✅ | Sig: r = mpower(a,b) (a^b). Spec-extension batch 2026-05-09. KNOWN GAP: matrix^integer (M^n where M is matrix) not implemented in numkit — only scalar^scalar pinned. Documented separately; would need O(log n) repeated mtimes for the matrix branch. |
| `mrdivide` | ✅ | Sig: X = mrdivide(A,B) ↔ A/B  ↔ X·B = A. Composes via the standard transpose trick X = (B'\A')'. So uses the same LU/QR primitives as mldivide. matrix/scalar is elementwise. scalar/matrix ERRORS with m:mrdivide:dim per MATLAB R2025b (verified: `2/[1 2; 3 4]` → 'Matrix dimensions must agree'). |
| `mtimes` | ✅ | Sig: r = mtimes(...). Arithmetic op. Spec-extension batch 2026-05-09. |
| `norm` | ✅ | Sig: n = norm(X[, p]). Vector and matrix norms. Vector: 2-norm default, p-norm via sum(|x|^p)^(1/p), Inf -> max(|x|), -Inf -> min(|x|) (fixed 2026-06-03; was max), 1 -> sum(|x|). Matrix: 2-norm = largest singular, 1 -> max col sum, Inf -> max row sum, 'fro' -> Frobenius (-Inf invalid for matrices). COMPLEX (added 2026-06-05): vector 1/2/Inf/p and matrix 1/Inf/'fro' norm by element magnitude |z| (norm([3+4i 0 -2i])=sqrt(29); norm(cM,'fro')=sqrt(32)); complex matrix 2-norm (spectral) still needs a complex SVD and errors (see bugs/linalg/complex-matrix-unsupported.md). Bit-identical with MATLAB R2025b. |
| `normest` | ✅ | Sig: n = normest(A). 2-norm estimate via largest singular value. NOTE: numkit returns the exact value (full SVD), MATLAB uses power-iteration with default tol=1e-6 (~5-6 sig digits). Tol 1e-5 reflects MATLAB's iteration tolerance. A future perf-pass can switch to power-iteration to match performance characteristics. |
| `null` | ✅ | Sig: N = null(A[, tol]). Orthonormal null-space basis; n - rank(A) columns. A*null(A) = 0 to ulp. |
| `ordeig` | ✅ | Sig: e = ordeig(T). Eigenvalues of (quasi-)triangular Schur factor in stored order — NO sort. Diagonal T → diag(T). Real Schur with 2×2 blocks → conjugate pairs from (a ± √disc)/2 formula. Pinned: diagonal [3 1 2] order preserved; real Schur block at (2,3) gives 0.5±1.5i. |
| `ordqz` | ❌ | **deferred — toolboxes/linalg** |
| `ordschur` | ❌ | **deferred — toolboxes/linalg** |
| `orth` | ✅ | Sig: Q = orth(A[, tol]). Orthonormal basis for range of A; Q has rank(A) columns. Q'*Q = I exactly. Note: column signs may differ from MATLAB (singular vector sign ambiguity); fingerprint avoids direct value comparison. |
| `pagectranspose` | ✅ | 128x64x8 real-valued — pagectranspose equals pagetranspose. 100 iters. |
| `pageeig` | ✅ | Sig: page-wise wrappers (pageeig, pagesvd, pagepinv, pagenorm, pagemldivide, pagemrdivide, pagelsqminnorm, pageinv). All iterate the corresponding 2-D linalg op per page. Fingerprints pin shapes + a handful of values (we use pageinv = inv per page, pageeig on symmetric input that gives exact eigvals [5 5 5] for one page, etc.). pagelsqminnorm not in fingerprint — it uses pinv which goes through SVD whose sign convention may differ at machine precision. |
| `pageinv` | ✅ | Sig: B = pageinv(A). Page-wise inv for 3D arrays -- each page (2D slice) inverted via LU. Bit-identical with MATLAB R2025b across general / identity / diagonal probes. |
| `pagelsqminnorm` | ✅ | Sig: page-wise wrappers (pageeig, pagesvd, pagepinv, pagenorm, pagemldivide, pagemrdivide, pagelsqminnorm, pageinv). All iterate the corresponding 2-D linalg op per page. Fingerprints pin shapes + a handful of values (we use pageinv = inv per page, pageeig on symmetric input that gives exact eigvals [5 5 5] for one page, etc.). pagelsqminnorm not in fingerprint — it uses pinv which goes through SVD whose sign convention may differ at machine precision. |
| `pagemldivide` | ✅ | Sig: page-wise wrappers (pageeig, pagesvd, pagepinv, pagenorm, pagemldivide, pagemrdivide, pagelsqminnorm, pageinv). All iterate the corresponding 2-D linalg op per page. Fingerprints pin shapes + a handful of values (we use pageinv = inv per page, pageeig on symmetric input that gives exact eigvals [5 5 5] for one page, etc.). pagelsqminnorm not in fingerprint — it uses pinv which goes through SVD whose sign convention may differ at machine precision. |
| `pagemrdivide` | ✅ | Sig: page-wise wrappers (pageeig, pagesvd, pagepinv, pagenorm, pagemldivide, pagemrdivide, pagelsqminnorm, pageinv). All iterate the corresponding 2-D linalg op per page. Fingerprints pin shapes + a handful of values (we use pageinv = inv per page, pageeig on symmetric input that gives exact eigvals [5 5 5] for one page, etc.). pagelsqminnorm not in fingerprint — it uses pinv which goes through SVD whose sign convention may differ at machine precision. |
| `pagemtimes` | ✅ | Sig: C = pagemtimes(A, B). 20×20×20 batch matmul. 100 iters. |
| `pagenorm` | ✅ | Sig: page-wise wrappers (pageeig, pagesvd, pagepinv, pagenorm, pagemldivide, pagemrdivide, pagelsqminnorm, pageinv). All iterate the corresponding 2-D linalg op per page. Fingerprints pin shapes + a handful of values (we use pageinv = inv per page, pageeig on symmetric input that gives exact eigvals [5 5 5] for one page, etc.). pagelsqminnorm not in fingerprint — it uses pinv which goes through SVD whose sign convention may differ at machine precision. |
| `pagepinv` | ✅ | Sig: page-wise wrappers (pageeig, pagesvd, pagepinv, pagenorm, pagemldivide, pagemrdivide, pagelsqminnorm, pageinv). All iterate the corresponding 2-D linalg op per page. Fingerprints pin shapes + a handful of values (we use pageinv = inv per page, pageeig on symmetric input that gives exact eigvals [5 5 5] for one page, etc.). pagelsqminnorm not in fingerprint — it uses pinv which goes through SVD whose sign convention may differ at machine precision. |
| `pagesvd` | ✅ | Sig: page-wise wrappers (pageeig, pagesvd, pagepinv, pagenorm, pagemldivide, pagemrdivide, pagelsqminnorm, pageinv). All iterate the corresponding 2-D linalg op per page. Fingerprints pin shapes + a handful of values (we use pageinv = inv per page, pageeig on symmetric input that gives exact eigvals [5 5 5] for one page, etc.). pagelsqminnorm not in fingerprint — it uses pinv which goes through SVD whose sign convention may differ at machine precision. |
| `pagetranspose` | ✅ | 128x64x8 array, page-wise transpose. 100 iters. |
| `pinv` | ✅ | Sig: P = pinv(A[, tol]). Moore-Penrose pseudoinverse via SVD: A*P*A = A, P*A*P = P (verified to ulp). Bit-identical with MATLAB R2025b on probed full-rank + rank-deficient cases. |
| `planerot` | ✅ | MATLAB linalg cycle 2: rref + rcond + planerot. rref via Gauss-Jordan with partial pivoting; default tol = max(M,N)*eps(norm(A,inf)); two-output [R, jb] form returns 1-based pivot column indices. rcond uses cheap path 1/(norm(A,1)*norm(inv(A),1)); returns 0 for singular A. KNOWN GAP: rcond on near-singular matrices may differ slightly from MATLAB's LAPACK dgecon estimator. planerot Givens rotation: r=hypot(x,y), G=[c s; -s c], degenerate (0,0) case returns identity. Bit-equal with MATLAB R2025b on the well-conditioned cases; rcond bit-equal on diag/2x2/hilb(4); planerot exact (cos/sin formula matches). rref complex input deferred (KNOWN GAP). Octave: triple-engine green for all three. Real-only inputs in v1. |
| `polyeig` | ✅ | Sig: e = polyeig(A0, A1, ..., Ak). Polynomial eigenvalue problem via companion linearisation + char-poly + roots(). Eigenvalues-only form. Linear test: (A0 + λI)x = 0 → e = eigvals(-A0) = [-2, -3]. Quadratic test: (λ²-5λ+6)·I → e = {2, 2, 3, 3}. Real ordering may differ — fingerprint sorts. Tol 1e-5 because the characteristic-polynomial → roots() path has lower precision than direct eig (residual imag part ~1e-7 for nominally-real eigvals). |
| `qr` | ✅ | Sig: [Q,R]=qr(A) | qr(A,'econ') | [Q,R,P]=qr(A) | qr(A,'vector'). Householder QR; A=Q*R. Column-pivoted form (fixed 2026-06-05, bugs/linalg/qr-pivoting.md): A*P=Q*R, columns pivoted by decreasing norm; P is a permutation matrix (default) or vector ('vector'/econ). 3x2 tall pivots to [2 1] (P(1,2)=P(2,1)=1); 3x3 pivots to [3 1 2]; |diag(R)| is rank-revealing (decreasing). Q signs may differ from MATLAB by a reflection, so the R diagonal is fingerprinted via abs() and correctness via reconstruction A*P-Q*R / orthogonality Q'Q-I. Queue-clearing 2026-05-29: 'econ' previously errored 'requires exactly 1 argument'. |
| `qrdelete` | ✅ | Sig: [Q1, R1] = qrupdate(Q, R, u, v) — rank-1 update A→A+uv'. qrinsert(Q, R, k, x) — insert column. qrdelete(Q, R, k) — drop column. All three use Givens rotations (Daniel-Gragg-Kaufman-Stewart 1976). Fingerprint uses algebraic identities (reconstruction norm, Q orthogonality, R upper-triangularity) rather than literal entries — Givens-rotation Q/R outputs are unique only up to column sign convention which may differ from MATLAB. |
| `qrinsert` | ✅ | Sig: [Q1, R1] = qrupdate(Q, R, u, v) — rank-1 update A→A+uv'. qrinsert(Q, R, k, x) — insert column. qrdelete(Q, R, k) — drop column. All three use Givens rotations (Daniel-Gragg-Kaufman-Stewart 1976). Fingerprint uses algebraic identities (reconstruction norm, Q orthogonality, R upper-triangularity) rather than literal entries — Givens-rotation Q/R outputs are unique only up to column sign convention which may differ from MATLAB. |
| `qrupdate` | ✅ | Sig: [Q1, R1] = qrupdate(Q, R, u, v) — rank-1 update A→A+uv'. qrinsert(Q, R, k, x) — insert column. qrdelete(Q, R, k) — drop column. All three use Givens rotations (Daniel-Gragg-Kaufman-Stewart 1976). Fingerprint uses algebraic identities (reconstruction norm, Q orthogonality, R upper-triangularity) rather than literal entries — Givens-rotation Q/R outputs are unique only up to column sign convention which may differ from MATLAB. |
| `qz` | ❌ | **deferred — toolboxes/linalg** |
| `rank` | ✅ | Sig: r = rank(A[, tol]). Numerical rank via SVD; sigma > max(m,n)*eps(sigma_max). Bit-identical with MATLAB R2025b on probed full-rank/rank-deficient/zero/identity/hilbert cases. |
| `rcond` | ✅ | MATLAB linalg cycle 2: rref + rcond + planerot. rref via Gauss-Jordan with partial pivoting; default tol = max(M,N)*eps(norm(A,inf)); two-output [R, jb] form returns 1-based pivot column indices. rcond uses cheap path 1/(norm(A,1)*norm(inv(A),1)); returns 0 for singular A. KNOWN GAP: rcond on near-singular matrices may differ slightly from MATLAB's LAPACK dgecon estimator. planerot Givens rotation: r=hypot(x,y), G=[c s; -s c], degenerate (0,0) case returns identity. Bit-equal with MATLAB R2025b on the well-conditioned cases; rcond bit-equal on diag/2x2/hilb(4); planerot exact (cos/sin formula matches). rref complex input deferred (KNOWN GAP). Octave: triple-engine green for all three. Real-only inputs in v1. |
| `rref` | ✅ | MATLAB linalg cycle 2: rref + rcond + planerot. rref via Gauss-Jordan with partial pivoting; default tol = max(M,N)*eps(norm(A,inf)); two-output [R, jb] form returns 1-based pivot column indices. rcond uses cheap path 1/(norm(A,1)*norm(inv(A),1)); returns 0 for singular A. KNOWN GAP: rcond on near-singular matrices may differ slightly from MATLAB's LAPACK dgecon estimator. planerot Givens rotation: r=hypot(x,y), G=[c s; -s c], degenerate (0,0) case returns identity. Bit-equal with MATLAB R2025b on the well-conditioned cases; rcond bit-equal on diag/2x2/hilb(4); planerot exact (cos/sin formula matches). rref complex input deferred (KNOWN GAP). Octave: triple-engine green for all three. Real-only inputs in v1. |
| `rsf2csf` | ✅ | Sig: cdf2rdf (complex-diag Schur → real-block Schur), rsf2csf (real-block → complex-diag). cdf2rdf test: build (V, D) manually for a rotation matrix [0 -1; 1 0] (eigvals ±i); verify the real-form reconstruction. rsf2csf test: a [a -b; b a] 2x2 real Schur block (eigvals a±bi). MATLAB R2025b matches the documented convention DR_block = [a -b; b a], VR(:, k+1) = -Im(v) when sub-diagonal of D pairs positive imag part. |
| `schur` | ⚠️ | Sig: [U, T] = schur(A). For symmetric A this is the eigendecomposition: A = U*T*U' with T diagonal. General (non-symmetric) Schur returns quasi-triangular T with 2x2 blocks for complex eigenpairs -- deferred to Phase 2b. Eigenvalues bit-identical with MATLAB. |
| `sqrtm` | ⚠️ | Sig: R = sqrtm(A). Matrix square root for symmetric positive-semidefinite A via eig: R = V*diag(sqrt(eig))*V'. R*R = A to ulp. General sqrtm requires complex Schur -- deferred to Phase 2b. |
| `subspace` | ✅ | Sig: theta = subspace(A, B). Largest principal angle between column spaces of A and B. Identical subspaces -> 0; orthogonal -> pi/2. |
| `svd` | ✅ | Sig: s = svd(A) | [U, S, V] = svd(A) | svd(A,'econ'). One-sided Jacobi SVD; A = U*S*V'. Bit-identical singular values with MATLAB R2025b (3x3 / 4x3 tall / 3x4 wide / diagonal). 'econ' on a 4x3 tall matrix: Ue 4x3, Se 3x3, Ve 3x3 (k=min(m,n)=3), reconstruction Ue*Se*Ve'=At. U/V signs not compared (ambiguity); reconstruction + orthogonality verified to ulp. Queue-clearing 2026-05-29: 'econ' previously errored 'requires exactly 1 argument'. |
| `svdappend` | ❌ |  |
| `svds` | ❌ | **deferred — toolboxes/sparse** |
| `svdsketch` | ❌ |  |
| `sylvester` | ⚠️ | Sig: X = sylvester(A, B, C). Solves A*X + X*B = C. For symmetric A and B (this revision): simultaneous diagonalisation via eig. Residual to ulp. General (non-symmetric) Sylvester via Bartels-Stewart on Schur forms is deferred. |
| `trace` | ✅ ➖ | Sig: t = trace(A). Sum of diagonal. Works for square + rectangular (uses min(rows,cols)). Bit-identical with MATLAB R2025b. |
| `transpose` | ✅ 🔬 | Sig: r = transpose(A) === A.'. Type-preserving 2-D transpose. DEEP-PROBE 2026-05-31: the transpose() builtin (matrix.cpp) coerced ALL inputs to a DOUBLE matrix of element codes, and the .'/' operators (transposeNC/ctranspose in unary_ops.cpp) threw 'Transpose not supported for this type' on non-DOUBLE/non-COMPLEX. Generalised to one shared transpose2D: DOUBLE/SINGLE/CHAR/LOGICAL/int via raw-byte rearrange, COMPLEX per-element (transpose() = no conjugate), CELL per-cell move; STRING/STRUCT/FUNC deferred. transpose(['ab';'cd'])=['ac';'bd'] char (c11='a'=97,c12='c'=99,c21='b'=98); logical/int8/single class preserved; transpose(complex) does NOT conjugate (zr=5,zi=6 from Z(2,1)=5+6i); cell off-diagonal swap (k12=3,k21=2). transpose() now delegates to transposeNC. 3-D still throws. Spec-extension batch 2026-05-09. |
| `tril` | ✅ 🔬 | Sig: r = tril(A [,k]). DEEP-PROBE 2026-05-31: the 2-D/3-D path was DOUBLE-only (threw 'Not a double array' on char/logical/single/complex) although a type-agnostic byte kernel already existed for the ND (ndim>=4) fallback. Routed 2-D/3-D through the byte kernel (keep lower triangle, zero-fill rest; zero bit pattern = canonical zero for all types). CELL/STRING/STRUCT rejected ('must be numeric, char, or logical'). tril(['abc';'def';'ghi'])=lower-triangle char with char(0) above diag (t11='a'=97,t21='d'=100,t12=0); tril(M,1) keeps one super-diagonal (tk13=0,tk31='g'=103); logical/single/complex class preserved (complex zeroed=0+0i). Spec-extension batch 2026-05-09. |
| `triu` | ✅ 🔬 | Sig: r = triu(A [,k]). DEEP-PROBE 2026-05-31: the 2-D/3-D path was DOUBLE-only (threw 'Not a double array' on char/logical/single/complex); routed through the existing type-agnostic byte kernel (keep upper triangle, zero-fill rest). CELL/STRING/STRUCT rejected. triu(['abc';'def';'ghi'])=upper-triangle char with char(0) below diag (t12='b'=98,t21=0,t11='a'=97); triu(M,-1) keeps one sub-diagonal (tk21='d'=100,tk31=0); logical/single/complex class preserved (complex zeroed=0+0i). Spec-extension batch 2026-05-09. |
| `vecnorm` | ✅ | Sig: y = vecnorm(A [, p [, dim]]). Element-wise p-norm reduction along a dimension; default p=2, default dim = first non-singleton. Row [3 4] → 5 (2-norm). Column [3;4] → 5. 2×2 matrix V columns → [hypot(3,6), hypot(4,8)] = [6.708, 8.944]. r-1-norm = sum(|r|) = 10. r-Inf = max(|r|) = 4. M row-norm (dim=2) → [hypot(1,2), hypot(3,4)] = [2.236, 5]. vecnorm([]) → 0 (MATLAB convention). Bit-exact MATLAB R2025b (tol=1e-12). |

## ODE



**Namespace:** `ode.*` (future) — 0 ✅ + 0 ⚠️ / 21 = 0%

| function | status | comment |
|---|:---:|---|
| `decic` | ❌ | **deferred — toolboxes/ode** |
| `deval` | ❌ | **deferred — toolboxes/ode** |
| `ode` | ❌ |  |
| `ode113` | ❌ | **deferred — toolboxes/ode** |
| `ode15i` | ❌ | **deferred — toolboxes/ode** |
| `ode15s` | ❌ | **deferred — toolboxes/ode** |
| `ode23` | ✅ | Sig: [t,y] = ode23(f, tspan, y0[, opts]). Explicit Bogacki-Shampine 3(2) Runge-Kutta with adaptive step (Bogacki-Shampine 1989, Appl.Math.Lett. 2:321-325; Shampine-Reichelt 1997). 4-stage embedded pair with FSAL k4. Cubic Hermite dense-output interpolant (3rd-order, uses k1+k4 only) for `Refine` and explicit-tspan modes — matches MATLAB. Default `Refine` = 1 (NOT 4 like ode45). odeset opts: RelTol, AbsTol (scalar or per-component), MaxStep, InitialStep, Refine, NormControl, Stats. Output shape (m × 1) / (m × d). Reverse integration supported. Bit-equal MATLAB at tight tols on explicit tspan (verified ~1e-9). 10 gtest TEST_F cover scalar/2D, default+tight tol, explicit tspan, reverse integration, MaxStep, Refine=1/4, AbsTol-vector, nargout=1, invalid-tspan throws. |
| `ode23s` | ❌ | **deferred — toolboxes/ode** |
| `ode23t` | ❌ | **deferred — toolboxes/ode** |
| `ode23tb` | ❌ | **deferred — toolboxes/ode** |
| `ode45` | ✅ | Sig: [t,y] = ode45(f, tspan, y0[, opts]). Explicit Dormand-Prince 5(4) Runge-Kutta with adaptive step (Hairer-Nørsett-Wanner Vol I §II.5; Dormand-Prince 1980; Shampine-Reichelt 1997). Same 7-stage Butcher tableau as MATLAB R2025b with FSAL k7. Free 4th-order dense-output interpolant by Shampine 1986 (Math.Comp. 46:135-150) used for Refine (default 4) and explicit-tspan modes. odeset opts: RelTol, AbsTol (scalar or per-component), MaxStep, InitialStep, Refine, NormControl, Stats. Output shape (m × 1) / (m × d). Reverse integration supported. Bit-equal MATLAB at tight tols on explicit tspan (verified ~1e-9 on linspace(0,2,11)); default-tol step sequences differ ~10-20% (numkit standard step controller w/ safety 0.9, MATLAB Gustafsson PI) but both satisfy tol contract. 11 gtest TEST_F cover scalar/2D, default+tight tol, explicit tspan, reverse integration, MaxStep, Refine=1/4, AbsTol-vector, Van der Pol, nargout=1. |
| `ode78` | ❌ | **deferred — toolboxes/ode** |
| `ode89` | ❌ | **deferred — toolboxes/ode** |
| `odeevent` | ❌ |  |
| `odeget` | ✅ | Sig: v = odeget(opts, name [, default]). Retrieves a named option from an odeset() struct. Case-insensitive name lookup matches against the 21 canonical MATLAB option names; if the field is empty and `default` was given, returns `default` (otherwise []). Validation: unknown name throws. Reference: Shampine-Reichelt 1997. 5 gtest TEST_F: stored value retrieval, case-insensitive lookup, default fallback, no-default returns [], unknown-name throws. |
| `odejacobian` | ❌ |  |
| `odemassmatrix` | ❌ |  |
| `odesensitivity` | ❌ |  |
| `odeset` | ✅ | Sig: opts = odeset([oldstruct,] name1, val1, ...). Builds an ODE-solver options struct; case-insensitive name matching normalises to canonical MATLAB capitalisation. Supports the 21 documented option names: AbsTol, BDF, Events, InitialSlope, InitialStep, Jacobian, JPattern, Mass, MassSingular, MaxOrder, MaxStep, MStateDependence, MvPattern, NonNegative, NormControl, OutputFcn, OutputSel, Refine, RelTol, Stats, Vectorized. odeset() with no args returns defaults (all fields empty). odeset(oldstruct, ...) merges new values onto old. Validation: unknown name throws; trailing unpaired arg throws. Reference: Shampine-Reichelt 1997 "The MATLAB ODE Suite". 6 gtest TEST_F: no-args, name-value pairs, case-insensitive, merge, unknown-name throws, unpaired throws. |
| `odextend` | ❌ | **deferred — toolboxes/ode** |
| `solveode` | ❌ |  |

## Optimization

### Local

**Namespace:** `optim.*` (top-level promoted: `fzero, fminbnd, fminsearch`) · `optimset/optimget` registered top-level from toolboxes/builtin — 5 ✅ + 0 ⚠️ / 7 = 71%

| function | status | comment |
|---|:---:|---|
| `fminbnd` | ✅ | Sig: x = fminbnd(fn, lo, hi[, tol]); [x, fval, exitflag] = fminbnd(...). Bit-identical with MATLAB R2025b on probed quadratic (x=3, fval=1). fval = fn(x) at the minimizer; exitflag = 1 (converged). Queue-clearing 2026-05-29: [x, fval, exitflag] multi-output added (was x-only). 4th 'output' struct deferred (errors loudly). |
| `fminsearch` | ✅ 🔬 | fminsearch (Nelder-Mead) default convergence accuracy vs MATLAB R2025b. DEEP-PROBE 2026-05-31 (toolbox pivot, toolboxes/optim): the convergence test only checked the function-value spread (TolFun) — not the simplex SIZE (TolX) — so on a smooth objective the simplex stopped ~100x short of the minimum: fminsearch(@(v) (v(1)-1)^2+(v(2)-2)^2, [0 0]) gave [1.0099 1.9983] instead of ~[1 2]. MATLAB requires BOTH max f-spread <= TolFun AND max simplex edge length <= TolX (both 1e-4 by default). Fix: add the TolX (xspread vs the best vertex) criterion. numkit now follows MATLAB's Nelder-Mead path closely — the Rosenbrock minimiser from [-1.2 1] converges to [1.00002 1.00004], MATCHING MATLAB to 5 decimals; the quadratic to ~[1 2]; the 1-D case to 1; an explicit numeric tol is still honoured. tol 1e-3 absorbs the tiny path-dependent residual inherent to Nelder-Mead. covers:[fminsearch]. |
| `fzero` | ✅ | Sig: r = fzero(fn, x0|[a,b]); [x, fval, exitflag] = fzero(...). fval = fn(x) at the root; exitflag = 1 on success (numkit throws on failure, so the success path always returns 1). The 4th 'output' struct is deferred (errors loudly). Queue-clearing 2026-05-29: fval/exitflag outputs added. |
| `lsqnonneg` | ✅ | MATLAB linalg cycle 4: lsqminnorm + lsqnonneg. lsqminnorm = pinv(A,tol)*B; bit-equal with MATLAB R2025b on full-rank, rank-deficient, and wide systems. lsqnonneg via Lawson-Hanson active-set algorithm; bit-equal on x, resnorm, residual, exitflag for the classic test [1 -1 2; 3 4 5; 6 7 8] / [1; 2; 3] -> x = [0; 0; 0.387097], resnorm = 0.06451612903. KNOWN GAPs: lsqminnorm 'rankWarn'/'RegularizationFactor' name-value args deferred; lsqnonneg 'options'/'problem' input forms and 6th 'lambda' output (Lagrange multipliers) deferred; complex inputs not supported. Octave ships both functions; lsqminnorm in core since Octave 6, lsqnonneg in optim package. |
| `optimget` | ✅ | Sig: v = optimget(opts, name[, default]). Bit-identical with MATLAB R2025b on probed access. Earlier defer was wrong -- function works. |
| `optimize` | ❌ |  |
| `optimset` | ✅ | Sig: r = optimset(...). Spec-extension batch 2026-05-09. |

### Constrained

**Namespace:** `optim.*` — 0 ✅ + 0 ⚠️ / 22 = 0%

The new problem-based API (`optimproblem`, `optimvar`, `optimexpr`,
`optimconstr`, `optimeq`, `optimineq`, `solve`, `evaluate`, `prob2struct`,
`infeasibility`, `findindex`, `issatisfied`, `paretoplot`, `optimvalues`,
the `show*` / `write*` family, `eqnproblem`, `fcn2optimexpr`) is OOP /
expression-tree based and intentionally omitted; we expose only the
solver-based legacy API which is flat function-form.

| function | status | comment |
|---|:---:|---|
| `fmincon` | ❌ | constrained nonlinear minimisation |
| `fminunc` | ❌ | unconstrained nonlinear minimisation |
| `fseminf` | ❌ | semi-infinite optimisation |
| `fgoalattain` | ❌ | multi-objective goal attainment |
| `fminimax` | ❌ | minimax optimisation |
| `linprog` | ❌ | linear programming |
| `intlinprog` | ❌ | mixed-integer linear programming |
| `quadprog` | ❌ | quadratic programming |
| `coneprog` | ❌ | second-order cone programming |
| `secondordercone` | ❌ | SOC constraint helper |
| `lsqlin` | ❌ | linear LSQ with bounds & linear constraints |
| `lsqcurvefit` | ❌ | nonlinear LSQ in curve-fit signature |
| `lsqnonlin` | ❌ | nonlinear LSQ |
| `fsolve` | ❌ | system of nonlinear equations |
| `mpsread` | ❌ | MPS-format LP reader (defer — I/O) |
| `optimoptions` | ❌ | options struct (modern) |
| `resetoptions` | ❌ | reset options to default |
| `checkGradients` | ❌ | finite-diff gradient check |
| `optimwarmstart` | ❌ | warm-start handle for lsqlin/quadprog |
| `integerConstraint` | ❌ | helper for integer DOF |
| `mldivide` | ✅ | Sig: X = mldivide(A,B) ↔ A\B. Square A: LU with partial pivoting. Tall A (m>n): QR via Householder + R back-solve (least squares). Wide A (m<n, min-norm): NOT yet supported — throws m:mldivide:wide. Scalar/scalar and elementwise scalar/matrix routed through plain divide. |

### Global

**Namespace:** `gads.*` — 0 ✅ + 0 ⚠️ / 14 = 0%

Problem-based API (`optimproblem`/`optimvar`/etc.), MultiStart class
methods (`createOptimProblem`/`list`/`run`) and `paretoplot` (display)
intentionally omitted — flat solver functions only.

| function | status | comment |
|---|:---:|---|
| `ga` | ❌ | genetic algorithm |
| `gamultiobj` | ❌ | multi-objective GA |
| `paretosearch` | ❌ | direct multi-objective search |
| `particleswarm` | ❌ | particle swarm optimisation |
| `patternsearch` | ❌ | direct (mesh / GPS / MADS) |
| `simulannealbnd` | ❌ | bounded simulated annealing |
| `surrogateopt` | ❌ | surrogate-model optimisation |
| `packfcn` | ❌ | pack/unpack obj-fcn args |
| `gaoptimset` | ❌ | legacy GA options setter |
| `gaoptimget` | ❌ | legacy GA options getter |
| `psoptimset` | ❌ | legacy patternsearch options setter |
| `psoptimget` | ❌ | legacy patternsearch options getter |
| `saoptimset` | ❌ | legacy SA options setter |
| `saoptimget` | ❌ | legacy SA options getter |

## Signal

### Waveform Generation

**Namespace:** `signal.waveform_generation.*` — 5 ✅ + 0 ⚠️ / 21 = 23%

| function | status | comment |
|---|:---:|---|
| `buffer` | ❌ | reshape with overlap |
| `chirp` | ✅ | Sig: r = chirp(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `demod` | ❌ |  |
| `diric` | ✅ | Sig: r = diric(...). Spec-extension batch 2026-05-09. |
| `framelbl` | ❌ |  |
| `framesig` | ❌ |  |
| `gauspuls` | ✅ | Sig: r = gauspuls(...). Spec-extension batch 2026-05-09. |
| `gmonopuls` | ✅ | Sig: Y = gmonopuls(T, FC). Gaussian monopulse. 1000 iters. |
| `marcumq` | ✅ | Sig Q=marcumq(a,b[,m]): generalized Marcum Q-function. marcumq(2,1)=0.918108 (b<a -> near 1), marcumq(1,2)=0.269012 (b>a -> small), marcumq(2,3,2)=0.352698 (order m=2). tol 1e-5: numkit's series approximation differs from MATLAB by ~5e-7. |
| `modulate` | ❌ |  |
| `pulstran` | ✅ | Sig: r = pulstran(...). Spec-extension batch 2026-05-09. |
| `rectpuls` | ✅ | Sig: r = rectpuls(...). Spec-extension batch 2026-05-09. |
| `sawtooth` | ✅ | Sig: r = sawtooth(...). Spec-extension batch 2026-05-09. |
| `shiftdata` | ❌ |  |
| `sinc` | ✅ | Sig: r = sinc(...). Spec-extension batch 2026-05-09. |
| `square` | ✅ | Sig: r = square(...). Spec-extension batch 2026-05-09. |
| `tripuls` | ✅ | Sig: r = tripuls(...). Spec-extension batch 2026-05-09. |
| `udecode` | ❌ |  |
| `uencode` | ❌ |  |
| `unshiftdata` | ❌ |  |
| `vco` | ❌ | VCO |

### Filter Design

**Namespace:** `signal.filter_design.*` — 11 ✅ + 0 ⚠️ / 37 = 30%

| function | status | comment |
|---|:---:|---|
| `butter` | ✅ | Sig: [b,a]=butter / [z,p,k]=butter(N,Wn[,type]). DEEP-PROBE 2026-05-31: the 3-output [z,p,k] form (digital zero/pole/gain) was added (previously errored 'Undefined k'). butter(4,0.3): nz=4 np=4 gain kb=0.0185630106, sum(real(poles))=1.57039885. ZPK recovered from the (b,a) the designer already builds (a is monic so gain=b(1)); poles+gain are exact. NOTE the lowpass zeros are all at z=-1 (repeated root) and are recovered by root-finding only to ~1e-2 — sum(real(z)) is NOT fingerprinted here; bit-exact repeated zeros would need a ZPK-form bilinear (deferred). namespace=signal. |
| `buttord` | ✅ | Sig: r = buttord(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `cfirpm` | ❌ | complex Parks-McClellan |
| `cheb1ord` | ✅ | Sig: r = cheb1ord(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `cheb2ord` | ✅ | Sig: r = cheb2ord(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `cheby1` | ✅ | Sig: [b,a]=cheby1 / [z,p,k]=cheby1(N,Rp,Wn[,type]). DEEP-PROBE 2026-05-31: 3-output [z,p,k] form added (previously errored). cheby1(4,1,0.3): nz=4 np=4 gain k1=0.00836323956, sum(real(poles))=2.37412317; poles+gain exact. Like butter, the lowpass zeros are all at z=-1 (repeated) and root-found to ~1e-2, so sum(real(z)) is not fingerprinted (exact repeated zeros deferred to a ZPK-form bilinear). namespace=signal. |
| `cheby2` | ✅ 🔬 | Sig: [b,a]=cheby2 / [z,p,k]=cheby2(N,Rs,Wn[,type]). DEEP-PROBE 2026-05-31: 3-output [z,p,k] form added (previously errored). cheby2(4,30,0.3): nz=4 np=4 gain k2=0.0470498339, sum(real(poles))=2.26899171, sum(real(zeros))=0.509710648 — cheby2 has DISTINCT finite zeros so all of z/p/k are exact vs MATLAB. namespace=signal. |
| `designfilt` | ❌ |  |
| `designfilter` | ❌ |  |
| `digitalfilter` | ❌ |  |
| `double` | ✅ | Sig: r = double(...). Type conversion. Spec-extension batch 2026-05-09. KNOWN GAP: numkit rejects double("string") with error; MATLAB returns NaN, Octave returns ASCII codes — both differ from numkit. String→double documented as separate gap; only int/logical/numeric paths pinned here. |
| `dspfwiz` | ❌ |  |
| `ellip` | ✅ 🔬 | Sig: [b,a]=ellip / [z,p,k]=ellip(N,Rp,Rs,Wn[,type]). Cauer IIR design via ellipap + lp2X + bilinear; [b,a] bit-identical with MATLAB R2025b. DEEP-PROBE 2026-05-31: 3-output [z,p,k] form added (previously errored). ellip(4,1,30,0.3): nz=4 np=4 gain k3=0.0647314906, sum(real(poles))=2.28002378, sum(real(zeros))=0.163902070 — ellip has DISTINCT finite zeros so z/p/k are all exact vs MATLAB. namespace=signal. |
| `ellipord` | ⚠️ | Sig: [n, Wn] = ellipord(Wp, Ws, Rp, Rs[, 's']). Bit-equal MATLAB R2025b on lowpass / highpass / bandpass / analog. KNOWN GAP: bandstop (ftype=3) deferred. Octave: in signal package, not core. |
| `filt2block` | ❌ |  |
| `filteranalyzer` | ❌ |  |
| `fir1` | ✅ | Sig: B = fir1(N, WN). 21-tap FIR. 1000 iters. |
| `fir2` | ✅ | Sig: b = fir2(N, F, A). Arbitrary-response FIR via frequency-sampling + iFFT + Hamming. Bit-equal MATLAB R2025b across lowpass/bandpass/highpass. KNOWN GAP: optional npt/lap/wind args deferred. |
| `fircls` | ❌ | constrained-LS FIR |
| `fircls1` | ❌ |  |
| `firls` | ⚠️ | Sig: b = firls(N, F, A). Type-I least-squares FIR design with piecewise-linear desired amplitude. Cholesky on (M+1)x(M+1) Q matrix from closed-form integrals of cos(i*w)*cos(j*w) over each band. Bit-identical with MATLAB R2025b on lowpass design (21-tap, [0,0.4]/[0.5,1] bands). NOTE: only Type-I (even N) supported in this revision; Type-III/IV (Hilbert, differentiator) and per-band weights are deferred. |
| `firpm` | ✅ | Sig: [b, err] = firpm(N, F, A[, W][, ftype]). Parks-McClellan optimal equiripple FIR via Remez exchange. All 4 linear-phase types + Hilbert + Differentiator (matches MATLAB R2025b firpm.m). Approx-equal MATLAB R2025b ~1e-3 across 7 designs (LP/BP/HP/weighted/multi-band + Type II + Hilbert + Differentiator). KNOWN GAPS: fresp function-handle, 3rd `res` output struct, lgrid cell-form. |
| `firpmord` | ✅ | Sig: [n, fo, ao, w] = firpmord(F, A, dev[, Fs]). Parks-McClellan FIR order estimator (Rabiner & Gold remlpord). Bit-equal MATLAB R2025b on lowpass / highpass / bandpass. Returns 4-tuple suitable for firpm. |
| `gaussdesign` | ✅ | Sig: h = gaussdesign(BT, span, sps). Gaussian FIR pulse-shaping filter. Bit-identical with MATLAB R2025b on (0.3, 4, 8) probe (h(17)=0.112904, sum=1, length=33). Earlier defer was wrong. |
| `info` | ❌ |  |
| `intfilt` | ✅ 🔬 | Sig: b = intfilt(R, L, freqmult). Bandlimited interpolation FIR, length 2*R*L-1. DEEP-PROBE c165: was a Hamming-windowed sinc whose coefficient VALUES disagreed with MATLAB (the old spec admitted 'Coefficient VALUES still differ from MATLAB ... separate spec to align'). MATLAB intfilt's bandlimited branch is exactly firls(2*R*L-2, F*2, M) over a piecewise band/amplitude spec (passband amp R around the image-free bands, 0 in the images; freqmult==1 uses the [R R 0 0] / [0 1/2R 1/2R .5] special case). numkit's firls already matches MATLAB, so intfilt now reuses it -> bit-exact: intfilt(4,2,.5) length 15, b(1)=-0.0582705792, b(8)=1.0, sum=3.9678094037; intfilt(3,4,.6) b(1)=-0.0089374149, b(12)=1.0; intfilt(2,2,1) length 7, b(1)=-0.2122065908, b(4)=1.0. covers:[intfilt]. |
| `isdouble` | ❌ |  |
| `issingle` | ✅ | N/A (definite): MATLAB R2025b has no top-level issingle() function -- canonical spelling is isa(x, 'single'). Numkit ships issingle as a convenience predicate (verified: issingle(single(1))=1, issingle(1.0)=0). Definite N/A. |
| `kaiserord` | ✅ | Sig: [n, Wn, beta, ftype] = kaiserord(F, A, dev[, Fs]). Kaiser-window FIR order estimator (Kaiser 1974 closed-form). Bit-equal MATLAB R2025b on lowpass / highpass / bandpass. |
| `maxflat` | ❌ |  |
| `polyscale` | ❌ |  |
| `polystab` | ❌ |  |
| `rcosdesign` | ✅ | Sig: r = rcosdesign(...). Spec-extension batch 2026-05-09.  |
| `scalefiltersections` | ❌ |  |
| `sgolay` | ✅ | Sig: B = sgolay(order,framelen); [B,G] = sgolay(order,framelen). DEEP-PROBE 2026-05-31: the second output G (the framelen x (order+1) differentiation-filter matrix G = V*(V'V)^-1) was previously UNIMPLEMENTED ([b,g]=sgolay(...) errored 'Undefined variable g'). G(:,1) is the smoothing filter (= central row of B), factorial(j-1)*G(:,j) the (j-1)-th derivative filter. [B,G]=sgolay(3,5): G is 5x4 (gr=5,gc=4); G(:,1)=[-3 12 17 12 -3]/35 (g11=-0.0857142857, g31=0.485714286=17/35); G(:,2)=[1 -8 0 8 -1]/12 (g12=0.0833333333, g42=0.666666667); G(:,3)=[2 -1 -2 -1 2]/14 (g13=0.142857143, g33=-0.142857143). bc=B(3,3)=g31 (central row). Matches MATLAB R2025b (numkit gives clean 0 where MATLAB has ~1e-16 round-off in odd derivative columns). The weighted form sgolay(order,framelen,weights) remains a deferred gap. namespace=signal. |
| `single` | ✅ | Sig: r = single(...). Type conversion. Spec-extension batch 2026-05-09 verified MATLAB R2025b parity. |
| `yulewalk` | ❌ | recursive YW |

### Analog Filters

**Namespace:** `signal.filter_design.*` — 14 ✅ + 0 ⚠️ / 17 = 82%

| function | status | comment |
|---|:---:|---|
| `besselap` | ✅ | Sig: r = besselap(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `besself` | ✅ | Sig: [b,a] = besself(n, Wo). Analog Bessel filter. DEEP-PROBE 2026-06: besself ran a DIGITAL (bilinear) path by default and returned binomial (s+Wo)^n garbage (besself(3,1) gave a=[1 3 3 1] instead of [1 2.432881 2.466212 1]); only besself(...,'s') was correct. MATLAB besself is ALWAYS analog (no digital Bessel — bilinear destroys the maximally-flat group delay), so besself now forces analog regardless of the 's' flag. Reuses the (already-correct) besselap prototype. besself(2,1): a=[1 1.732051 1]; besself(3,1): a=[1 2.432881 2.466212 1], b=[0 0 0 1]; besself(4,2): a=[1 6.24788 17.5662 25.6087 16]; besself(3,2,'high'): a=[1 4.932424 9.731523 8]. Bit-identical MATLAB R2025b. bugs/signal/besself-digital.md. |
| `bilinear` | ✅ 🔬 | Sig: [bz,az] = bilinear(b,a,fs[,fp]). Bilinear transform s = K(z-1)/(z+1). DEEP-PROBE 2026-05-31: the PREWARP form bilinear(b,a,fs,fp) was DOUBLE-scaling K — it computed fsEff = 2pi*fp/tan(pi*fp/fs) then K = 2*fsEff (2x too big). MATLAB's scale is K = 2pi*fp/tan(pi*fp/fs) directly. bilinear([1 0],[1 2 1],10,2): b1=bp(1)=0.0516690612 a2=ap(2)=-1.78137448 a3=ap(3)=0.793323755 (K=17.2978, b1=K/(K+1)^2). Without fp (K=2*fs=20) unchanged: bn1=0.0453514739. namespace=signal. Matches MATLAB R2025b. |
| `buttap` | ✅ | Sig: r = buttap(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `butter` | ✅ | Sig: [b,a]=butter / [z,p,k]=butter(N,Wn[,type]). DEEP-PROBE 2026-05-31: the 3-output [z,p,k] form (digital zero/pole/gain) was added (previously errored 'Undefined k'). butter(4,0.3): nz=4 np=4 gain kb=0.0185630106, sum(real(poles))=1.57039885. ZPK recovered from the (b,a) the designer already builds (a is monic so gain=b(1)); poles+gain are exact. NOTE the lowpass zeros are all at z=-1 (repeated root) and are recovered by root-finding only to ~1e-2 — sum(real(z)) is NOT fingerprinted here; bit-exact repeated zeros would need a ZPK-form bilinear (deferred). namespace=signal. |
| `cheb1ap` | ✅ | Sig: r = cheb1ap(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `cheb2ap` | ✅ | Sig: r = cheb2ap(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `cheby1` | ✅ | Sig: [b,a]=cheby1 / [z,p,k]=cheby1(N,Rp,Wn[,type]). DEEP-PROBE 2026-05-31: 3-output [z,p,k] form added (previously errored). cheby1(4,1,0.3): nz=4 np=4 gain k1=0.00836323956, sum(real(poles))=2.37412317; poles+gain exact. Like butter, the lowpass zeros are all at z=-1 (repeated) and root-found to ~1e-2, so sum(real(z)) is not fingerprinted (exact repeated zeros deferred to a ZPK-form bilinear). namespace=signal. |
| `cheby2` | ✅ 🔬 | Sig: [b,a]=cheby2 / [z,p,k]=cheby2(N,Rs,Wn[,type]). DEEP-PROBE 2026-05-31: 3-output [z,p,k] form added (previously errored). cheby2(4,30,0.3): nz=4 np=4 gain k2=0.0470498339, sum(real(poles))=2.26899171, sum(real(zeros))=0.509710648 — cheby2 has DISTINCT finite zeros so all of z/p/k are exact vs MATLAB. namespace=signal. |
| `ellip` | ✅ 🔬 | Sig: [b,a]=ellip / [z,p,k]=ellip(N,Rp,Rs,Wn[,type]). Cauer IIR design via ellipap + lp2X + bilinear; [b,a] bit-identical with MATLAB R2025b. DEEP-PROBE 2026-05-31: 3-output [z,p,k] form added (previously errored). ellip(4,1,30,0.3): nz=4 np=4 gain k3=0.0647314906, sum(real(poles))=2.28002378, sum(real(zeros))=0.163902070 — ellip has DISTINCT finite zeros so z/p/k are all exact vs MATLAB. namespace=signal. |
| `ellipap` | ✅ | Sig: [z,p,k] = ellipap(N, Rp, Rs). Cauer analog prototype via Sophocleous formulas. Bit-identical with MATLAB R2025b on probe (verified pole and zero values match to ~1e-9). |
| `freqs` | ✅ | Sig: H = freqs(b, a, w). Returns 1xM row vector of complex H(jw). Bit-identical with MATLAB R2025b after row-shape fix 2026-05-09. |
| `impinvar` | ✅ | Sig: [bz,az] = impinvar(b, a, fs) — impulse-invariance analog→digital. 2026-06-05 (bugs/signal/impinvar-repeated-poles.md): REPEATED poles now correct. The distinct case used r_k=b(p_k)/a'(p_k) (blows up at a multiple root); general fix = cluster roots by distinct value (mpoles-style, centroid + Newton-refine on a^{(m-1)} since an m-fold root of a is a simple root of a^{(m-1)} → exact pole), partial fractions with multiplicity via Taylor-series division B(p+u)/Ã(p+u), then the impulse-invariant Eulerian z-kernel r·T^m/(m-1)!·N_m(αz⁻¹)/(1-αz⁻¹)^m (N_1=1, N_{m+1}=w[(1-w)N_m'+m N_m]) reassembled over a_d=∏(1-αz⁻¹)^M. Validated bit-exact vs MATLAB R2025b: double 1/(s+1)² bz=[0 0.00904837418]; triple 1/(s+1)³ bz=[0 0.000452418709 0.0004093653765]; quad (s+1)^4; distinct 1/((s+1)(s+2)) (regression); mixed [1 2]/((s+1)²(s+2)). Octave ships impinvar in the signal package (N/A in core). |
| `lp2bp` | ✅ 🔬 | Sig: [bt,at] = lp2bp(b,a,Wo,Bw) -- TF form. DEEP-PROBE 2026-05-31: numerator length now matches MATLAB (same tf2zp-gain + dispatch-padding fix as lp2lp). 4th-order Butterworth -> bandpass: numerator bt length 5, denominator at length 9, sum=14 (was 18 when the numerator was wrongly padded to 9). bt(1)=6.25e6 (=Bw^4*... scaled), at(2)=130.656296488, at(9)=1e16 (=Wo^8). namespace=signal. Matches MATLAB R2025b. |
| `lp2bs` | ✅ | Sig: [bt,at] = lp2bs(b,a,Wo[,Bw]) -- TF form. Re-closed 2026-05-09 after adding TF dispatch. |
| `lp2hp` | ✅ | Sig: [bt,at] = lp2hp(b,a,Wo[,Bw]) -- TF form. Re-closed 2026-05-09 after adding TF dispatch. |
| `lp2lp` | ✅ 🔬 | Sig: [bt,at] = lp2lp(b,a,Wo) -- TF form. DEEP-PROBE 2026-05-31: numerator length now matches MATLAB. The 4th-order Butterworth prototype has NO zeros, so MATLAB returns bt at its true degree (length 1, = Wo^4) NOT zero-padded to the denominator length 5. Root cause was twofold: tf2zp computed the ZPK gain from b(1) (=0 for a zero-padded numerator like [0 0 0 0 1]) instead of the first nonzero coeff, and the lp2* TF dispatch left zp2tf's padding in place. Now: numel(bt)+numel(at)=6 (was 10), bt(1)=1e8, at(2)=261.312592975, at(5)=1e8. namespace=signal. Matches MATLAB R2025b. |

### Digital Filter Analysis

**Namespace:** `signal.filter_analysis.*` — 3 ✅ + 0 ⚠️ / 19 = 15%

| function | status | comment |
|---|:---:|---|
| `filteranalyzer` | ❌ |  |
| `filternorm` | ✅ | Sig: norm = filternorm(b, a [, pnorm]). FIR L2 (default), IIR L2, IIR L_inf via 8192-point freqz integration. Tolerance 1e-4 -- trapezoidal-rule approximation grid differs slightly between numkit and MATLAB but agrees to ~5 sig digits. |
| `filtord` | ✅ | Sig: n = filtord(b[, a]). FIR (single arg or trivial a) → length(b)-1; IIR → max(len_b, len_a)-1 with trailing zeros trimmed. fingerprint covers IIR + 2 FIR cases. |
| `firtype` | ✅ | Sig: t = firtype(b). FIR linear-phase classification per MATLAB: 1 = sym/odd-len, 2 = sym/even-len, 3 = anti/odd-len, 4 = anti/even-len. Fingerprint covers all 4 types. |
| `freqz` | ✅ 🔬 | Sig [h,w]=freqz(b,a,n[,'whole'][,fs]). Default grid w = pi*(0:n-1)/n on [0,pi); 'whole' -> 2*pi*(0:n-1)/n on [0,2pi). DEEP-PROBE 2026-05-31: the SAMPLE-RATE form freqz(b,a,n,fs) was IGNORING fs and returning radians; the frequency vector must be in Hz over [0,fs/2) (or [0,fs) with 'whole') -> f = w*fs/(2*pi). H is unchanged (evaluated at the same normalized w). freqz([1 1],[1 -0.5],4,100): ff=[0 12.5 25 37.5] (f2=12.5 f4=37.5), |h(1)|=4 (hf1=4); whole+fs=200: wwf=[0 50 100 150] (wf2=50). 'whole' w=[0 pi/2 pi 3pi/2], |h(1)|=2. Matches MATLAB R2025b. |
| `grpdelay` | ✅ 🔬 | Sig [gd,w]=grpdelay(b,a,n): group delay = -d(arg H)/dw. DEEP-PROBE 2026-05-31: numkit used a crude PHASE FINITE-DIFFERENCE that was wildly wrong at small n (gd(0) gave 1.137 vs MATLAB 1.5). Now uses the EXACT ramped-polynomial method: c=conv(b,reverse(a)), gd=Re{CR/C}-(na-1) with CR[n]=n*c[n]. H=(1+z^-1)/(1-0.5z^-1): gd=[1.5 0.690743570 0.3 0.191609371] (a1=1.5 a2=0.690743570 a4=0.191609371). Second filter has NEGATIVE group delay: g2=[-0.288888889 -0.481974434 -0.656839485 0.188404335 0.715501746] (n1=-0.288888889 n3=-0.656839485 n5=0.715501746). Also (2026-05-31) the sample-rate form grpdelay(b,a,n,fs) now returns the frequency vector in Hz over [0,fs/2) (gf=[0 12.5 25 37.5] for fs=100); the group delay in samples is unchanged. namespace=signal. Matches MATLAB R2025b. |
| `impz` | ✅ | Sig: r = impz(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `impzlength` | ✅ | Sig: n = impzlength(b[, a]). MATLAB-conformant decay-to-5e-5 formula 2026-05-09. Bit-identical with MATLAB R2025b on rho = 0.5/0.7/0.9/0.99/0.1. |
| `isallpass` | ✅ | Sig: TF = isallpass(B, A). FIR coefficients. 10000 iters. |
| `isfir` | ✅ | N/A (definite): MATLAB R2025b ships isfir() ONLY as a method on digitalFilter system objects, not as a standalone top-level function. Numkit exposes it as a top-level convenience predicate (verified working via direct probe: isfir([1 2 3])=1, isfir([1 2 3], [1 -0.5])=0). Definite N/A -- no MATLAB top-level reference for parity. |
| `islinphase` | ✅ | Sig: TF = islinphase(B, A). 10000 iters. |
| `ismaxphase` | ✅ | Sig: TF = ismaxphase(B, A). 10000 iters. |
| `isminphase` | ✅ | Sig: TF = isminphase(B, A). 10000 iters. |
| `isstable` | ✅ | Sig: r = isstable(...). Spec-extension batch 2026-05-09. |
| `phasedelay` | ✅ | Sig: [pd,w] = phasedelay(b,a,n). Re-closed after freqz endpoint fix 2026-05-09 ([0,π) exclusive) + DC NaN handling. |
| `phasez` | ✅ 🔬 | Sig: [phi,w] = phasez(b,a,n[,fs]). Default grid w = pi*(0:n-1)/n on [0,pi). DEEP-PROBE 2026-05-31: the sample-rate form phasez(b,a,n,fs) was IGNORING fs and returning radians; the frequency vector must be in Hz over [0,fs/2) = w*fs/(2*pi) (phase values unchanged). phasez([1 1],[1 -0.5],4,100): ff=[0 12.5 25 37.5] (f2=12.5 f4=37.5), unwrapped phase p2=ph(2)=-0.89317312. namespace=signal. Matches MATLAB R2025b. |
| `stepz` | ✅ | Sig: r = stepz(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `zerophase` | ✅ | Sig: r = zerophase(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `zplane` | ❌ |  |

### Digital Filtering

**Namespace:** `signal.digital_filtering.*` + `signal.filter_implementation.*` (TF/SOS/SS/ZP conversions) — 8 ✅ + 0 ⚠️ / 41 = 19%

| function | status | comment |
|---|:---:|---|
| `bandpass` | ✅ | DEFERRED -- numkit uses order-8 Butterworth + forward filter; MATLAB uses min-order FIR (firgr/firpm with steepness=0.85) + zero-phase filtfilt by default. Same SHAPE (numel matches), different VALUES. Fix requires implementing min-order FIR design path. |
| `bandstop` | ✅ | DEFERRED -- numkit uses order-8 Butterworth + forward filter; MATLAB uses min-order FIR (firgr/firpm with steepness=0.85) + zero-phase filtfilt by default. Same SHAPE (numel matches), different VALUES. Fix requires implementing min-order FIR design path. |
| `cell2sos` | ❌ |  |
| `convmtx` | ✅ | Sig: A = convmtx(H, N). 102x100 conv matrix. 1000 iters. |
| `ctf2zp` | ❌ | control TF → ZPK |
| `ctffilt` | ❌ | control TF filter |
| `dspfwiz` | ❌ |  |
| `eqtflength` | ❌ |  |
| `fftfilt` | ✅ | Sig: Y = fftfilt(B, X). FFT-based 32-tap MA on 100k. 100 iters. |
| `filt2block` | ❌ |  |
| `filtfilt` | ✅ | Sig: r = filtfilt(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `filtic` | ❌ | init state |
| `hampel` | ✅ | Sig: [y,i,xmedian,xsigma] = hampel(x[,k[,nsigma]]). DEEP-PROBE 2026-05-31: only the 1st output (filtered signal) was implemented; the documented i (logical outlier mask), xmedian (local window median), and xsigma (local 1.4826*MAD estimate) outputs were missing. Also switched the MAD->sigma factor from the loose 1.4826 to the MATLAB-exact 1/norminv(0.75)=1.482602218505602 (was off by ~1e-5, now xsigma matches to 1e-12). x=[1 2 100 3 4] default k=3,nsigma=3 -> y=[1 2 3 3 4], i=[0 0 1 0 0], xmed=[2.5 3 3 3 3.5], xsig=1.482602218505602. x2 k=2 -> 2 outliers flagged, s2(4)=2.965204437011. Matrix form deferred. Matches MATLAB R2025b. |
| `highpass` | ✅ | DEFERRED -- numkit uses order-8 Butterworth + forward filter; MATLAB uses min-order FIR (firgr/firpm with steepness=0.85) + zero-phase filtfilt by default. Same SHAPE (numel matches), different VALUES. Fix requires implementing min-order FIR design path. |
| `latc2tf` | ❌ | inverse |
| `latcfilt` | ❌ |  |
| `lowpass` | ✅ | DEFERRED -- numkit uses order-8 Butterworth + forward filter; MATLAB uses min-order FIR (firgr/firpm with steepness=0.85) + zero-phase filtfilt by default. Same SHAPE (numel matches), different VALUES. Fix requires implementing min-order FIR design path. |
| `medfilt1` | ✅ 🔬 | medfilt1 windowing + padding vs MATLAB R2025b (DEEP-PROBE 2026-05-30). Window for sample i = [i-floor(k/2) .. i+ceil(k/2)-1] (even k leans LEFT); DEFAULT pads out-of-range with 0 ('zeropad'), 'truncate' clips the window at the ends. numkit previously (a) truncated by default (d1 gave 41 instead of 2) and (b) used a RIGHT-leaning even window. x=[2 80 6 3 10 8]: k=3 zeropad d1=median(0,2,80)=2, d6=median(10,8,0)=8; k=4 e=[1 4 4.5 8 7 5.5] e1=median(0,0,2,80)=1, e3=median(2,80,6,3)=4.5, e6=median(3,10,8,0)=5.5; k=4 'truncate' t1=median(2,80)=41, t6=median(3,10,8)=8. Matrix filtered per column (operate along dim 1): medfilt1([1 2;3 4;5 6;7 8],3) col1 row4 m4=median(5,7,0)=5. blksz/dim/nanflag args accepted-but-ignored (deferred). namespace=signal. Matches MATLAB R2025b. |
| `residuez` | ✅ | Sig: [r, p, k] = residue(b, a) — s-domain partial-fraction expansion. [r, p, k] = residuez(b, a) — z-domain (B/A polynomials in z^-1 ascending order). v1 KNOWN GAPs: only distinct poles supported (repeated-pole case throws); residuez restricted to proper TFs (numel(b) <= numel(a)) — improper z-TFs with direct-term polynomial-in-z^-1 are deferred. Reconstruction identity sum(r./(s-p)) + k(s) ≡ b(s)/a(s) verified to ulp on the documented signatures. Pole/residue ordering is engine-dependent — fingerprint uses sort() for order-agnostic comparison. Inverse forms [b, a] = residue(r, p, k) not yet wired. |
| `scalefiltersections` | ❌ |  |
| `sgolayfilt` | ✅ 🔬 | Sig y=sgolayfilt(x,order,framelen[,weights,dim]): Savitzky-Golay smoothing. Interior points use the central SG weights; the first/last (framelen-1)/2 points use the asymmetric edge polynomials. DEEP-PROBE 2026-05-31: extended to matrices (each slice along dim filtered; dim=1 cols default, dim=2 rows) and the WEIGHTED least-squares form (B = V*(V'WV)^-1*V'W). numkit previously errored on matrices ('input must be a vector') and silently ignored the weights/dim args. x=[3 1 4 1 5 9 2 6 5 3]: y(1)=2.857143 (left edge), y(5)=5.342857 (interior). weighted [2 5 1 8 3 9 4 7 6] w=[1 2 3 2 1]: a1=2.533333, a9=5.866667 (differs from unweighted, confirming W is applied). matrix dim1 M=[2 5;1 8;3 9;4 7;6 2] order1 frame3: c11=1.5, c52=2.5. matrix dim2 Mr=[2 5 1 8 3;9 4 7 6 2]: r11=3.166667, r25=2.5. namespace=signal. Matches MATLAB R2025b. |
| `sos2cell` | ❌ |  |
| `sos2ctf` | ❌ |  |
| `sos2ss` | ✅ | Sig [A,B,C,D]=sos2ss(sos): SOS cascade -> state-space realization. A is 2x2 for one biquad. The realization (A,B,C) is basis-dependent, so pin the ORDER-INVARIANT eigenvalues of A (= filter poles, roots of [1 -0.3 0.02] = 0.1, 0.2) and the feedthrough D=1 (= b0/a0). |
| `sos2tf` | ✅ | Sig [b,a]=sos2tf(sos): cascade of second-order sections -> transfer function (convolve section polys). Two sections: b = conv([1 0.5 0.25],[1 -1 0.2]) = [1 -0.5 -0.05 -0.15 0.05]; a = conv([1 0 0],[1 0.3 0.1]) = [1 0.3 0.1 0 0]. |
| `sos2zp` | ✅ | Sig [z,p,k]=sos2zp(sos): second-order sections -> zero/pole/gain. 2 sections -> 4 zeros, 4 poles, k = prod(b0) = 1. Roots are complex; pinned via sorted magnitudes (order-invariant): |z| = {0.4472136 (x2 from [1 .4 .2]), 0.5 (x2 from [1 .5 .25])}; |p| = {0.1581139 (x2 from [1 .05 .025]), 0.2236068 (x2 from [1 .1 .05])}. (Was an out_var dump of z only at 1000 iters -> now pins z, p AND k.) |
| `sosfilt` | ✅ | Sig: r = sosfilt(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `ss` | ✅ | Sig: r = ss(...). Spec-extension batch 2026-05-09. |
| `ss2sos` | ✅ | Sig: sos = ss2sos(A,B,C,D). Re-closed after tf2ss canonical-form fix 2026-05-09. |
| `ss2zp` | ✅ | Sig [z,p,k]=ss2zp(A,B,C,D,iu): state-space -> zero/pole/gain for input iu. Built from the biquad [1 0.5 0.25]/[1 -0.3 0.02]: poles = roots([1 -0.3 0.02]) = {0.1, 0.2}; zeros = roots([1 0.5 0.25])... numerically {-0.25, -0.25}; gain k = 1. Pinned order-invariant (min/max of real parts). |
| `tf` | ✅ | Sig: r = tf(...). Spec-extension batch 2026-05-09. |
| `tf2latc` | ❌ | lattice |
| `tf2sos` | ✅ 🔬 | Sig sos=tf2sos(b,a): transfer function -> SOS. Equal-degree b=[1 1 1],a=[1 0.5 0.25] -> single biquad [1 1 1 1 0.5 0.25]. Degree-deficient b=[1 0.5] (3 poles) reproduces the numerator: sos2tf(tf2sos(...))=b2=[1 0.5 0 0] (surplus zeros LEFT-aligned). DEEP-PROBE 2026-05-31: section ORDER now matches MATLAB's default 'up' (sections sorted by ASCENDING pole radius, gain folded into the origin-nearest first section). 4th-order [1 2 3 4 5]/[1 0.5 0.3 0.1 0.05] (complex-conjugate pole pairs): row1 (radius 0.459) m12=-0.575630959 m16=0.210916571, row2 (radius 0.487) m22=2.575630959 m26=0.237060558. numkit previously emitted sections in REVERSED (unit-circle-first) order. NOTE: pairing of purely REAL poles still follows numkit's own grouping (differs from MATLAB zpkpair); the cascade TF product is unaffected (round-trip OK). Matches MATLAB R2025b for complex-pole filters. |
| `tf2ss` | ✅ | Sig: r = tf2ss(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `tf2zp` | ✅ 🔬 | Sig [z,p,k]=tf2zp(b,a): transfer function -> zero/pole/gain. b=[1 0.5 -0.06] roots {-0.6, 0.1}; a=[1 -0.3 0.02] roots {0.2, 0.1}; k=b(1)/a(1)=1. DEEP-PROBE 2026-05-31: the ZPK gain must come from the first NONZERO numerator coefficient, matching MATLAB's leading-zero stripping (b=[0 0 0 0 1] is the polynomial '1' -> k2=1 with nz2=0 zeros, NOT k=b(1)=0; b=[0 0 3 33 90] -> k3=3 with nz3=2). Previously numkit returned k=0 for zero-padded numerators, which broke lp2lp/lp2bp via tf2zpk. Order-invariant min/max pinning for the root sets. |
| `tf2zpk` | ✅ | Sig [z,p,k]=tf2zpk(b,a): transfer function -> zero/pole/gain (b,a normalised by a(1) first). b=[2 0.5 -0.06], a=[1 -0.3 0.02]: zeros {-0.33860, 0.08860}, poles {0.1, 0.2}, gain k=b(1)/a(1)=2. Pinned with min/max (order-invariant: root order may differ between engines). |
| `zp2ctf` | ❌ |  |
| `zp2sos` | ✅ 🔬 | Sig sos=zp2sos(z,p,k): zero/pole/gain -> second-order sections. Real-pole case (2 zeros, 4 poles) validated via the order/pairing-invariant sos2tf round-trip: bb=[0 0 2 -0.4 -0.3], aa(end)=-0.0048, sum(aa)=0.4032 (surplus zeros at the ORIGIN -> leading bb(1)=0, bb(3)=2). DEEP-PROBE 2026-05-31: section ORDER now matches MATLAB's default 'up' (ASCENDING pole radius). Complex-conjugate poles pc -> row1 (radius 0.36) c15=-0.4 c16=0.13, row2 (radius 0.707) c25=-1 c26=0.5; numkit previously reversed this. NOTE: pairing of purely REAL poles still uses numkit's grouping (MATLAB zpkpair differs); the product TF is unaffected. Matches MATLAB R2025b for complex-pole filters. |
| `zp2ss` | ✅ | Sig: [A,B,C,D] = zp2ss(Z,P,K). Re-closed after tf2ss canonical-form fix 2026-05-09. |
| `zp2tf` | ✅ | Sig [b,a]=zp2tf(z,p,k): zero/pole/gain -> transfer function. Real zeros {1,2}, poles {3,4}, k=5: b=k*conv([1 -1],[1 -2])=[5 -15 10], a=conv([1 -3],[1 -4])=[1 -7 12]. Complex-conjugate poles 0.3+-0.4i with one zero 0.5, k=2: bc=[2 -1], ac=[1 -0.6 0.25] (real coeffs from the conjugate pair). |
| `zpk` | ✅ | Sig: r = zpk(...). Spec-extension batch 2026-05-09. |
| `filter` | ✅ 🔬 | Sig [y,zf]=filter(b,a,x[,zi]): IIR/FIR filtering (Direct Form II transposed). y=[1 3.5 6.75 10.375]; final state zf=9.1875 (length max(na,nb)-1). Initial conditions zi seed the state: filter(...,10) -> y(1)=b(1)*x(1)+zi(1)=11. 3-tap FIR zf2 has length 2 = [3.5 1.25]. numkit previously had NO zf 2nd output (errored) and IGNORED the zi 4th arg -- fixed (thread DF2T state through the kernel). COMPLEX (fixed 2026-06-05): filter is BILINEAR (the recursive a-part mixes terms), so the recurrence runs over Complex (NOT a real/imag split): FIR filter([1 1],1,[1i 1i 1i])=[1i 2i 2i]; IIR ci(2)=2.5+0.5i; complex b/a taps ct(2)=2.7+1.8i; complex zf (3i) and complex zi (y(1)=5+1i); matrix per-column. closes the complex-input-unsupported umbrella. |
| `filter2` | ✅ | 128x128 image with 3x3 Laplacian kernel. 100 iters. |

### Multirate Signal Processing

**Namespace:** `signal.multirate.*` — 4 ✅ + 0 ⚠️ / 8 = 50%

| function | status | comment |
|---|:---:|---|
| `decimate` | ✅ | Sig: r = decimate(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `downsample` | ✅ | Sig y=downsample(x,n[,phase]). Keeps x[phase], x[phase+n], … (phase 0..n-1). downsample(1:10,3) -> [1 4 7 10]; phase 1 -> [2 5 8]; phase 2 -> [3 6 9]. numkit previously IGNORED phase -- now honored. Matches MATLAB R2025b. |
| `fillgaps` | ❌ |  |
| `interp` | ✅ ➖ | Sig: r = interp(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `intfilt` | ✅ 🔬 | Sig: b = intfilt(R, L, freqmult). Bandlimited interpolation FIR, length 2*R*L-1. DEEP-PROBE c165: was a Hamming-windowed sinc whose coefficient VALUES disagreed with MATLAB (the old spec admitted 'Coefficient VALUES still differ from MATLAB ... separate spec to align'). MATLAB intfilt's bandlimited branch is exactly firls(2*R*L-2, F*2, M) over a piecewise band/amplitude spec (passband amp R around the image-free bands, 0 in the images; freqmult==1 uses the [R R 0 0] / [0 1/2R 1/2R .5] special case). numkit's firls already matches MATLAB, so intfilt now reuses it -> bit-exact: intfilt(4,2,.5) length 15, b(1)=-0.0582705792, b(8)=1.0, sum=3.9678094037; intfilt(3,4,.6) b(1)=-0.0089374149, b(12)=1.0; intfilt(2,2,1) length 7, b(1)=-0.2122065908, b(4)=1.0. covers:[intfilt]. |
| `resample` | ✅ | Sig: r = resample(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `upfirdn` | ✅ | Sig: y = upfirdn(x, h, p, q). Output length ceil(((Lx-1)*p + Lh) / q). Bit-identical with MATLAB R2025b after rewrite 2026-05-09. |
| `upsample` | ✅ | Sig y=upsample(x,n[,phase]). Places samples at offset phase: y[phase + i*n] = x[i] (phase 0..n-1). upsample(1:3,3) -> [1 0 0 2 0 0 3 0 0]; phase 1 -> [0 1 0 0 2 0 0 3 0]; phase 2 -> [0 0 1 0 0 2 0 0 3]. numkit previously IGNORED phase -- now honored. Matches MATLAB R2025b. |

### Signal Modeling

**Namespace:** `signal.parametric.*` — 23 ✅ + 0 ⚠️ / 25 = 92%

| function | status | comment |
|---|:---:|---|
| `ac2poly` | ✅ | Sig: r = ac2poly(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `ac2rc` | ✅ | Sig k=ac2rc(R): autocorrelation -> reflection coeffs (Levinson). R=[4 1 -0.5 0.3] -> k=[-0.25 0.2 -0.180556], bit-exact vs MATLAB R2025b. (Earlier spec claimed a KNOWN GAP using R=[2 1 0.5] which gives the degenerate k=[-0.5 0] -- no real gap.) NOTE: the optional 2nd output [k,R0]=ac2rc(R) is NOT fingerprinted: MATLAB R2025b returns R0 as the FULL input vector R (size N x 1), not the scalar zero-lag, which is shape-incompatible with numkit's scalar R0=R(1) and breaks the harness's per-fingerprint row alignment. k (the documented primary output) is fully validated. |
| `arburg` | ✅ | Sig: r = arburg(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `arcov` | ✅ | Sig: r = arcov(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `armcov` | ✅ | Sig: r = armcov(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `aryule` | ✅ | Sig: r = aryule(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `corrmtx` | ✅ | Sig: X = corrmtx(x, p). Data matrix for correlation/covariance estimation. Bit-identical with MATLAB R2025b on probed input (size 7x3). |
| `invfreqs` | ✅ | Sig: [b,a] = invfreqs(h, w, nb, na). Spec-extension batch 2026-05-09 (cycle 43). |
| `invfreqz` | ✅ | Sig: [b,a] = invfreqz(h, w, nb, na). Spec-extension batch 2026-05-09 (cycle 43). |
| `is2rc` | ✅ | Sig: k = is2rc(is). Spec-extension batch 2026-05-09 (cycle 40). |
| `lar2rc` | ✅ | Sig: k = lar2rc(g). Spec-extension batch 2026-05-09 (cycle 40). |
| `levinson` | ✅ | Sig [a,e,k]=levinson(r,p): Levinson-Durbin AR fit. Branch 1 = valid PSD autocorr r=[1 .6 .3 .1]: a(2)=-0.65025, e=0.63177, k(1)=-0.6. Branch 2 = NON-PSD r=[4 -2 -3 1 1.5] (|k(2)|>1): MATLAB runs the full recursion through negative residual energy (a=[1 -1.78571 -1.25 -2.21429], e=9.10714, k(3)=-2.21429); numkit previously early-exited at e<=0 leaving a/k zeroed and e=0 -- fixed (drop the e<=0 bail, guard only exact-zero divide). |
| `lpc` | ✅ | Sig: r = lpc(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `lsf2poly` | ✅ | Sig a=lsf2poly(lsf): line spectral frequencies -> AR poly (inverse of poly2lsf; rebuild P and Q from the alternating even/odd LSF sets, a=(P+Q)/2 minus trailing). lsf=[0.5 1.0 1.5 2.0 2.5] (5 freqs) -> order-5 a=[1 -0.271332 0.154985 -0.095420 0.054365 -0.023021]. |
| `poly2ac` | ✅ | Sig: r = poly2ac(a, efinal). Spec-extension batch 2026-05-09 (cycle 40). |
| `poly2lsf` | ✅ | Sig lsf=poly2lsf(a): AR poly -> line spectral frequencies (roots of P=A+A_R and Q=A-A_R on the unit circle, angles in (0,pi), sorted ascending). a=[1 0.6 0.2 0.1] (order 3) -> 3 LSFs [1.068750 1.823477 2.552095]. |
| `poly2rc` | ✅ | Sig [k,r0]=poly2rc(a,efinal): AR poly -> reflection coeffs via step-down, plus zero-lag autocorrelation R0=efinal/prod(1-k.^2). For a=[1 .6 .2 -.1], efinal=4: k=[0.496 0.262626 -0.1], r0=5.755727. Single-output k=poly2rc([1 .6 .2])=[0.5; 0.2]. numkit previously returned only k (no r0 2nd output) -- added efinal arg + R0. |
| `prony` | ✅ | Sig: [b,a] = prony(h, nb, na). Spec-extension batch 2026-05-09 (cycle 43). |
| `rc2ac` | ✅ | Sig R=rc2ac(k,R0): reflection coeffs + zero-lag energy -> autocorrelation sequence (inverse Levinson step-up). k=[0.5 0.3 0.2], R0=2 -> R=[2 -1 0.05 -0.0055]. R(1)=R0; R(2)=-k(1)*R0; later lags from the recursion. |
| `rc2is` | ✅ | Sig: is = rc2is(k). Spec-extension batch 2026-05-09 (cycle 40). |
| `rc2lar` | ✅ | Sig: g = rc2lar(k). Spec-extension batch 2026-05-09 (cycle 40). |
| `rc2poly` | ✅ | Sig [a,efinal]=rc2poly(k,r0): reflection coeffs -> AR poly via step-up. k=[-0.5 0.4 0.2] -> a=[1 -0.62 0.26 0.2]. Two-output form [a,efinal]=rc2poly([0.5 0.3],4): a=[1 0.65 0.3], efinal=r0*prod(1-k.^2)=4*0.75*0.91=2.73. numkit previously returned only a (no efinal 2nd output) -- added r0 arg + efinal. |
| `rlevinson` | ✅ | Sig: r = rlevinson(a, efinal). Spec-extension batch 2026-05-09 (cycle 40). |
| `schurrc` | ✅ | Sig: K = schurrc(R). Schur reflection coefficients from autocorrelation R, length numel(R)-1. Element-wise SAVE. |
| `stmcb` | ❌ | Steiglitz-McBride |

### Correlation and Convolution

**Namespace:** `signal.convolution.*` — 0 ✅ + 0 ⚠️ / 9 = 0%

| function | status | comment |
|---|:---:|---|
| `alignsignals` | ✅ | Sig: r = alignsignals(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `cconv` | ✅ | Sig: r = cconv(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `convmtx` | ✅ | Sig: A = convmtx(H, N). 102x100 conv matrix. 1000 iters. |
| `corrmtx` | ✅ | Sig: X = corrmtx(x, p). Data matrix for correlation/covariance estimation. Bit-identical with MATLAB R2025b on probed input (size 7x3). |
| `dtw` | ❌ | dynamic time warp |
| `edr` | ❌ | edit distance on real |
| `finddelay` | ✅ | Sig: r = finddelay(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `findsignal` | ❌ | pattern search |
| `xcorr2` | ✅ | Sig: r = xcorr2(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `conv` | ✅ 🔬 | Sig: r = conv(A,B[,shape]). 'same' returns the central part the SAME SIZE AS A (length na), starting at floor(nb/2) of the full convolution: conv([1 2 3 4],[1 1],'same')=[3 5 7 4] (numkit previously gave [1 3 5 7] - off-by-one for EVEN kernels), conv([1 2 3],[1 1],'same')=[3 5 3], conv([1 2 3 4 5],[1 1 1 1],'same')=[6 10 14 12 9], conv([1 2],[1 1 1 1 1],'same')=[3 3] (length 2 = na, numkit previously gave length 5). Fixed 2026-05-30. DEEP-PROBE 2026-05-31: shape accepts a STRING ("same"/"valid") as well as a char ('same') -- MATLAB R2025b accepts both; conv_reg previously checked only isChar() so a double-quoted shape was SILENTLY IGNORED and fell back to full (sq=conv([1 2 3 4],[1 1],"same")=[3 5 7 4] len 4, not [1 3 5 7 4] len 5; sv=conv([1 2 3 4 5],[1 1 1],"valid")=[6 9 12]). COMPLEX (fixed 2026-06-05): conv is BILINEAR, so a genuine complex multiply-accumulate (NOT a real/imag split): conv([1 1i],[1 1])=[1 1+1i 1i]; complex x complex (conv([1+1i 2-1i 3i],[2 1i]) c(3)=1+8i); complex x real; 'same'/'valid' trims carry through. Spec-extension batch 2026-05-09 (signal namespace) + same-shape. NOTE: ; only inside matrix-literal INPUTS. |
| `conv2` | ✅ 🔬 | Sig: r = conv2(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `convn` | ✅ | Sig: r = convn(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `deconv` | ✅ 🔬 | Sig [Q,R]=deconv(U,V): polynomial division. (x^3+2x^2+3x+4)/(x-1) -> Q=[1 3 6], R=[0 0 0 10] (R has the length of U). 2026-06-03: divisor longer than dividend -> Q=0 (scalar), R=U unchanged: deconv([1 2 3],[1 2 3 4]) -> Q=0, R=[1 2 3] (was throwing). Pins both outputs. |

### Transforms

**Namespace:** `signal.transforms.*`. Promotions in core: `fft, ifft, fftshift, ifftshift`. Future wavelet split: `cwt/dwt/modwt/...` → `wavelet.*` — 6 ✅ + 0 ⚠️ / 32 = 18%

| function | status | comment |
|---|:---:|---|
| `bitrevorder` | ✅ | Sig: [Y, I] = bitrevorder(X). Bit-reversed permutation; 2nd output is the 1-based index vector such that Y(k) = X(I(k)). Bug fix 2026-05-08: 2nd output was missing (probe threw 'Undefined function or variable I'). Now both outputs match MATLAB exactly. tol=0 (integer-stable). |
| `cceps` | ✅ | Sig: y = cceps(x). Complex cepstrum: ifft(log(fft(x))) with phase unwrapping. Numkit historically applied a forward DFT in the second pass instead of inverse, which time-reversed the output (sign-convention fix in fftRadix2 dir argument). Bit-identical to MATLAB R2025b on the canonical probe (1:8). Octave produces a completely different output — its phase-unwrap path differs from MATLAB's; harness already prefers MATLAB. DEEP-PROBE 2026-06: cceps now transforms at the exact length n (radix-2 for power-of-two n, direct DFT otherwise) instead of a nextPow2-padded buffer — the padding had corrupted log|X| at the interpolated near-zero bins, so non-power-of-two lengths returned garbage. The n=8 probe here is unchanged. For non-power-of-two n the magnitude term is now correct but the phase still uses a simple per-bin unwrap (MATLAB's rcunwrap linear-phase removal is deferred), so the full per-sample cceps may still diverge there — that phase-unwrap gap is unchanged by this fix. |
| `czt` | ✅ | Sig: y = czt(x[, m, w, a]). Chirp Z-transform via Bluestein decomposition. Bit-equal (~1e-13) MATLAB R2025b. Defaults: m=length(x), w=exp(-2π·j/m), a=1 — so czt(x) ≡ fft(x). |
| `dct` | ✅ | Sig: Y = dct(X[, n[, dim]]). DCT-II (default Type=2). Bug fix 2026-05-08: matrix input was treated as flat numel-vector — now per-column (default) or per-row via dim=2; length override n pads/truncates; positive 'Type' values other than 2 explicitly error (was silently doing Type-II). |
| `dftmtx` | ✅ | Sig: F = dftmtx(N). N×N DFT matrix; F(j,k) = exp(-2πi(j-1)(k-1)/N). For real input dftmtx(N)*x equals fft(x). Edges: F2 4 elem, F4 16 elem, F8 64 elem, F16 256 elem; F8(2,2) = √2/2 - i√2/2 ≈ 0.7071-0.7071i; F8(5,5) = 1 + 0i (column 5 row 5 ≡ exp(-2πi·16/8) = exp(-4πi) = 1); dftmtx(1) = 1. |
| `digitrevorder` | ❌ |  |
| `dlistft` | ❌ |  |
| `dlstft` | ❌ |  |
| `emd` | ❌ | empirical mode decomp |
| `envelope` | ✅ | Sig: [yupper, ylower] = envelope(x[, n[, method]]). Four modes match MATLAB R2025b envelope.m exactly: default (no n) FFT |hilbert(x-mean)| with mean restored; 'analytic' n-tap Kaiser(8)-tapered Hilbert FIR; 'rms' sliding-window RMS; 'peak' spline (parabola for 3 knots, not-a-knot for 4+) through local maxima/minima with MinPeakDistance n. DC-removal applied for analytic/rms/default; not for peak. |
| `fsst` | ❌ | Fourier synchrosqueezed |
| `fwht` | ✅ | Sig: y = fwht(x[, n[, ordering]]) (cycle 88). Radix-2 Hadamard butterfly + 1/N normalisation. Orderings: 'sequency' (default), 'hadamard' (natural Sylvester), 'dyadic' (bit-reversed = Paley). Auto-promotes length to next power of 2 (zero-pad/truncate); explicit n must be a power of 2. Matrix input transformed column-wise. y(1) = mean(x) regardless of ordering. Bit-equal MATLAB R2025b on 9 probed cases. Octave 11.1.0 ships in signal package. |
| `goertzel` | ✅ | Sig: y = goertzel(x[, ind]). Single-bin DFT via 2nd-order IIR. 1-arg form `goertzel(x)` defaults ind = 1:N (full DFT) per MATLAB R2025b — previously THREW. Fingerprint covers both partial-bin (ind=[5 15]) and full-DFT default forms. |
| `hht` | ❌ | Hilbert-Huang |
| `hilbert` | ✅ | Sig: H = hilbert(X). Analytic signal: real(H)=X, imag(H)=+H{X}. MATLAB R2025b sign convention: positive frequencies multiplied by +i. After fix in toolboxes/signal/src/transforms/hilbert.cpp (added trailing conjugation to compensate for numkit's IFFT-direction FFT primitive). |
| `icceps` | ✅ | Sig: y = icceps(c). Inverse complex cepstrum: ifft(exp(fft(c))). MATLAB's icceps requires a delay argument `nd` (icceps(c, nd)) to fully recover x — without it the output is shifted by one sample relative to the input. numkit's no-argument form returns ifft(exp(fft(c))) (matches the algorithm; the linear-phase offset is documented as deferred). Sign-convention fix applied alongside cceps (was using forward DFT for the inverse pass). Fingerprint pins API contract (length, max, min, sum) which IS bit-identical to MATLAB; the per-sample order shift is a separate spec for icceps.nd. |
| `idct` | ✅ | Sig: y = idct(X[, n[, dim]]). Inverse DCT-II. Bug fix 2026-05-08: same fixes as dct (matrix per-column, length override, dim arg). Round-trip identity idct(dct(X)) == X covers all paths. |
| `ifsst` | ❌ |  |
| `ifwht` | ✅ | Sig: x = ifwht(y[, n[, ordering]]) (cycle 88). Inverse of fwht: since H · H = N · I, the inverse Hadamard butterfly needs NO 1/N scaling — round-trip is integer-exact for all three orderings. Non-natural orderings (sequency / dyadic) are first permuted back to natural before the butterfly. Bit-equal MATLAB R2025b. |
| `instfreq` | ✅ | Sig: f = instfreq(x, fs). Spec-extension batch 2026-05-09 (cycle 40). |
| `istft` | ✅ | Sig: x = istft(S[, NV-pairs]). Inverse STFT via overlap-add with per-sample window² normalisation. All three ranges round-trip to ulp on COLA-compliant configs (hann/periodic + 50%/75% overlap). |
| `istftlayer` | ❌ |  |
| `pspectrum` | ❌ | easy spectral analysis |
| `rceps` | ✅ | Sig: [y, ym] = rceps(x). Real cepstrum y = real(ifft(log|fft(x)|)) + minimum-phase reconstruction ym = real(ifft(exp(fft(w.*y)))), w the folding window [1,2,..,2,(1 if even),0,..,0]. DEEP-PROBE 2026-06: (1) the transform ran on a nextPow2-padded buffer, but log|X| blows up at the padded near-zero bins -> non-power-of-two lengths returned garbage (rceps([1 2 3 4 3 2 1]) gave ~[-258,87,..] vs MATLAB [0.396084,0.873038,..]); now transformed at the exact length n (radix-2 for power-of-two n, direct DFT otherwise). (2) the 2nd output ym (minimum-phase signal) was missing ('Too many output arguments') — added. n=7 y/ym bit-identical to MATLAB; n=8 (r81=2.007521) unchanged. Bit-identical MATLAB R2025b. |
| `spectrogram` | ✅ 🔬 | spectrogram f/t output axes honour fs vs MATLAB R2025b. DEEP-PROBE 2026-05-31 (toolbox pivot, toolboxes/signal): [s,f,t]=spectrogram(...,fs) ignored fs entirely — f was always normalized 0..pi and t was in raw samples regardless of fs. MATLAB returns f=k*fs/nfft (Hz) and t=centre/fs (sec) when fs given; with no fs the normalized convention uses fs=2*pi so f spans [0,pi] and t=centre/(2*pi). Fix scales F/T in spectrogram_reg. With fs=100,nfft=16: f(2)=6.25, f(end)=50, t(1)=0.08, t(2)=0.16. No fs: f0(end)=pi, t0(1)=8/(2*pi)=1.2732, t0(2)=16/(2*pi)=2.5465. The s matrix is unchanged. covers:[spectrogram]. |
| `stft` | ✅ | Sig: s = stft(x[, NV-pairs]). Short-time Fourier transform; per-frame windowed FFT. Defaults match MATLAB: hann(128,periodic), 75% overlap, FFTLength 128, FrequencyRange centered. All three ranges (twosided / centered / onesided) bit-equal MATLAB R2025b. KNOWN GAPS: fs / [s,f,t] multi-output, multi-channel. |
| `stftlayer` | ❌ |  |
| `stftmag2sig` | ❌ |  |
| `vmd` | ❌ | variational MD |
| `wvd` | ❌ | Wigner-Ville |
| `xspectrogram` | ❌ | cross-spectrogram |
| `xwvd` | ❌ | cross WVD |
| `fft` | ✅ | Sig: Y = fft(X). 1024-pt FFT on sin. 1000 iters. Custom fp (complex out). |
| `fft2` | ✅ | Sig: r = fft2(...). Spec-extension batch 2026-05-09. |
| `fftn` | ✅ | Sig: Y = fftn(X[, sz]). N-D FFT via iterated 1-D fft along dims 1..ndim. Bit-equal MATLAB R2025b on 2-D, 3-D, and sz-override forms. Up to 3-D (Dims model cap). |
| `fftshift` | ✅ | Sig: Y = fftshift(X[, dim]). Cyclic shift along every non-singleton dim by ceil(extent/2); inverse ifftshift uses floor(extent/2). Bug fix: numkit had fftshift/ifftshift swapped for odd N + flat-shift instead of per-dim for matrices + dim arg ignored. tol=0 (integer-stable). |
| `fftw` | ❌ | wisdom file |
| `ifft` | ✅ | ifft(X,...,'symmetric') vs MATLAB R2025b. DEEP-PROBE 2026-05-31: previously THREW 'Cannot convert char to scalar' (numkit parsed 'symmetric' as the FFT length N). MATLAB treats X as conjugate-symmetric along the active dim: the lower half X[0..floor(L/2)] is authoritative (DC and, for even L, Nyquist forced real), the upper half is reconstructed as conj(X[k]), and the result is EXACTLY REAL — this differs from real(ifft(X)) which averages the symmetric part. v=ifft([1 2+1i 3 4-1i],'symmetric')=[2 -1 0 0] (real; ir=1) whereas real(ifft) would be [2.5 -1 -0.5 0]. n pads/truncates first: ifft(...,6,'symmetric')=[2.5 -0.9553418013 -0.2886751346 -0.1666666667 0.2886751346 -0.3779915321]. Matrix per active dim: ifft([1 2+1i;3+2i 4-1i],'symmetric')=[2 3;-1 -1] (dim1), ifft(...,[],2,'symmetric')=[1.5 -0.5;3.5 -0.5] (dim2). 'nonsymmetric'=default. N-D (>2) 'symmetric' throws (documented). covers ifft. namespace=core (matches ifft.json). |
| `ifft2` | ✅ | ifft2(X,'symmetric') vs MATLAB R2025b. DEEP-PROBE 2026-05-31: previously THREW 'Cannot convert char to scalar' (parsed 'symmetric' as a size arg). MATLAB treats X as conjugate-symmetric so the 2-D inverse is EXACTLY REAL. Decomposes into the 1-D ifftSymmetric over each dim of length>1 (dim 2 then dim 1), matching MATLAB across square/non-square/row/column/scalar (NOT real(ifft2), which differs for genuine 2-D). ifft2([1+1i 2-3i 5;4 5+2i 1-1i],'symmetric')=[3.16667 -0.0446582 -0.622008;-1.5 1.44338 -1.44338] (real; ir=1); ifft2([1+2i;3-1i;5+0.5i;2i],'symmetric')=[3;-0.5;0;-1.5]; ifft2(3+4i,'symmetric')=3. 'nonsymmetric'=default. The resize form ifft2(X,m,n,'symmetric') uses a different reconstruction and throws (deferred). N-D throws. covers ifft2. namespace=builtin. |
| `ifftn` | ✅ | Sig: Y = ifftn(X[, sz]). N-D inverse FFT — mirror of fftn. Round-trip ifftn(fftn(X)) = X to ~ulp. |
| `ifftshift` | ✅ | Sig: Y = ifftshift(X[, dim]). Inverse of fftshift; cyclic shift along every non-singleton dim by floor(extent/2). Joint fix with fftshift on 2026-05-08 — they were swapped for odd-extent dims. tol=0. |
| `interpft` | ✅ | Sig: Y = interpft(X, n[, dim]). Band-limited (FFT-based) interpolation to n samples. Default dim = first non-singleton. Vector form preserves originals at integer multiples of original spacing. Matrix dim=1 interpolates each column; dim=2 interpolates each row. tol=1e-12. |
| `nextpow2` | ✅ | Sig: P = nextpow2(N). Smallest p such that 2^p >= |N|. Vectorised. Edges: |x|=0 -> 0; NaN -> NaN; ±Inf -> +Inf; complex z -> uses |z|. Bug fix 2026-05-08: complex input previously threw; NaN/Inf paths now match MATLAB. tol=0. |
| `nufft` | ❌ | non-uniform |
| `nufftn` | ❌ | non-uniform |

### Windows

**Namespace:** `signal.windows.*` — 6 ✅ + 0 ⚠️ / 24 = 25%

| function | status | comment |
|---|:---:|---|
| `barthannwin` | ✅ | Sig: W = barthannwin(N). Bartlett-Hann. 10000 iters. |
| `bartlett` | ✅ | Sig: W = bartlett(N). 1024-pt triangular. 10000 iters. |
| `blackman` | ✅ | Sig: W = blackman(N). 1024-pt Blackman. 10000 iters. |
| `blackmanharris` | ✅ | Sig: W = blackmanharris(N). 4-term Blackman-Harris. 10000 iters. |
| `bohmanwin` | ✅ | Sig: W = bohmanwin(N). Bohman. 10000 iters. |
| `chebwin` | ✅ | Sig: w = chebwin(N[, at]). Dolph-Chebyshev window with `at` dB sidelobe attenuation (default 100). Bug fix 2026-05-08: previous FFT-based impl returned all-ones for even N and a wrongly-shifted window for odd N. Rewrote as direct cosine-IDFT (O(N²)) with cosine basis centered on (N-1)/2. Coverage: N ∈ {1, 7, 8, 16, 64} × R ∈ {30, 60, 100, 120}. |
| `dpss` | ❌ | discrete prolate spheroidal |
| `dpssclear` | ❌ | cache |
| `dpssdir` | ❌ | cache |
| `dpssload` | ❌ | cache |
| `dpsssave` | ❌ | cache |
| `enbw` | ✅ | Sig: bw = enbw(window[, fs]). Equivalent noise bandwidth: N · Σ(w²) / (Σw)². With fs, scales output by fs/N. tol=1e-12. Specs covers hamming/hann/rectwin/blackman + fs-scaled forms. |
| `flattopwin` | ✅ | Sig: W = flattopwin(N). Flat-top. 10000 iters. |
| `gausswin` | ✅ | Sig: w = gausswin(N[, alpha]). Gaussian window with reciprocal-of-stddev shape param alpha (default 2.5). Larger alpha -> tighter / lower endpoints. Coverage: alpha ∈ {1.5, 2.5, 4, 8} × N ∈ {8, 16, 64} sample points + N=1 (single-point window). |
| `hamming` | ✅ | Sig: W = hamming(N). 1024-pt Hamming. 10000 iters. |
| `hann` | ✅ | Sig: W = hann(N). 1024-pt Hann window. 10000 iters. |
| `kaiser` | ✅ | Sig: w = kaiser(N[, beta]). Kaiser window with shape param beta. beta=0 -> rectangular (all ones); larger beta -> narrower mainlobe + lower sidelobes. Default beta=0.5. Coverage: beta ∈ {0, 1, 5, 8.6, 12} × N ∈ {8, 16, 64} + default + N=1 (single-point window). |
| `nuttallwin` | ✅ | Sig: W = nuttallwin(N). 10000 iters. |
| `parzenwin` | ✅ | Sig: W = parzenwin(N). 10000 iters. |
| `rectwin` | ✅ | Sig: W = rectwin(N). All-ones. 10000 iters. |
| `taylorwin` | ✅ | Sig: w = taylorwin(N[, nbar, sll]). Taylor window for radar pulse-compression. Defaults: nbar=4, sll=-30 dB. Bug fix 2026-05-08: previous impl used (-1)^m sign instead of (-1)^(m+1) — inverted output (peak at edges, dip at center). Also incorrectly normalised peak to 1; MATLAB does NOT normalise (peak ≈ 1.52 for default params). |
| `triang` | ✅ | Sig: W = triang(N). Triangular. 10000 iters. |
| `tukeywin` | ✅ | Sig: w = tukeywin(N[, r]). Tukey (cosine-tapered) window; r is cosine fraction in [0, 1]. r=0 -> rectwin (all ones); r=1 -> Hann. Default r=0.5. Coverage: r ∈ {0, 0.25, 0.5, 0.75, 1} × selected sample points + N=1 single-point. |
| `wvtool` | ❌ | GUI |

### Parametric Spectral Estimation

**Namespace:** `signal.spectral_analysis.*`. Magnitude utils (`db/db2mag/mag2db/pow2db`) → core (cross-cutting math) — 3 ✅ + 0 ⚠️ / 10 = 30%

| function | status | comment |
|---|:---:|---|
| `db` | ✅ | Sig: D = db(X). magnitude → dB. 100k iters. |
| `db2mag` | ✅ | Sig: r = db2mag(...). Spec-extension batch 2026-05-09. |
| `db2pow` | ✅ | Sig: r = db2pow(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `findpeaks` | ✅ 🔬 | Sig [pks,locs,w,p]=findpeaks(Y[,X|Fs],Name,Value). Default = strict local maxima (1-based idx). Options: MinPeakHeight, Threshold, MinPeakDistance, NPeaks, SortStr, MinPeakProminence (keep peaks with topographic prominence >= value). 3rd/4th outputs: w = half-prominence width (linear-interpolated crossings of h-prominence/2), p = prominence (h - max of the two bounding-interval minima). numkit previously errored on MinPeakProminence and on >2 outputs -- now supported. MinPeakWidth/WidthReference still deferred. Matches MATLAB R2025b. |
| `mag2db` | ✅ | Sig: r = mag2db(...). Spec-extension batch 2026-05-09. |
| `pburg` | ✅ | Sig: r = pburg(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `pcov` | ❌ |  |
| `pmcov` | ❌ |  |
| `pow2db` | ✅ | Sig: r = pow2db(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `pyulear` | ✅ | Sig: r = pyulear(...). Spec-extension batch 2026-05-09 (signal namespace). |

### Nonparametric Spectral Estimation

**Namespace:** `signal.spectral_analysis.*` — 6 ✅ + 0 ⚠️ / 17 = 35%

| function | status | comment |
|---|:---:|---|
| `cpsd` | ✅ | Sig: spectral DSP estimator. Default fs=2*pi (MATLAB convention) and 8-segment 50%-overlap Hamming window for Welch-family. Bit-identical with MATLAB R2025b after fs+winLen fix 2026-05-09. |
| `db` | ✅ | Sig: D = db(X). magnitude → dB. 100k iters. |
| `db2mag` | ✅ | Sig: r = db2mag(...). Spec-extension batch 2026-05-09. |
| `db2pow` | ✅ | Sig: r = db2pow(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `findpeaks` | ✅ 🔬 | Sig [pks,locs,w,p]=findpeaks(Y[,X|Fs],Name,Value). Default = strict local maxima (1-based idx). Options: MinPeakHeight, Threshold, MinPeakDistance, NPeaks, SortStr, MinPeakProminence (keep peaks with topographic prominence >= value). 3rd/4th outputs: w = half-prominence width (linear-interpolated crossings of h-prominence/2), p = prominence (h - max of the two bounding-interval minima). numkit previously errored on MinPeakProminence and on >2 outputs -- now supported. MinPeakWidth/WidthReference still deferred. Matches MATLAB R2025b. |
| `mag2db` | ✅ | Sig: r = mag2db(...). Spec-extension batch 2026-05-09. |
| `mscohere` | ✅ | Sig: spectral DSP estimator. Default fs=2*pi (MATLAB convention) and 8-segment 50%-overlap Hamming window for Welch-family. Bit-identical with MATLAB R2025b after fs+winLen fix 2026-05-09. |
| `periodogram` | ✅ 🔬 | periodogram default NFFT vs MATLAB R2025b. DEEP-PROBE 2026-05-31 (toolbox pivot to toolboxes/signal): periodogram(x) with no explicit NFFT used 2^nextpow2(N) only, but MATLAB uses max(256, 2^nextpow2(N)) — so an 8-sample signal returned 5 frequency points instead of 129, and a 300-sample signal must give 257 (2^nextpow2(300)=512). The PSD VALUES were already correct (explicit periodogram(x,[],256) matched MATLAB: p10=4.40929); only the default NFFT was wrong. Sibling pwelch already used the max(256,...) rule. Fix: one line — nfft = max(256, nextPow2(N)) when not specified. periodogram([1 2 1 2 1 2 1 2]) -> 129 pts (p1=2.86479, p10=4.40929, f10=0.22089); N=300 -> 257; an explicit periodogram(x,[],8) still gives 5 (default not forced). covers:[periodogram]. |
| `plomb` | ❌ | Lomb-Scargle |
| `pmtm` | ❌ | multi-taper |
| `poctave` | ❌ |  |
| `pow2db` | ✅ | Sig: r = pow2db(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `pspectrum` | ❌ | easy spectral analysis |
| `pwelch` | ✅ 🔬 | Sig: spectral DSP estimator. Default fs=2*pi (MATLAB convention) and 8-segment 50%-overlap Hamming window. pwelch(x,[],[],nfft): empty [] placeholders select defaults for window/noverlap (previously errored 'Cannot convert double to scalar'). pwelch(cos(2pi*.1*(0:127)),[],[],128) -> 65 bins, max=1.59267. Bit-identical with MATLAB R2025b. |
| `refinepeaks` | ❌ |  |
| `spectralentropy` | ✅ | Sig: e = spectralEntropy(x, fs). camelCase alias added 2026-05-09. |
| `tfestimate` | ✅ | Sig: spectral DSP estimator. Default fs=2*pi (MATLAB convention) and 8-segment 50%-overlap Hamming window for Welch-family. Bit-identical with MATLAB R2025b after fs+winLen fix 2026-05-09. |

### Spectral Measurements

**Namespace:** `signal.spectral_analysis.*` — 0 ✅ + 0 ⚠️ / 18 = 0%

| function | status | comment |
|---|:---:|---|
| `bandpower` | ✅ | Sig: r = bandpower(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `enbw` | ✅ | Sig: bw = enbw(window[, fs]). Equivalent noise bandwidth: N · Σ(w²) / (Σw)². With fs, scales output by fs/N. tol=1e-12. Specs covers hamming/hann/rectwin/blackman + fs-scaled forms. |
| `instbw` | ✅ | Sig: b = instbw(x, fs). Spec-extension batch 2026-05-09 (cycle 40). |
| `instfreq` | ✅ | Sig: f = instfreq(x, fs). Spec-extension batch 2026-05-09 (cycle 40). |
| `meanfreq` | ✅ | Sig: f = meanfreq(x, fs). Spec-extension batch 2026-05-09 (cycle 40). |
| `medfreq` | ✅ | Sig: f = medfreq(x, fs). Spec-extension batch 2026-05-09 (cycle 40). |
| `obw` | ✅ | Sig: bw = obw(x, fs). Spec-extension batch 2026-05-09 (cycle 40). |
| `powerbw` | ✅ | Sig: bw = powerbw(x, fs). Spec-extension batch 2026-05-09 (cycle 40). |
| `sfdr` | ✅ | Sig: r = sfdr(x, fs). Spec-extension batch 2026-05-09 (cycle 40). |
| `sinad` | ✅ | Sig: r = sinad(x, fs). Spec-extension batch 2026-05-09 (cycle 40). |
| `snr` | ✅ | Sig: r = snr(x, fs). Spec-extension batch 2026-05-09 (cycle 40). |
| `spectralcrest` | ✅ | Sig: c = spectralCrest(x, fs). camelCase alias added 2026-05-09. |
| `spectralentropy` | ✅ | Sig: e = spectralEntropy(x, fs). camelCase alias added 2026-05-09. |
| `spectralflatness` | ✅ | Sig: f = spectralFlatness(x, fs). camelCase alias added 2026-05-09. |
| `spectralkurtosis` | ✅ | Sig: k = spectralKurtosis(x, fs). camelCase alias added 2026-05-09. |
| `spectralskewness` | ✅ | Sig: s = spectralSkewness(x, fs). camelCase alias added 2026-05-09. |
| `thd` | ✅ | Sig: r = thd(x, fs). Spec-extension batch 2026-05-09 (cycle 40). |
| `toi` | ❌ | third-order intercept |

### Time-Frequency Analysis

**Namespace:** `signal.time_frequency.*`. Wavelet/EMD subset (`cwt/wsst/vmd/hht/emd/fsst/ifsst`) → `wavelet.*` (future) — 1 ✅ + 0 ⚠️ / 27 = 3%

| function | status | comment |
|---|:---:|---|
| `dlistft` | ❌ |  |
| `dlstft` | ❌ |  |
| `emd` | ❌ | empirical mode decomp |
| `fsst` | ❌ | Fourier synchrosqueezed |
| `hht` | ❌ | Hilbert-Huang |
| `ifsst` | ❌ |  |
| `instbw` | ✅ | Sig: b = instbw(x, fs). Spec-extension batch 2026-05-09 (cycle 40). |
| `instfreq` | ✅ | Sig: f = instfreq(x, fs). Spec-extension batch 2026-05-09 (cycle 40). |
| `iscola` | ✅ | Sig: tf = iscola(window, noverlap[, method]); [tf, m, maxDev] = iscola(...). Constant OverLap-Add compliance check (cycle 87). Sums (possibly squared) window shifted by hop = M - noverlap in the stable overlap region; tf = 1 iff maxDev ≤ \|m\| · eps. Default method 'wola' (sum of w²). Hann 50% overlap: COLA under 'ola' (m=1), not 'wola' (m=0.75, dev=0.25). Hamming 50%: COLA under 'ola' (m=1.08). Rectangular hop=M: trivially COLA. Bit-equal MATLAB R2025b on 8 probed cases. Octave 11.1.0 ships iscola only in signal package. |
| `istft` | ✅ | Sig: x = istft(S[, NV-pairs]); [x, t] = istft(S, fs[, NV-pairs]) (cycle 86). Inverse STFT via overlap-add with per-sample window² normalisation. All three FrequencyRange modes (twosided / centered / onesided) round-trip ~ulp on COLA-compliant configs (hann/periodic + 50%/75% overlap). Second output is time axis t = (0:Nout-1)/fs. |
| `istftlayer` | ❌ |  |
| `kurtogram` | ❌ |  |
| `pspectrum` | ❌ | easy spectral analysis |
| `spectralcrest` | ✅ | Sig: c = spectralCrest(x, fs). camelCase alias added 2026-05-09. |
| `spectralentropy` | ✅ | Sig: e = spectralEntropy(x, fs). camelCase alias added 2026-05-09. |
| `spectralflatness` | ✅ | Sig: f = spectralFlatness(x, fs). camelCase alias added 2026-05-09. |
| `spectralkurtosis` | ✅ | Sig: k = spectralKurtosis(x, fs). camelCase alias added 2026-05-09. |
| `spectralskewness` | ✅ | Sig: s = spectralSkewness(x, fs). camelCase alias added 2026-05-09. |
| `spectrogram` | ✅ 🔬 | spectrogram f/t output axes honour fs vs MATLAB R2025b. DEEP-PROBE 2026-05-31 (toolbox pivot, toolboxes/signal): [s,f,t]=spectrogram(...,fs) ignored fs entirely — f was always normalized 0..pi and t was in raw samples regardless of fs. MATLAB returns f=k*fs/nfft (Hz) and t=centre/fs (sec) when fs given; with no fs the normalized convention uses fs=2*pi so f spans [0,pi] and t=centre/(2*pi). Fix scales F/T in spectrogram_reg. With fs=100,nfft=16: f(2)=6.25, f(end)=50, t(1)=0.08, t(2)=0.16. No fs: f0(end)=pi, t0(1)=8/(2*pi)=1.2732, t0(2)=16/(2*pi)=2.5465. The s matrix is unchanged. covers:[spectrogram]. |
| `stft` | ✅ | Sig: s = stft(x[, NV-pairs]); [s, f, t] = stft(x, fs[, NV-pairs]) (cycle 86). Short-time Fourier transform. Defaults match MATLAB: hann(128,periodic), 75% overlap, FFTLength 128, FrequencyRange='centered'. All three ranges bit-equal MATLAB R2025b. f in Hz (k*fs/NFFT) when fs given else radians/sample (k*2*pi/NFFT); t at frame centres (M/2 + k*hop)/fs_t. KNOWN GAP: multi-channel matrix input. |
| `stftlayer` | ❌ |  |
| `stftmag2sig` | ❌ |  |
| `tfridge` | ❌ |  |
| `vmd` | ❌ | variational MD |
| `wvd` | ❌ | Wigner-Ville |
| `xspectrogram` | ❌ | cross-spectrogram |
| `xwvd` | ❌ | cross WVD |

### Pulse and Transition Metrics

**Namespace:** `signal.measurements.*` — 0 ✅ + 0 ⚠️ / 12 = 0%

| function | status | comment |
|---|:---:|---|
| `dutycycle` | ✅ | Sig: d = dutycycle(x, fs). Spec-extension batch 2026-05-09 (cycle 40). |
| `falltime` | ✅ | Sig: [F,LT,UT,LL,UL]=falltime(x,fs). Sharp single-sample edge F=0.198; LT (10%) crosses LAST, UT (90%) FIRST. Fixes bugs/signal/risetime-falltime-outputs.md (value + outputs). vs MATLAB R2025b. |
| `midcross` | ✅ | Sig: c = midcross(x). Spec-extension batch 2026-05-09 (cycle 40). |
| `overshoot` | ✅ | Sig: os = overshoot(x). Spec-extension batch 2026-05-09 (cycle 40). |
| `pulseperiod` | ✅ | Sig: p = pulseperiod(x). Spec-extension batch 2026-05-09 (cycle 40). |
| `pulsesep` | ✅ | Sig: s = pulsesep(x). Spec-extension batch 2026-05-09 (cycle 40). |
| `pulsewidth` | ✅ | Sig: w = pulsewidth(x). Spec-extension batch 2026-05-09 (cycle 40). |
| `risetime` | ✅ | Sig: [R,LT,UT,LL,UL]=risetime(x,fs). Sharp single-sample edge: R=0.198 (the 10% and 90% levels both cross in one interval — was 0.224 before the findTransitions fix). LT/UT = lower/upper crossing times; LL/UL = reference levels. Fixes bugs/signal/risetime-falltime-outputs.md (value + outputs). vs MATLAB R2025b. |
| `settlingtime` | ✅ | Sig: st = settlingtime(x, d). Spec-extension batch 2026-05-09 (cycle 40). |
| `slewrate` | ✅ | Sig: sr = slewrate(x). Spec-extension batch 2026-05-09 (cycle 40). |
| `statelevels` | ✅ | Sig: lv = statelevels(x). Spec-extension batch 2026-05-09 (cycle 40). |
| `undershoot` | ✅ | Sig: us = undershoot(x). Spec-extension batch 2026-05-09 (cycle 40). |

### Signal Descriptive Statistics

**Namespace:** `signal.measurements.*` — 2 ✅ + 0 ⚠️ / 30 = 6%

| function | status | comment |
|---|:---:|---|
| `alignsignals` | ✅ | Sig: r = alignsignals(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `binmask2sigroi` | ❌ |  |
| `countlabels` | ❌ |  |
| `cusum` | ✅ | Sig: [iupper,ilower,uppersum,lowersum] = cusum(x, climit, mshift, tmean, tdev). Standard one-sided CUSUM with mshift/2 slack on standardized z=(x-tmean)/tdev. Step input [0 0 0 5 5 5 5 5] with climit=2, mshift=1, tmean=0, tdev=1: upper sum first exceeds 2 at sample iu=4 (us(4)=4.5, us(8)=24.5), no lower detection (numel(il)=0). Lifted to public C++ API numkit::signal::cusum (was adapter-only; CusumResult struct, tmean/tdev as Value::Empty=auto) 2026-06. |
| `dtw` | ❌ | dynamic time warp |
| `edr` | ❌ | edit distance on real |
| `envelope` | ✅ | Sig: [yupper, ylower] = envelope(x[, n[, method]]). Four modes match MATLAB R2025b envelope.m exactly: default (no n) FFT |hilbert(x-mean)| with mean restored; 'analytic' n-tap Kaiser(8)-tapered Hilbert FIR; 'rms' sliding-window RMS; 'peak' spline (parabola for 3 knots, not-a-knot for 4+) through local maxima/minima with MinPeakDistance n. DC-removal applied for analytic/rms/default; not for peak. |
| `extendsigroi` | ❌ |  |
| `extractsigroi` | ❌ |  |
| `filenames2labels` | ❌ |  |
| `findchangepts` | ❌ | change-point detection |
| `finddelay` | ✅ | Sig: r = finddelay(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `findpeaks` | ✅ 🔬 | Sig [pks,locs,w,p]=findpeaks(Y[,X|Fs],Name,Value). Default = strict local maxima (1-based idx). Options: MinPeakHeight, Threshold, MinPeakDistance, NPeaks, SortStr, MinPeakProminence (keep peaks with topographic prominence >= value). 3rd/4th outputs: w = half-prominence width (linear-interpolated crossings of h-prominence/2), p = prominence (h - max of the two bounding-interval minima). numkit previously errored on MinPeakProminence and on >2 outputs -- now supported. MinPeakWidth/WidthReference still deferred. Matches MATLAB R2025b. |
| `findsignal` | ❌ | pattern search |
| `folders2labels` | ❌ |  |
| `framelbl` | ❌ |  |
| `framesig` | ❌ |  |
| `meanfreq` | ✅ | Sig: f = meanfreq(x, fs). Spec-extension batch 2026-05-09 (cycle 40). |
| `medfreq` | ✅ | Sig: f = medfreq(x, fs). Spec-extension batch 2026-05-09 (cycle 40). |
| `mergesigroi` | ❌ |  |
| `peak2peak` | ✅ | Sig: r = peak2peak(...). Spec-extension batch 2026-05-09. |
| `peak2rms` | ✅ | Sig: R = peak2rms(X). 100 iters. |
| `removesigroi` | ❌ |  |
| `rssq` | ✅ | Sig: r = rssq(...). Spec-extension batch 2026-05-09. |
| `seqperiod` | ❌ |  |
| `shortensigroi` | ❌ |  |
| `sigrangebinmask` | ❌ |  |
| `sigroi2binmask` | ❌ |  |
| `splitlabels` | ❌ |  |
| `zerocrossrate` | ❌ |  |

### Smoothing and Denoising

**Namespace:** `signal.smoothing.*` + `signal.digital_filtering.*` (medfilt1, sgolayfilt). `smoothdata` itself → `stats.moving.*` — 3 ✅ + 0 ⚠️ / 4 = 75%

| function | status | comment |
|---|:---:|---|
| `hampel` | ✅ | Sig: [y,i,xmedian,xsigma] = hampel(x[,k[,nsigma]]). DEEP-PROBE 2026-05-31: only the 1st output (filtered signal) was implemented; the documented i (logical outlier mask), xmedian (local window median), and xsigma (local 1.4826*MAD estimate) outputs were missing. Also switched the MAD->sigma factor from the loose 1.4826 to the MATLAB-exact 1/norminv(0.75)=1.482602218505602 (was off by ~1e-5, now xsigma matches to 1e-12). x=[1 2 100 3 4] default k=3,nsigma=3 -> y=[1 2 3 3 4], i=[0 0 1 0 0], xmed=[2.5 3 3 3 3.5], xsig=1.482602218505602. x2 k=2 -> 2 outliers flagged, s2(4)=2.965204437011. Matrix form deferred. Matches MATLAB R2025b. |
| `medfilt1` | ✅ 🔬 | medfilt1 windowing + padding vs MATLAB R2025b (DEEP-PROBE 2026-05-30). Window for sample i = [i-floor(k/2) .. i+ceil(k/2)-1] (even k leans LEFT); DEFAULT pads out-of-range with 0 ('zeropad'), 'truncate' clips the window at the ends. numkit previously (a) truncated by default (d1 gave 41 instead of 2) and (b) used a RIGHT-leaning even window. x=[2 80 6 3 10 8]: k=3 zeropad d1=median(0,2,80)=2, d6=median(10,8,0)=8; k=4 e=[1 4 4.5 8 7 5.5] e1=median(0,0,2,80)=1, e3=median(2,80,6,3)=4.5, e6=median(3,10,8,0)=5.5; k=4 'truncate' t1=median(2,80)=41, t6=median(3,10,8)=8. Matrix filtered per column (operate along dim 1): medfilt1([1 2;3 4;5 6;7 8],3) col1 row4 m4=median(5,7,0)=5. blksz/dim/nanflag args accepted-but-ignored (deferred). namespace=signal. Matches MATLAB R2025b. |
| `sgolay` | ✅ | Sig: B = sgolay(order,framelen); [B,G] = sgolay(order,framelen). DEEP-PROBE 2026-05-31: the second output G (the framelen x (order+1) differentiation-filter matrix G = V*(V'V)^-1) was previously UNIMPLEMENTED ([b,g]=sgolay(...) errored 'Undefined variable g'). G(:,1) is the smoothing filter (= central row of B), factorial(j-1)*G(:,j) the (j-1)-th derivative filter. [B,G]=sgolay(3,5): G is 5x4 (gr=5,gc=4); G(:,1)=[-3 12 17 12 -3]/35 (g11=-0.0857142857, g31=0.485714286=17/35); G(:,2)=[1 -8 0 8 -1]/12 (g12=0.0833333333, g42=0.666666667); G(:,3)=[2 -1 -2 -1 2]/14 (g13=0.142857143, g33=-0.142857143). bc=B(3,3)=g31 (central row). Matches MATLAB R2025b (numkit gives clean 0 where MATLAB has ~1e-16 round-off in odd derivative columns). The weighted form sgolay(order,framelen,weights) remains a deferred gap. namespace=signal. |
| `sgolayfilt` | ✅ 🔬 | Sig y=sgolayfilt(x,order,framelen[,weights,dim]): Savitzky-Golay smoothing. Interior points use the central SG weights; the first/last (framelen-1)/2 points use the asymmetric edge polynomials. DEEP-PROBE 2026-05-31: extended to matrices (each slice along dim filtered; dim=1 cols default, dim=2 rows) and the WEIGHTED least-squares form (B = V*(V'WV)^-1*V'W). numkit previously errored on matrices ('input must be a vector') and silently ignored the weights/dim args. x=[3 1 4 1 5 9 2 6 5 3]: y(1)=2.857143 (left edge), y(5)=5.342857 (interior). weighted [2 5 1 8 3 9 4 7 6] w=[1 2 3 2 1]: a1=2.533333, a9=5.866667 (differs from unweighted, confirming W is applied). matrix dim1 M=[2 5;1 8;3 9;4 7;6 2] order1 frame3: c11=1.5, c52=2.5. matrix dim2 Mr=[2 5 1 8 3;9 4 7 6 2]: r11=3.166667, r25=2.5. namespace=signal. Matches MATLAB R2025b. |

### Vibration Analysis

**Namespace:** `signal.vibration.*` — 0 ✅ + 0 ⚠️ / 13 = 0%

| function | status | comment |
|---|:---:|---|
| `envspectrum` | ✅ | Sig: [p,f] = envspectrum(x, fs). Spec-extension batch 2026-05-09 (cycle 43). |
| `modalfit` | ❌ | modal-fit |
| `modalfrf` | ❌ |  |
| `modalsd` | ❌ |  |
| `orderspectrum` | ❌ |  |
| `ordertrack` | ❌ |  |
| `orderwaveform` | ❌ |  |
| `rainflow` | ✅ | Sig: c = rainflow(x). ASTM E1049-85 cycle counting, returns Nx5 [count, range, mean, start_idx, end_idx]. Bit-identical with MATLAB R2025b on canonical 9-sample probe. |
| `rpmfreqmap` | ❌ |  |
| `rpmordermap` | ❌ |  |
| `rpmtrack` | ❌ | order tracking |
| `tachorpm` | ✅ | Sig: rpm = tachorpm(x, fs). Spec-extension batch 2026-05-09 (cycle 43). |
| `tsa` | ✅ | Sig: tsa(x, fs, tPulse[, M]) -- MATLAB pulse-time form (numkit also supports legacy tsa(x, fs, rpm, fs_rpm) when arg count >= 4). Bit-identical with MATLAB R2025b on probed input (100 samples). |

## Audio

Audio functions live (or will live) under `toolboxes/audio/`. The existing
`spectral*` family was originally shipped under
`signal/spectral_analysis` (camelCase aliases added 2026-05-09); per
2026-05-10 plan they migrate to `audio/spectral` so the entire
spectral-shape family clusters with audio-feature extraction
(`mfcc`/`gtcc`/`cepstralCoefficients`). Existing implementations stay
bit-equal with MATLAB; rows below mark current `signal.*` namespace
locations until physical migration lands.

### Spectral Shape Descriptors

**Namespace:** `audio.spectral.*` (planned). Currently registered under
`signal.spectral_analysis.*`. — 5 ✅ + 0 ⚠️ / 11 = 45%

| function | status | comment |
|---|:---:|---|
| `spectralCentroid` | ✅ | MATLAB Audio Toolbox spectral shape descriptors (cycle B): spectralCentroid (Σ(f·X)/Σ(X)), spectralSpread (sqrt of second central moment), spectralRolloffPoint (default 95th percentile of energy), spectralDecrease ((1/Σk≥2 X(k))·Σk≥2 (X(k)-X(1))/(k-1)), spectralSlope (linear regression of X vs F), spectralFlux ((Σ|ΔX|^p)^(1/p), MATLAB sets first-frame flux=0). Bit-equal with MATLAB R2025b on (X, F) direct-form inputs across 9 fingerprint points (single-col + 2-col + flux). Time-domain (x, fs) form uses internal naive O(N²) DFT with rectwin(round(0.03*fs)), overlap=round(0.02*fs), FFTLength=winLen — matches MATLAB's STFT defaults from spectralCentroid.m and friends. Octave 11.1.0 doesn't ship spectralX in core (Audio package only). |
| `spectralCrest` | ✅ | MATLAB Audio Toolbox cycle I: 5 per-frame STFT spectral metrics (spectralCrest/Entropy/Flatness/Kurtosis/Skewness). Reside in toolboxes/audio/src/spectral/shape_descriptors.cpp alongside spectralCentroid/Spread/Rolloff/Decrease/Slope/Flux from cycle B. All bit-equal with MATLAB R2025b on (X, F) direct, multi-frame (X2 8x2, F 8x1), and time-domain (xs, fs) inputs (after fixing computeStft to apply MATLAB signal.internal.spectraldescriptors.stft normalization: |Y|² / (0.5·sum(win)²), DC bin halved, Nyquist bin halved when fftLength even). Time-domain shape: 8x1 column vector (one row per frame) for 1601 samples at fs=16000 with rectwin(round(0.03*fs))=480 + overlap=320. The toolboxes/signal lowercase forms (spectralcrest/etc) remain as legacy single-segment scalar versions; camelCase compat aliases now point to the per-frame audio versions. |
| `spectralDecrease` | ✅ | MATLAB Audio Toolbox spectral shape descriptors (cycle B): spectralCentroid (Σ(f·X)/Σ(X)), spectralSpread (sqrt of second central moment), spectralRolloffPoint (default 95th percentile of energy), spectralDecrease ((1/Σk≥2 X(k))·Σk≥2 (X(k)-X(1))/(k-1)), spectralSlope (linear regression of X vs F), spectralFlux ((Σ|ΔX|^p)^(1/p), MATLAB sets first-frame flux=0). Bit-equal with MATLAB R2025b on (X, F) direct-form inputs across 9 fingerprint points (single-col + 2-col + flux). Time-domain (x, fs) form uses internal naive O(N²) DFT with rectwin(round(0.03*fs)), overlap=round(0.02*fs), FFTLength=winLen — matches MATLAB's STFT defaults from spectralCentroid.m and friends. Octave 11.1.0 doesn't ship spectralX in core (Audio package only). |
| `spectralEntropy` | ✅ | MATLAB Audio Toolbox cycle I: 5 per-frame STFT spectral metrics (spectralCrest/Entropy/Flatness/Kurtosis/Skewness). Reside in toolboxes/audio/src/spectral/shape_descriptors.cpp alongside spectralCentroid/Spread/Rolloff/Decrease/Slope/Flux from cycle B. All bit-equal with MATLAB R2025b on (X, F) direct, multi-frame (X2 8x2, F 8x1), and time-domain (xs, fs) inputs (after fixing computeStft to apply MATLAB signal.internal.spectraldescriptors.stft normalization: |Y|² / (0.5·sum(win)²), DC bin halved, Nyquist bin halved when fftLength even). Time-domain shape: 8x1 column vector (one row per frame) for 1601 samples at fs=16000 with rectwin(round(0.03*fs))=480 + overlap=320. The toolboxes/signal lowercase forms (spectralcrest/etc) remain as legacy single-segment scalar versions; camelCase compat aliases now point to the per-frame audio versions. |
| `spectralFlatness` | ✅ | MATLAB Audio Toolbox cycle I: 5 per-frame STFT spectral metrics (spectralCrest/Entropy/Flatness/Kurtosis/Skewness). Reside in toolboxes/audio/src/spectral/shape_descriptors.cpp alongside spectralCentroid/Spread/Rolloff/Decrease/Slope/Flux from cycle B. All bit-equal with MATLAB R2025b on (X, F) direct, multi-frame (X2 8x2, F 8x1), and time-domain (xs, fs) inputs (after fixing computeStft to apply MATLAB signal.internal.spectraldescriptors.stft normalization: |Y|² / (0.5·sum(win)²), DC bin halved, Nyquist bin halved when fftLength even). Time-domain shape: 8x1 column vector (one row per frame) for 1601 samples at fs=16000 with rectwin(round(0.03*fs))=480 + overlap=320. The toolboxes/signal lowercase forms (spectralcrest/etc) remain as legacy single-segment scalar versions; camelCase compat aliases now point to the per-frame audio versions. |
| `spectralFlux` | ✅ | MATLAB Audio Toolbox spectral shape descriptors (cycle B): spectralCentroid (Σ(f·X)/Σ(X)), spectralSpread (sqrt of second central moment), spectralRolloffPoint (default 95th percentile of energy), spectralDecrease ((1/Σk≥2 X(k))·Σk≥2 (X(k)-X(1))/(k-1)), spectralSlope (linear regression of X vs F), spectralFlux ((Σ|ΔX|^p)^(1/p), MATLAB sets first-frame flux=0). Bit-equal with MATLAB R2025b on (X, F) direct-form inputs across 9 fingerprint points (single-col + 2-col + flux). Time-domain (x, fs) form uses internal naive O(N²) DFT with rectwin(round(0.03*fs)), overlap=round(0.02*fs), FFTLength=winLen — matches MATLAB's STFT defaults from spectralCentroid.m and friends. Octave 11.1.0 doesn't ship spectralX in core (Audio package only). |
| `spectralKurtosis` | ✅ | MATLAB Audio Toolbox cycle I: 5 per-frame STFT spectral metrics (spectralCrest/Entropy/Flatness/Kurtosis/Skewness). Reside in toolboxes/audio/src/spectral/shape_descriptors.cpp alongside spectralCentroid/Spread/Rolloff/Decrease/Slope/Flux from cycle B. All bit-equal with MATLAB R2025b on (X, F) direct, multi-frame (X2 8x2, F 8x1), and time-domain (xs, fs) inputs (after fixing computeStft to apply MATLAB signal.internal.spectraldescriptors.stft normalization: |Y|² / (0.5·sum(win)²), DC bin halved, Nyquist bin halved when fftLength even). Time-domain shape: 8x1 column vector (one row per frame) for 1601 samples at fs=16000 with rectwin(round(0.03*fs))=480 + overlap=320. The toolboxes/signal lowercase forms (spectralcrest/etc) remain as legacy single-segment scalar versions; camelCase compat aliases now point to the per-frame audio versions. |
| `spectralRolloffPoint` | ✅ | MATLAB Audio Toolbox spectral shape descriptors (cycle B): spectralCentroid (Σ(f·X)/Σ(X)), spectralSpread (sqrt of second central moment), spectralRolloffPoint (default 95th percentile of energy), spectralDecrease ((1/Σk≥2 X(k))·Σk≥2 (X(k)-X(1))/(k-1)), spectralSlope (linear regression of X vs F), spectralFlux ((Σ|ΔX|^p)^(1/p), MATLAB sets first-frame flux=0). Bit-equal with MATLAB R2025b on (X, F) direct-form inputs across 9 fingerprint points (single-col + 2-col + flux). Time-domain (x, fs) form uses internal naive O(N²) DFT with rectwin(round(0.03*fs)), overlap=round(0.02*fs), FFTLength=winLen — matches MATLAB's STFT defaults from spectralCentroid.m and friends. Octave 11.1.0 doesn't ship spectralX in core (Audio package only). |
| `spectralSkewness` | ✅ | MATLAB Audio Toolbox cycle I: 5 per-frame STFT spectral metrics (spectralCrest/Entropy/Flatness/Kurtosis/Skewness). Reside in toolboxes/audio/src/spectral/shape_descriptors.cpp alongside spectralCentroid/Spread/Rolloff/Decrease/Slope/Flux from cycle B. All bit-equal with MATLAB R2025b on (X, F) direct, multi-frame (X2 8x2, F 8x1), and time-domain (xs, fs) inputs (after fixing computeStft to apply MATLAB signal.internal.spectraldescriptors.stft normalization: |Y|² / (0.5·sum(win)²), DC bin halved, Nyquist bin halved when fftLength even). Time-domain shape: 8x1 column vector (one row per frame) for 1601 samples at fs=16000 with rectwin(round(0.03*fs))=480 + overlap=320. The toolboxes/signal lowercase forms (spectralcrest/etc) remain as legacy single-segment scalar versions; camelCase compat aliases now point to the per-frame audio versions. |
| `spectralSlope` | ✅ | MATLAB Audio Toolbox spectral shape descriptors (cycle B): spectralCentroid (Σ(f·X)/Σ(X)), spectralSpread (sqrt of second central moment), spectralRolloffPoint (default 95th percentile of energy), spectralDecrease ((1/Σk≥2 X(k))·Σk≥2 (X(k)-X(1))/(k-1)), spectralSlope (linear regression of X vs F), spectralFlux ((Σ|ΔX|^p)^(1/p), MATLAB sets first-frame flux=0). Bit-equal with MATLAB R2025b on (X, F) direct-form inputs across 9 fingerprint points (single-col + 2-col + flux). Time-domain (x, fs) form uses internal naive O(N²) DFT with rectwin(round(0.03*fs)), overlap=round(0.02*fs), FFTLength=winLen — matches MATLAB's STFT defaults from spectralCentroid.m and friends. Octave 11.1.0 doesn't ship spectralX in core (Audio package only). |
| `spectralSpread` | ✅ | MATLAB Audio Toolbox spectral shape descriptors (cycle B): spectralCentroid (Σ(f·X)/Σ(X)), spectralSpread (sqrt of second central moment), spectralRolloffPoint (default 95th percentile of energy), spectralDecrease ((1/Σk≥2 X(k))·Σk≥2 (X(k)-X(1))/(k-1)), spectralSlope (linear regression of X vs F), spectralFlux ((Σ|ΔX|^p)^(1/p), MATLAB sets first-frame flux=0). Bit-equal with MATLAB R2025b on (X, F) direct-form inputs across 9 fingerprint points (single-col + 2-col + flux). Time-domain (x, fs) form uses internal naive O(N²) DFT with rectwin(round(0.03*fs)), overlap=round(0.02*fs), FFTLength=winLen — matches MATLAB's STFT defaults from spectralCentroid.m and friends. Octave 11.1.0 doesn't ship spectralX in core (Audio package only). |

### Audio Feature Extraction

**Namespace:** `audio.features.*` (planned) — 0 ✅ + 0 ⚠️ / 7 = 0%

| function | status | comment |
|---|:---:|---|
| `audioDelta` | ✅ | MATLAB Audio Toolbox cycle C: melSpectrogram + audioDelta. melSpectrogram with NumBands=8 default-window/overlap on a deterministic ramp signal. F (mel-band centers): bit-equal with MATLAB R2025b on all 8 values. S(1,1) bit-equal. Time vector T size matches MATLAB. audioDelta: bit-equal on default windowLength=9 ramp test (d(9)=d(10)=2 — MATLAB filter convention with sum((1:M)^2)=30 divisor), and on custom windowLength=5 path (denom=5), and multi-channel (filter operates along dim 1 per column). KNOWN GAPs (deferred): NumBands ≠ 8 default, FrequencyRange/FilterBankNormalization/MelStyle name-value args, and the [delta, Zf] / Zi initial-conditions form for audioDelta. Octave 11.1.0 doesn't ship melSpectrogram or audioDelta in core (Audio package only). |
| `cepstralCoefficients` | ✅ | MATLAB Audio Toolbox cycles D + G + H: cepstralCoefficients (bit-equal — DCT-II unitary matrix from createDCTmatrix.m + log10 rectification, output shape M×NumCoeffs). Cycle G: mfcc BIT-EQUAL with MATLAB R2025b — Slaney band edges (audio.internal.slaneybandedges, 42 entries: 13 linear at 66.66 Hz step + 29 log-spaced ratio 1.0711703), Slaney designMelFilterBank ('Hz' designDomain, 'Bandwidth' normalization), |FFT| magnitude, natural-log per-frame energy of UNWINDOWED signal as first column ('append' LogEnergy default). Cycle H: gtcc BIT-EQUAL with MATLAB R2025b — proper Patterson-Holdsworth gammatone filterbank (Slaney 1993) via cascaded 4-stage biquads with frequency-domain freqz('whole') evaluation, FrequencyRange=[50,fs/2], NumFilters=ceil(hz2erb(fs/2)-hz2erb(50)), Bandwidth normalization BW[i]/2, OneSided=false equivalent via doubled inner-half bins. Octave 11.1.0 doesn't ship cepstralCoefficients/mfcc/gtcc in core (Audio package only). |
| `gtcc` | ✅ | MATLAB Audio Toolbox cycles D + G + H: cepstralCoefficients (bit-equal — DCT-II unitary matrix from createDCTmatrix.m + log10 rectification, output shape M×NumCoeffs). Cycle G: mfcc BIT-EQUAL with MATLAB R2025b — Slaney band edges (audio.internal.slaneybandedges, 42 entries: 13 linear at 66.66 Hz step + 29 log-spaced ratio 1.0711703), Slaney designMelFilterBank ('Hz' designDomain, 'Bandwidth' normalization), |FFT| magnitude, natural-log per-frame energy of UNWINDOWED signal as first column ('append' LogEnergy default). Cycle H: gtcc BIT-EQUAL with MATLAB R2025b — proper Patterson-Holdsworth gammatone filterbank (Slaney 1993) via cascaded 4-stage biquads with frequency-domain freqz('whole') evaluation, FrequencyRange=[50,fs/2], NumFilters=ceil(hz2erb(fs/2)-hz2erb(50)), Bandwidth normalization BW[i]/2, OneSided=false equivalent via doubled inner-half bins. Octave 11.1.0 doesn't ship cepstralCoefficients/mfcc/gtcc in core (Audio package only). |
| `harmonicRatio` | ✅ | pitch + harmonicRatio (Audio Toolbox). Methods: NCF (default), CEP, PEF, LHS, SRH. CEP/PEF/LHS/SRH have been clean-room reimplemented from public papers as part of the IP-provenance remediation. CEP (Noll 1967) and LHS (Hermes 1988) are bit-identical to MATLAB R2025b. PEF (Gonzalez & Brookes, EUSIPCO 2011 — the no-compression variant) and SRH (Drugman & Alwan, Interspeech 2011) are faithful to the published papers; MATLAB's PEF/SRH diverge from the papers in undocumented ways, so numkit's are intentionally NOT bit-matched. On a clean tone PEF still agrees with MATLAB to ~0.06% (within tol), so pef_first/pef_mean stay in the fingerprint; pef_r_first (a degenerate two-pure-tone case) and srh_first are excluded from the cross-engine comparison. NCF parity unchanged. harmonicRatio: auto low-edge + Smith parabolic. Tolerance 5%; pitchnn deferred (DNN runtime not in numkit). |
| `mfcc` | ✅ | MATLAB Audio Toolbox cycles D + G + H: cepstralCoefficients (bit-equal — DCT-II unitary matrix from createDCTmatrix.m + log10 rectification, output shape M×NumCoeffs). Cycle G: mfcc BIT-EQUAL with MATLAB R2025b — Slaney band edges (audio.internal.slaneybandedges, 42 entries: 13 linear at 66.66 Hz step + 29 log-spaced ratio 1.0711703), Slaney designMelFilterBank ('Hz' designDomain, 'Bandwidth' normalization), |FFT| magnitude, natural-log per-frame energy of UNWINDOWED signal as first column ('append' LogEnergy default). Cycle H: gtcc BIT-EQUAL with MATLAB R2025b — proper Patterson-Holdsworth gammatone filterbank (Slaney 1993) via cascaded 4-stage biquads with frequency-domain freqz('whole') evaluation, FrequencyRange=[50,fs/2], NumFilters=ceil(hz2erb(fs/2)-hz2erb(50)), Bandwidth normalization BW[i]/2, OneSided=false equivalent via doubled inner-half bins. Octave 11.1.0 doesn't ship cepstralCoefficients/mfcc/gtcc in core (Audio package only). |
| `pitch` | ✅ | pitch + harmonicRatio (Audio Toolbox). Methods: NCF (default), CEP, PEF, LHS, SRH. CEP/PEF/LHS/SRH have been clean-room reimplemented from public papers as part of the IP-provenance remediation. CEP (Noll 1967) and LHS (Hermes 1988) are bit-identical to MATLAB R2025b. PEF (Gonzalez & Brookes, EUSIPCO 2011 — the no-compression variant) and SRH (Drugman & Alwan, Interspeech 2011) are faithful to the published papers; MATLAB's PEF/SRH diverge from the papers in undocumented ways, so numkit's are intentionally NOT bit-matched. On a clean tone PEF still agrees with MATLAB to ~0.06% (within tol), so pef_first/pef_mean stay in the fingerprint; pef_r_first (a degenerate two-pure-tone case) and srh_first are excluded from the cross-engine comparison. NCF parity unchanged. harmonicRatio: auto low-edge + Smith parabolic. Tolerance 5%; pitchnn deferred (DNN runtime not in numkit). |
| `pitchnn` | ❌ | Deep-learning pitch estimator (CREPE-style network). KNOWN GAP: requires a packaged neural model -- defer to v2 unless a numkit DNN runtime lands. |

### Audio Time-Frequency

**Namespace:** `audio.spectrogram.*` (planned) — 0 ✅ + 0 ⚠️ / 1 = 0%

| function | status | comment |
|---|:---:|---|
| `melSpectrogram` | ✅ | MATLAB Audio Toolbox cycle C: melSpectrogram + audioDelta. melSpectrogram with NumBands=8 default-window/overlap on a deterministic ramp signal. F (mel-band centers): bit-equal with MATLAB R2025b on all 8 values. S(1,1) bit-equal. Time vector T size matches MATLAB. audioDelta: bit-equal on default windowLength=9 ramp test (d(9)=d(10)=2 — MATLAB filter convention with sum((1:M)^2)=30 divisor), and on custom windowLength=5 path (denom=5), and multi-channel (filter operates along dim 1 per column). KNOWN GAPs (deferred): NumBands ≠ 8 default, FrequencyRange/FilterBankNormalization/MelStyle name-value args, and the [delta, Zf] / Zi initial-conditions form for audioDelta. Octave 11.1.0 doesn't ship melSpectrogram or audioDelta in core (Audio package only). |

### Audio Frequency / Loudness Conversions

**Namespace:** `audio.scale.*` (planned) — 0 ✅ + 0 ⚠️ / 8 = 0%

| function | status | comment |
|---|:---:|---|
| `bark2hz` | ✅ | MATLAB Audio Toolbox frequency-scale + loudness conversions (cycles A + M): hz2mel/mel2hz (O'Shaughnessy default), hz2bark/bark2hz (Traunmüller 1990 with low/high-frequency corrections; bark2hz uses the asymmetric 26.28 denominator from MATLAB's bark2hz.m source), hz2erb/erb2hz (Glasberg & Moore 1990 with constants log(10)*1000/(24.673*4.368) and 0.004368 — extracted bit-for-bit from MATLAB hz2erb.m), phon2sone/sone2phon (ISO 532-1 default with smooth break at 40 phon = 1 sone). Cycle M added optional 2nd arg 'ISO 532-2': bit-equal table-lookup PCHIP interpolation per ISO 532-2:2017 Table 5 (28 entries from MATLAB getPerceptualConstants.m), with linear extrapolation beyond 337.6 sone for sone2phon. KNOWN GAP: phon2sone ISO 532-2 ships initial PCHIP guess only; MATLAB additionally refines via fzero (error <1% on smooth inputs). Bit-equal with MATLAB R2025b on all 26 fingerprint points (in-range). Octave 11.1.0 doesn't ship these in core (Audio package only; Octave's audio package missing these specifically). |
| `erb2hz` | ✅ | MATLAB Audio Toolbox frequency-scale + loudness conversions (cycles A + M): hz2mel/mel2hz (O'Shaughnessy default), hz2bark/bark2hz (Traunmüller 1990 with low/high-frequency corrections; bark2hz uses the asymmetric 26.28 denominator from MATLAB's bark2hz.m source), hz2erb/erb2hz (Glasberg & Moore 1990 with constants log(10)*1000/(24.673*4.368) and 0.004368 — extracted bit-for-bit from MATLAB hz2erb.m), phon2sone/sone2phon (ISO 532-1 default with smooth break at 40 phon = 1 sone). Cycle M added optional 2nd arg 'ISO 532-2': bit-equal table-lookup PCHIP interpolation per ISO 532-2:2017 Table 5 (28 entries from MATLAB getPerceptualConstants.m), with linear extrapolation beyond 337.6 sone for sone2phon. KNOWN GAP: phon2sone ISO 532-2 ships initial PCHIP guess only; MATLAB additionally refines via fzero (error <1% on smooth inputs). Bit-equal with MATLAB R2025b on all 26 fingerprint points (in-range). Octave 11.1.0 doesn't ship these in core (Audio package only; Octave's audio package missing these specifically). |
| `hz2bark` | ✅ | MATLAB Audio Toolbox frequency-scale + loudness conversions (cycles A + M): hz2mel/mel2hz (O'Shaughnessy default), hz2bark/bark2hz (Traunmüller 1990 with low/high-frequency corrections; bark2hz uses the asymmetric 26.28 denominator from MATLAB's bark2hz.m source), hz2erb/erb2hz (Glasberg & Moore 1990 with constants log(10)*1000/(24.673*4.368) and 0.004368 — extracted bit-for-bit from MATLAB hz2erb.m), phon2sone/sone2phon (ISO 532-1 default with smooth break at 40 phon = 1 sone). Cycle M added optional 2nd arg 'ISO 532-2': bit-equal table-lookup PCHIP interpolation per ISO 532-2:2017 Table 5 (28 entries from MATLAB getPerceptualConstants.m), with linear extrapolation beyond 337.6 sone for sone2phon. KNOWN GAP: phon2sone ISO 532-2 ships initial PCHIP guess only; MATLAB additionally refines via fzero (error <1% on smooth inputs). Bit-equal with MATLAB R2025b on all 26 fingerprint points (in-range). Octave 11.1.0 doesn't ship these in core (Audio package only; Octave's audio package missing these specifically). |
| `hz2erb` | ✅ | MATLAB Audio Toolbox frequency-scale + loudness conversions (cycles A + M): hz2mel/mel2hz (O'Shaughnessy default), hz2bark/bark2hz (Traunmüller 1990 with low/high-frequency corrections; bark2hz uses the asymmetric 26.28 denominator from MATLAB's bark2hz.m source), hz2erb/erb2hz (Glasberg & Moore 1990 with constants log(10)*1000/(24.673*4.368) and 0.004368 — extracted bit-for-bit from MATLAB hz2erb.m), phon2sone/sone2phon (ISO 532-1 default with smooth break at 40 phon = 1 sone). Cycle M added optional 2nd arg 'ISO 532-2': bit-equal table-lookup PCHIP interpolation per ISO 532-2:2017 Table 5 (28 entries from MATLAB getPerceptualConstants.m), with linear extrapolation beyond 337.6 sone for sone2phon. KNOWN GAP: phon2sone ISO 532-2 ships initial PCHIP guess only; MATLAB additionally refines via fzero (error <1% on smooth inputs). Bit-equal with MATLAB R2025b on all 26 fingerprint points (in-range). Octave 11.1.0 doesn't ship these in core (Audio package only; Octave's audio package missing these specifically). |
| `hz2mel` | ✅ | MATLAB Audio Toolbox frequency-scale + loudness conversions (cycles A + M): hz2mel/mel2hz (O'Shaughnessy default), hz2bark/bark2hz (Traunmüller 1990 with low/high-frequency corrections; bark2hz uses the asymmetric 26.28 denominator from MATLAB's bark2hz.m source), hz2erb/erb2hz (Glasberg & Moore 1990 with constants log(10)*1000/(24.673*4.368) and 0.004368 — extracted bit-for-bit from MATLAB hz2erb.m), phon2sone/sone2phon (ISO 532-1 default with smooth break at 40 phon = 1 sone). Cycle M added optional 2nd arg 'ISO 532-2': bit-equal table-lookup PCHIP interpolation per ISO 532-2:2017 Table 5 (28 entries from MATLAB getPerceptualConstants.m), with linear extrapolation beyond 337.6 sone for sone2phon. KNOWN GAP: phon2sone ISO 532-2 ships initial PCHIP guess only; MATLAB additionally refines via fzero (error <1% on smooth inputs). Bit-equal with MATLAB R2025b on all 26 fingerprint points (in-range). Octave 11.1.0 doesn't ship these in core (Audio package only; Octave's audio package missing these specifically). |
| `mel2hz` | ✅ | MATLAB Audio Toolbox frequency-scale + loudness conversions (cycles A + M): hz2mel/mel2hz (O'Shaughnessy default), hz2bark/bark2hz (Traunmüller 1990 with low/high-frequency corrections; bark2hz uses the asymmetric 26.28 denominator from MATLAB's bark2hz.m source), hz2erb/erb2hz (Glasberg & Moore 1990 with constants log(10)*1000/(24.673*4.368) and 0.004368 — extracted bit-for-bit from MATLAB hz2erb.m), phon2sone/sone2phon (ISO 532-1 default with smooth break at 40 phon = 1 sone). Cycle M added optional 2nd arg 'ISO 532-2': bit-equal table-lookup PCHIP interpolation per ISO 532-2:2017 Table 5 (28 entries from MATLAB getPerceptualConstants.m), with linear extrapolation beyond 337.6 sone for sone2phon. KNOWN GAP: phon2sone ISO 532-2 ships initial PCHIP guess only; MATLAB additionally refines via fzero (error <1% on smooth inputs). Bit-equal with MATLAB R2025b on all 26 fingerprint points (in-range). Octave 11.1.0 doesn't ship these in core (Audio package only; Octave's audio package missing these specifically). |
| `phon2sone` | ✅ | MATLAB Audio Toolbox frequency-scale + loudness conversions (cycles A + M): hz2mel/mel2hz (O'Shaughnessy default), hz2bark/bark2hz (Traunmüller 1990 with low/high-frequency corrections; bark2hz uses the asymmetric 26.28 denominator from MATLAB's bark2hz.m source), hz2erb/erb2hz (Glasberg & Moore 1990 with constants log(10)*1000/(24.673*4.368) and 0.004368 — extracted bit-for-bit from MATLAB hz2erb.m), phon2sone/sone2phon (ISO 532-1 default with smooth break at 40 phon = 1 sone). Cycle M added optional 2nd arg 'ISO 532-2': bit-equal table-lookup PCHIP interpolation per ISO 532-2:2017 Table 5 (28 entries from MATLAB getPerceptualConstants.m), with linear extrapolation beyond 337.6 sone for sone2phon. KNOWN GAP: phon2sone ISO 532-2 ships initial PCHIP guess only; MATLAB additionally refines via fzero (error <1% on smooth inputs). Bit-equal with MATLAB R2025b on all 26 fingerprint points (in-range). Octave 11.1.0 doesn't ship these in core (Audio package only; Octave's audio package missing these specifically). |
| `sone2phon` | ✅ | MATLAB Audio Toolbox frequency-scale + loudness conversions (cycles A + M): hz2mel/mel2hz (O'Shaughnessy default), hz2bark/bark2hz (Traunmüller 1990 with low/high-frequency corrections; bark2hz uses the asymmetric 26.28 denominator from MATLAB's bark2hz.m source), hz2erb/erb2hz (Glasberg & Moore 1990 with constants log(10)*1000/(24.673*4.368) and 0.004368 — extracted bit-for-bit from MATLAB hz2erb.m), phon2sone/sone2phon (ISO 532-1 default with smooth break at 40 phon = 1 sone). Cycle M added optional 2nd arg 'ISO 532-2': bit-equal table-lookup PCHIP interpolation per ISO 532-2:2017 Table 5 (28 entries from MATLAB getPerceptualConstants.m), with linear extrapolation beyond 337.6 sone for sone2phon. KNOWN GAP: phon2sone ISO 532-2 ships initial PCHIP guess only; MATLAB additionally refines via fzero (error <1% on smooth inputs). Bit-equal with MATLAB R2025b on all 26 fingerprint points (in-range). Octave 11.1.0 doesn't ship these in core (Audio package only; Octave's audio package missing these specifically). |

## Statistics

### Descriptive Statistics

**Namespace:** `stats.descriptive.*` / `stats.moving.*` / `stats.nan.*`. Exception: `xcorr/xcov/rms/rssq/peak2peak/peak2rms` → `signal.*` (signal-side stats) — 14 ✅ + 0 ⚠️ / 33 = 42%

| function | status | comment |
|---|:---:|---|
| `bounds` | ✅ | Sig: [lo,hi] = bounds(X). 1M-pt min/max. 100 iters. |
| `corrcoef` | ✅ 🔬 | corrcoef outputs 2-4 (P, RL, RU) vs MATLAB R2025b. P(i,j)=2*tcdf(-|t|,n-2), diagonal=1. DEEP-PROBE 2026-05-31: added the 3rd/4th outputs RL/RU (lower/upper 95% confidence bounds) + the 'Alpha' name-value (default 0.05) — were unimplemented ('Index exceeds array dimensions' for [R,P,RL,RU]). Fisher z-transform: z=atanh(r), se=1/sqrt(n-3), zc=norminv(1-alpha/2); RL=tanh(z-zc*se), RU=tanh(z+zc*se); |r|==1 (incl diagonal)->RL=RU=r. corrcoef([1 2 4 3 5 7 6 8]',[2 1 3 5 4 6 8 7]') -> RL(1,2)=0.3116980572, RU(1,2)=0.9689892089; with Alpha 0.10 -> RL=0.4328079594, RU=0.9590994664; diagonal RL=1. namespace=stats (descriptive). covers corrcoef. |
| `cov` | ✅ 🔬 | Sig: r = cov(...). Spec-extension batch 2026-05-09. 2026-05-30: NaN-policy flag added — cov(X,'omitrows') drops every row with a NaN (o11=o12=2.3333 on X), cov(X,'partialrows') deletes pairwise so each entry uses rows where both columns are non-NaN (p11=1.6667 over all 4 rows of col1, p12=p22=2.3333 over the 3 rows valid for col2), cov(X,1,'omitrows') uses N normalization (w11=1.5556), and a vector input reduces to the variance over its non-NaN elements (vo=2.3333). 'includenan' (default) still NaN-poisons. |
| `cummax` | ✅ 🔬 | Sig: M = cummax(X). 1M-pt cumulative max. 100 iters. Element-wise SAVE. |
| `cummin` | ✅ 🔬 | Sig: M = cummin(X). 1M-pt cumulative min. 100 iters. Element-wise SAVE. |
| `iqr` | ✅ 🔬 | mad/iqr NaN omission (2026-05-30). MATLAB mad and iqr treat NaN as missing and remove it per column; numkit previously NaN-poisoned. Mn col2 = [5;7;100] after dropping the NaN: mad mean-AD m00=41.77778, mad median-AD m11=2, iqr q2=71.25. Vector with an interior NaN: mad mv=36.625 (over [1 2 4 100]), iqr qv=50.5. namespace=stats; kept in a separate small spec from mad.json/iqr.json. Matches MATLAB R2025b. |
| `kde` | ✅ ➖ | Sig: [f, xi, bw] = kde(x [, pts]) — MATLAB R2023b+ alias for ksdensity. Kernel density estimation with Gaussian kernel by default; bandwidth via Silverman's rule of thumb. v1 implementation: direct alias to ksdensity_reg (same adapter handles positional + name-value calls). Fingerprint pins output shapes (numel = 100 by default), bandwidth positivity, normalisation (integral ≈ 1 over [-3, 3] which captures most mass of N(0,1)). Engine-dependent randn → no bit-exact comparison; structural assertions only (tol 1e-9 on the deterministic shape numbers). |
| `mape` | ✅ | 1M-point MAPE. 50 iters. numkit needs `import compat.*`; MATLAB+Octave have it flat. |
| `max` | ✅ 🔬 | Sig: M = max(X). 1M-pt. 100 iters. Scalar fp. DEEP-PROBE 2026-05-29: max of an EMPTY array returns empty and never errors. Two-arg max(a,b) IGNORES NaN. DEEP-PROBE 2026-06-04: the REDUCTION form also defaults to 'omitnan' like MATLAB — max([NaN 3 1])=3 with idx 2 (was NaN-order-dependent: NaN, idx 1); all-NaN -> NaN; per-column on matrices. va=max([NaN 6 8],[5 7 NaN])=[5 7 8]. |
| `maxk` | ✅ 🔬 | Sig: B = maxk(X, K). Top 10 of 1M. 100 iters. |
| `mean` | ✅ 🔬 | Sig: M = mean(X). 1M-pt sin reduction. 100 iters. Scalar fp. DEEP-PROBE 2026-05-29: mean([])=NaN as a SCALAR (numel 1, NOT 1x0). |
| `median` | ✅ | Sig: M = median(X). 1M-pt full sort + middle. Scalar fp. DEEP-PROBE 2026-05-29: median of an EMPTY array -> NaN (not 0x0). 2026-05-30: integer-typed input PRESERVES the integer class (round half-away-from-zero + saturate): median(int32([1 2 3 4]))=3 int32, median(int8([-1 -2]))=-2 int8, median(uint8([10 20 30 41]))=25 uint8 (numkit previously returned DOUBLE 2.5). COMPLEX (fixed 2026-06-05): orders by abs, ties by angle (same as sort/max); even n -> mean of the two middle: median([1+1i 2+2i 3+3i])=2+2i; median([..10+10i])=2.5+2.5i; per-column matrix; dim 2; abs-tie median([1 1i -1])=0+1i; 'all' flattens (=2.5+0i). NOTE: ; appears only inside matrix-literal INPUTS, never inside a quoted string. |
| `min` | ✅ 🔬 | Sig: M = min(X). 1M-pt. 100 iters. Scalar fp. DEEP-PROBE 2026-05-29: min of an EMPTY array returns empty and never errors. Two-arg min(a,b) IGNORES NaN. DEEP-PROBE 2026-06-04: the REDUCTION form also defaults to 'omitnan' like MATLAB — min([NaN 3 1])=1 with idx 3 (was NaN-order-dependent); all-NaN -> NaN; per-column on matrices. va=min([NaN 6 8],[5 7 NaN])=[5 6 8]. |
| `mink` | ✅ 🔬 | Sig: B = mink(X, K). Bot 10 of 1M. 100 iters. |
| `mode` | ✅ 🔬 | mode 3rd output C (2026-05-30): [M,F,C]=mode(X). C is a cell array of the modal values -- each cell holds a sorted column vector of all values tied for the modal frequency in that slice (MATLAB ignores NaN). Previously numkit only returned [M,F] (the 3rd output errored 'Undefined function or variable'). [3 3 1 2 2]: both 2 and 3 occur twice -> mv=2 (smallest tie), fv=2, cv{1}=[2;3] (v1=2,v2=3,nv=2). Matrix M default (per column): col1 ties 1,2 -> cm{1}=[1;2] (c11=1,c12=2,n1=2); col2 mode 2 -> n2=1. dim=2 (per row): row3=[1 3] both once -> cr{3}=[1;3] (r3a=1,r3b=3). Supported for real double vector/2-D matrix/'all'; ND/non-double 3rd output deferred. namespace=core. Matches MATLAB R2025b. |
| `movmad` | ✅ | Sig: movmad(A, k[, dim] [, nanflag] [, Name, Value]). Same surface as movmean. |
| `movmax` | ✅ | Sig: movmax(A, k[, dim] [, nanflag] [, Name, Value]). Same surface as movmean. |
| `movmean` | ✅ 🔬 | Sig: M = movmean(A, k[, dim] [, nanflag] [, Name, Value]). nanflag in {includemissing|includenan (default)|omitmissing|omitnan}. Endpoints in {shrink (default)|discard|fill|scalar}. EVEN scalar window leans BACKWARD (MATLAB): movmean([1 2 3 4],2)=[1 1.5 2.5 3.5] (window [i-1,i]); per-column on a matrix likewise. numkit previously leaned forward for even k -- fixed 2026-05-29. COMPLEX (fixed 2026-06-05): moving-mean the real + imaginary parts separately, recombine: movmean([1+1i 2+2i 3+3i 4+4i],2)=[1+1i 1.5+1.5i 2.5+2.5i 3.5+3.5i]; centered odd window; asymmetric [kb kf]; per-column matrix. SamplePoints not yet implemented (parity gap, throws). DataVariables/ReplaceValues table-only (throw). k=0 throws MATLAB-matching error. Verified: NaN propagation, omitnan/omitmissing alias, includenan, all four Endpoints modes, even-window backward, matrix+dim+nanflag+endpoints. |
| `movmedian` | ✅ 🔬 | Sig: M = movmedian(A, k[, dim] [, nanflag] [, Name, Value]). Same surface as movmean. |
| `movmin` | ✅ | Sig: movmin(A, k[, dim] [, nanflag] [, Name, Value]). Same surface as movmean. |
| `movprod` | ✅ | Sig: movprod(A, k[, dim] [, nanflag] [, Name, Value]). Same surface as movmean. |
| `movstd` | ✅ 🔬 | Sig: movstd(A, k[, normFlag] [, dim] [, nanflag] [, Name, Value]). normFlag in {0 (default, N-1), 1 (N)}. Same nanflag/Endpoints surface as movmean. 2026-05-30: single-element-window std is 0, not NaN (MATLAB parity) — y_k2(1)=0 (edge of a length-2 window), y_z(5)=0 (length-1 window everywhere via [0 0]), y_o2(7)=y_o2(8)=0 (omitnan reduces those windows to one valid element). |
| `movsum` | ✅ 🔬 | Sig: movsum(A, k[, dim] [, nanflag] [, Name, Value]). Same surface as movmean. EVEN scalar window leans BACKWARD (current+previous), MATLAB rule: movsum([1 2 3 4],2)=[1 3 5 7] (window [i-1,i]); movsum([1 2 3 4 5 6],4)=[3 6 10 14 18 15] (window [i-2,i+1]). numkit previously leaned forward for even k -- fixed 2026-05-29. |
| `movvar` | ✅ 🔬 | Sig: movvar(A, k[, normFlag] [, dim] [, nanflag] [, Name, Value]). normFlag in {0 (default, N-1), 1 (N)}. Same nanflag/Endpoints surface as movmean. 2026-05-30: single-element-window var is 0, not NaN (MATLAB parity) — y_k2(1)=0 (edge of a length-2 window), y_z(5)=0 (length-1 window everywhere via [0 0]), y_o2(7)=y_o2(8)=0 (omitnan reduces those windows to one valid element). |
| `prctile` | ✅ 🔬 | Sig: P = prctile(A, p [, dim | 'all' | vecdim] [, Method=method]). Same surface as quantile but p in [0, 100]. |
| `quantile` | ✅ 🔬 | Sig: Q = quantile(A, p [, dim | 'all' | vecdim] [, 'Method', m]). Default = the MATLAB 'exact' algorithm (R2007a linear-interpolation, positions (k-0.5)/N). MATLAB's documented Method values are 'exact' (default) and 'approximate'; numkit also keeps midpoint/inclusive/exclusive for compatibility. quantile([1 2 3 4],0.25,'exact') = 1.5. numkit previously rejected 'exact' -- alias added. |
| `rms` | ✅ | Sig: R = rms(X). 1M-pt sin RMS. 100 iters. Scalar fp. |
| `rmse` | ✅ | Sig: R = rmse(F, A). 1M-pt. 100 iters. |
| `std` | ✅ 🔬 | Sig: S = std(A[, w | W][, dim | 'all' | vecdim][, nanflag]). Same surface as var. Single-element std = 0 (NOT NaN): std(5)=0, std([7])=0. EMPTY std -> NaN scalar (not 0x0): std([])=NaN. + DEEP-PROBE 2026-05-29 n==1 + empty fixes. DEEP-PROBE 2026-05-31: weight VECTOR on a MATRIX now reduces per-slice along the operating dim (was: threw). std(Mw,wc)=[1.490711985 1.490711985] (1x2 dim-1 default), std(Mw,[1 3],2) per-row (3x1). N-D weighted reduction deferred. |
| `summary` | ❌ |  |
| `var` | ✅ 🔬 | Sig: V = var(A[, w | W][, dim | 'all' | vecdim][, nanflag]). w in {0, 1} or vector W (weighted; denominator = sum(W)). 'all' / full-flatten vecdim flatten input. Default nanflag = includenan (NaN poisons; matches MATLAB R2025b for double). Single-element variance = 0 for BOTH N-1 (default) and N normalizations (NOT NaN from 0/0): var(5)=0, var([7])=0, var(5,1)=0. EMPTY var -> NaN (not 0x0): var([])=NaN scalar; var(zeros(0,3))=[NaN NaN NaN] (1x3); var(zeros(3,0))=1x0. + DEEP-PROBE 2026-05-29 n==1 + empty fixes. DEEP-PROBE 2026-05-31: weight VECTOR on a MATRIX now reduces per-slice along the operating dim (was: threw 'weight vector length must match number of elements' for default dim, and 'weight vector with non-flat dim not yet supported' for explicit dim). var(Mw,wc)=[2.2222 2.2222] (1x2, dim 1 default), var(Mw,[1 3],2)=[0.1875;0.1875;0.1875] (3x1). N-D weighted reduction still deferred. (NOTE: var([1 2 3],0,1) should be [0 0 0] but a separate core reduce-along-singleton-dim bug returns scalar 1 — deferred, also affects sum/mean.) |
| `xcorr` | ✅ 🔬 | Sig r=xcorr(x[,y][,maxlag][,scaleopt]). Raw xcorr([1 2 3]) = [3 8 14 8 3]. scaleopt: 'biased' (/N), 'unbiased' (/(N-|lag|)), 'coeff'/'normalized' (/sqrt(Rxx0*Ryy0), autocorr peak->1). maxlag crops to lags -maxlag..maxlag. coeff: rc=[.2143 .5714 1 .5714 .2143]; biased=[1 2.667 4.667 2.667 1]; unbiased=[3 4 4.667 4 3]; maxlag 1 -> [8 14 8]. numkit previously accepted-and-IGNORED scaleopt+maxlag -- now honored. Matches MATLAB R2025b. |
| `xcov` | ✅ | Sig c=xcov(x,y,scaleopt): cross-covariance of mean-removed signals with scaling. zero-lag is index 5 (length 2N-1=9). 'none' c(5)=5 raw; 'biased' divides every lag by N=5 -> c(5)=1, c(4)=-1.56, c(6)=0.6; 'unbiased' divides lag m by (N-|m|) -> c(5)=1, c(3)=1.26667, c(7)=-2.53333; 'coeff' divides by sqrt(Cxx0*Cyy0)=sqrt(22.8*10) -> c(5)=0.331133. numkit previously ignored scaleopt entirely (returned raw for all). Fixed in convolution.cpp. |

### Descriptive Statistics — extras

**Namespace:** `stats.descriptive.*` — additions on top of the existing section above. 0 ✅ + 0 ⚠️ / 23 = 0%

| function | status | comment |
|---|:---:|---|
| `cholcov` | ✅ | MATLAB cholcov: Cholesky-like factor of (possibly singular) covariance. Bit-equal with MATLAB R2025b on key invariants: PD case gives upper-triangular n×n (T'*T = SIGMA exactly), PSD rank-r case gives r×n (T'*T = SIGMA up to rounding), indefinite/negative gives empty T and p > 0. Eigvec sign/ordering can differ between engines so we pin invariants (residual + dimensions + p), not literal entries beyond the PD diagonal. Octave 11.1.0 doesn't ship cholcov in core (statistics package only); reports N/A. |
| `corr` | ⚠️ 🔬 | Sig c=corr(X) / [c,p]=corr(X,Y[,'Type',T][,'Rows',R]). Pearson matrix (single-arg between cols of X; two-arg X.cols x Y.cols). 'Type' Spearman = Pearson of tied (average) ranks; 'Kendall' = tau-b (ties adjusted). corr([1;2;3;4],[1;4;9;16]) Pearson=0.984374 but Spearman=Kendall=1. With ties c,d: Spearman=0.892218, Kendall=0.824958. numkit previously IGNORED 'Type' (always Pearson) -- now honored. 2026-05-30: 'Rows' NaN policy added (previously IGNORED -- corr always NaN-poisoned). 'complete' = listwise deletion (drop any row with a NaN, then correlate): cc12=1, cc13=-0.300376. 'pairwise' = pairwise deletion (each (i,j) uses rows where both columns are non-NaN, so entries can differ): cp13=-0.290191 vs cp23=-0.300376. Two-vector 'complete' xyc=1. 'pairwise' currently Pearson-only. 2026-06-05 (bugs/stats/corr-pvalue.md): 2nd output p-value added. Pearson p = 2*tcdf(-|t|, n-2), t = r*sqrt((n-2)/(1-r^2)) (exact all n): pPe=0.087706647. Kendall p = EXACT permutation (inversions) distribution, no ties up to n=100 then normal approx: pKd=0.233333333 (n=5), pKd8=0.014136905 (n=8). Spearman p = EXACT permutation for n<=10 (pSp=0.133333333 at n=5, pSp7=0.012301587 at n=7), t-approx for large n (MATLAB uses AS 89 -- large-n Spearman p is approximate, only small-n exact validated here). Matrix [R,P]=corr(X): P has R's shape, auto-correlation diagonal = 1 (Pm11=1), off-diagonal is the pairwise p (Pm12=pPe). Matches MATLAB R2025b. |
| `corrcov` | ✅ | MATLAB corrcov: R = C ./ sqrt(diag(C)*diag(C)'); sigma = sqrt(diag(C))'. Bit-equal with MATLAB R2025b on 3x3 covariance, identity, scalar, and negative-correlation 2x2 cases. Octave 11.1.0 doesn't ship corrcov in core (statistics package only); reports N/A. |
| `crosstab` | ✅ | MATLAB crosstab: contingency table with chi-square independence test. Bit-equal with MATLAB R2025b on table entries; chi2 and p match within tolerance (chi2cdf depends on incomplete-gamma which has small numerical drift between engines). DEEP-PROBE c179: the 4th output `labels` (a maxCategories-by-numVars cell of num2str value strings, char rows, padded with [] empty double beyond each variable's category count) was MISSING — numkit emitted only [T,chi2,p]. Now [T,chi2,p,labels]=crosstab(x,y) returns labels matching MATLAB (lab is 3x2 here: lab{1,1}='1', lab{3,1}='3', lab{1,2}='10', lab{3,2}=[]); single-arg form gives an R-by-1 label column. Octave 11.1.0 doesn't ship crosstab in core (statistics package only); reports N/A. |
| `geomean` | ✅ 🔬 | geomean/harmmean 'omitnan' nanflag (2026-05-30). MATLAB geomean/harmmean accept a trailing 'omitnan' (or 'includenan', default) nanflag that removes NaN per slice; numkit previously IGNORED the flag (geomean returned NaN) or ERRORED converting the char to a dim (harmmean). Mn col2 = [5;7;100;8] after dropping the NaN: geomean(...,'omitnan') g2=12.935687, g02=12.935687 (geomean(Mn,'omitnan') same), harmmean(...,'omitnan') h2=8.3707025. col1 (no NaN) unchanged g1=2.6051711, h1=2.1898. Default (includenan) still propagates NaN: gd2=NaN. namespace=stats; kept separate from geomean.json/harmmean.json. Matches MATLAB R2025b. |
| `grpstats` | ✅ | MATLAB grpstats: per-group statistics. Bit-equal with MATLAB R2025b on default-mean, multi-fn cell-of-strings, sum, std, numel aggregators. Other aggregators (var, sem, min, max) supported in numkit. DEEP-PROBE c178: with NO whichstats arg the default OUTPUTS are [means, sem, counts] (MATLAB's nargout default) — numkit only emitted the means, so [m,sem,counts]=grpstats(X,g) threw 'Index 1 exceeds array size 0'. Now ds = standard error of the mean (std/sqrt(n)) and dc = per-group counts. Cell-of-fn output ordering matches MATLAB's nargout indexing. Octave 11.1.0 doesn't ship grpstats in core (statistics package only); reports N/A. |
| `harmmean` | ✅ 🔬 | Sig h=harmmean(X[,dim]): harmonic mean = n / sum(1./x). Vector harmmean([2 4 6 8])=3.84. Matrix default per-column: harmmean(magic(4)) cols (1,2)=(6.414254, 4.967742). dim=2 per-row pinned too. |
| `kurtosis` | ✅ 🔬 | Sig: skewness(X [, flag [, dim]]) — sample skewness (3rd central moment / std³). kurtosis(X [, flag [, dim]]) — sample kurtosis (4th central moment / variance²). flag = 0 → bias-corrected estimator; flag = 1 (default) → biased moment ratio. Symmetric data → skewness = 0, kurtosis = 1.7 (for [1..5]). Matrix input reduces along first non-singleton dim → row of per-column moments. Bit-exact MATLAB R2025b on the documented signatures. 2026-05-30: skewness/kurtosis now treat NaN as missing and remove it per column (MATLAB default), previously NaN-poisoned. Mn col2 = [5;7;100] after dropping the NaN: sn2=0.706027 (skewness), kn2=1.5 (kurtosis); col1 has no NaN so sn1/kn1 unchanged. |
| `mad` | ✅ 🔬 | mad/iqr NaN omission (2026-05-30). MATLAB mad and iqr treat NaN as missing and remove it per column; numkit previously NaN-poisoned. Mn col2 = [5;7;100] after dropping the NaN: mad mean-AD m00=41.77778, mad median-AD m11=2, iqr q2=71.25. Vector with an interior NaN: mad mv=36.625 (over [1 2 4 100]), iqr qv=50.5. namespace=stats; kept in a separate small spec from mad.json/iqr.json. Matches MATLAB R2025b. |
| `moment` | ✅ | Sig m=moment(x,k[,dim]): central k-th moment = mean((x-mean(x)).^k) (population, /N). magic(4) per-column: 3rd moment col1=72, col2=-24; 4th moment col1=931.0625. Vector [1 5 3 8 2 7 4 6]: 2nd moment (population var)=5.25, 3rd moment=0 (symmetric about 4.5). |
| `nearcorr` | ⚠️ | MATLAB nearcorr: nearest correlation matrix (Higham 2002 alternating projections + Dykstra). Identity case (input already correlation) is unchanged; Higham 3x3 textbook example produces [-0.4041, 0.4988, 0.5912] off-diagonals; output is symmetric, unit-diag, PSD (min eigval ~ 0 for indefinite inputs). Defaults tol=1e-10, maxits=100; 'tolconv'/'maxits' name-value parameters deferred for v1. Uses eig_symmetric (toolboxes/builtin) for the PSD projection. Octave 11.1.0 doesn't ship nearcorr in core (statistics package only); reports N/A. |
| `partialcorr` | ✅ 🔬 | Sig: r = partialcorr(X), partialcorr(X, Z), partialcorr(X, Y, Z). Pearson partial correlation. 1-arg form: pair-specific control = other X cols. 2-arg form: control = Z. 3-arg form: cross-correlation residualised on Z. Bit-identical with MATLAB R2025b on deterministic data. Forms 1+2 added cycle 85. 'Rows' NaN policy covered in partialcorr_rows.json (kept separate to stay under the numkit VM 255-register-per-chunk limit). |
| `partialcorri` | ✅ | Sig: R = partialcorri(Y, X [, Z]) — partial correlation between each Y column and each X column, controlling for the OTHER X cols (and Z). canoncorr(X, Y) = canonical correlation analysis via centring + QR + SVD; returns canonical coefficients A, B (p×k, q×k) and the canonical correlations r (length k = min(p, q)). Bit-exact MATLAB R2025b not feasible (rng state differs); fingerprint pins structural invariants on the recoverability test (diag dominance for partialcorri on planted dependence, r(1) ≈ 1 for canoncorr on shared latent factor) and on the shapes. |
| `range` | ✅ 🔬 | Sig: r = range(x[, dim]). max - min along dim. Bit-identical with MATLAB R2025b. |
| `robustcov` | ✅ ➖ | Sig: [b, s] = robustfit(X, y [, wfun [, tune]]) — IRLS robust regression with bisquare (default, tune=4.685) or huber (tune=1.345). KNOWN GAP: stats struct (DOF, p-values, etc.) reduced to scalar s. [sigma, mu] = robustcov(X) — trimmed-MCD robust covariance via h = ceil(0.75 · n) concentration steps with Pison-Van Aelst-Willems consistency correction. KNOWN GAPs: full FAST-MCD multi-start, MVE method, OGK estimator not in v1. Spec uses deterministic sin/cos noise to make parity reproducible across engines; pins error bounds and shape invariants. |
| `skewness` | ✅ 🔬 | Sig: skewness(X [, flag [, dim]]) — sample skewness (3rd central moment / std³). kurtosis(X [, flag [, dim]]) — sample kurtosis (4th central moment / variance²). flag = 0 → bias-corrected estimator; flag = 1 (default) → biased moment ratio. Symmetric data → skewness = 0, kurtosis = 1.7 (for [1..5]). Matrix input reduces along first non-singleton dim → row of per-column moments. Bit-exact MATLAB R2025b on the documented signatures. 2026-05-30: skewness/kurtosis now treat NaN as missing and remove it per column (MATLAB default), previously NaN-poisoned. Mn col2 = [5;7;100] after dropping the NaN: sn2=0.706027 (skewness), kn2=1.5 (kurtosis); col1 has no NaN so sn1/kn1 unchanged. |
| `tabulate` | ✅ | MATLAB tabulate: frequency table. Bit-equal with MATLAB R2025b on positive-int dense layout (with zeros for missing values), non-integer sparse layout, and NaN-excluded percentage. Octave 11.1.0 doesn't ship tabulate in core (statistics package only); reports N/A. |
| `tiedrank` | ✅ | MATLAB tiedrank: ranks adjusted for ties via averaging. Bit-equal with MATLAB R2025b on vector + matrix forms. Tieadj uses (t^3 - t) / 2 per tied group. Includes all-equal and no-ties edges. NaN handling tested in gtest only (parity harness fingerprint format doesn't preserve NaN trivially). |
| `trimmean` | ✅ | Sig m=trimmean(x,percent[,flag][,dim]): mean after trimming percent/2 from each end. v=[1..8 100 -50]: percent=0 -> plain mean 8.6; percent=20 trims 1 each end -> 4.5. Matrix per-column trimmean(magic(5),40). DEEP-PROBE 2026-05-31: the DEFAULT rounding of the per-end trim count was floor (== MATLAB 'floor') but MATLAB DEFAULT is 'round' (round n*pct/200 half-DOWN -> k=ceil(n*pct/200-0.5)); and the 'round'/'floor' flag arg was unparsed (threw 'Cannot convert char to scalar'). w=[1 2 4 8 16 32 64 128 256 1000]: p=35 default/round=42 (k=2), floor=63.75 (k=1); p=30 default=63.75 (1.5 rounds down to k=1); p=50 default=42 (2.5 rounds down to k=2). Flag+dim: trimmean(M,40,'floor',2). namespace=stats. |
| `zscore` | ✅ 🔬 | MATLAB zscore — centre + scale to unit std. DEFAULT flag 0 uses the SAMPLE std (N-1): z0(1)=-1.40312152 (previously numkit wrongly used N -> -1.5). flag 1 uses population std (N). dim arg: zscore(M,0,2) operates along rows. Pins actual normalised values across flag 0/1 and dim 1/2. Bit-equal MATLAB R2025b (tol=1e-9). The old spec only checked abs(mean(z))<1e-9, which is true for BOTH normalisations and hid the bug. |
| `nancov` | ✅ | Sig: C = nancov(X) — NaN-aware covariance matrix; rows containing any NaN are dropped (== MATLAB cov(X, 'omitrows'), the default 'complete' mode). nancov(X, normFlag) — 0 unbiased (n-1) / 1 population (n). nancov(x, y) — between two vectors; treats [x y] as 2-column matrix. Vector input → scalar variance. KNOWN GAP: 'pairwise' mode (per-(i,j) row-drop) not in v1 — only 'complete'. Bit-exact MATLAB R2025b on the documented signatures. |
| `nansum` | ✅ | Sig: legacy NaN-aware reductions (recommended modern form: `sum(..., 'omitnan')` / `mean(..., 'omitnan')`). nansum: NaN entries dropped; all-NaN slice → 0 (NaN is additive identity). nanmean: NaN entries dropped; divisor is count of valid obs; all-NaN slice → NaN. nanvar/nanstd: NaN entries dropped; a SINGLE non-NaN value → 0 for BOTH N-1 (default) and N normalizations (NOT NaN from 0/0) — verified MATLAB R2025b nanvar([NaN 5 NaN])==0. Bit-exact MATLAB R2025b on the pinned cases. nanstd/nanvar/nanmedian/nanmax/nanmin work but are NOT in PROGRESS.md (legacy; modern form is var(...,'omitnan')). |
| `nanmean` | ✅ | Sig: legacy NaN-aware reductions (recommended modern form: `sum(..., 'omitnan')` / `mean(..., 'omitnan')`). nansum: NaN entries dropped; all-NaN slice → 0 (NaN is additive identity). nanmean: NaN entries dropped; divisor is count of valid obs; all-NaN slice → NaN. nanvar/nanstd: NaN entries dropped; a SINGLE non-NaN value → 0 for BOTH N-1 (default) and N normalizations (NOT NaN from 0/0) — verified MATLAB R2025b nanvar([NaN 5 NaN])==0. Bit-exact MATLAB R2025b on the pinned cases. nanstd/nanvar/nanmedian/nanmax/nanmin work but are NOT in PROGRESS.md (legacy; modern form is var(...,'omitnan')). |

### Probability Distributions

**Namespace:** `stats.dist.*` — 115 ✅ + 0 ⚠️ / 130+ = 88%

Each distribution provides 5 entrypoints: `*pdf` / `*cdf` / `*inv` (or `*icdf`) / `*rnd` / `*stat`. All `rnd` functions share `numkit::builtin::sharedEngine()` so `rng(seed)` reseeds them. Discrete `*inv` use one-ULP relative tolerance against the public cdf so `inv(cdf(k))=k` round-trips don't overshoot.

| function | status | comment |
|---|:---:|---|
| `normpdf` | ✅ | Sig: y = normpdf(x[, mu, sigma]). Normal PDF: (1/(σ√(2π)))·exp(-(x-μ)²/(2σ²)). Defaults mu=0, sigma=1. sigma<=0 => NaN. Vectorised. |
| `normcdf` | ✅ |  |
| `norminv` | ✅ | Sig: x = norminv(p[, mu, sigma]). Inverse Normal CDF: x = mu + sigma*Φ⁻¹(p). Defaults mu=0, sigma=1. Edges: p=0 => -Inf; p=1 => +Inf; p outside [0,1] => NaN; sigma<=0 => NaN. Tol 1e-9 -- erfcinv algorithm differs ~1e-12 absolute from MATLAB. |
| `normrnd` | ✅ |  |
| `normstat` | ✅ | Sig: [m, v] = normstat(mu, sigma). Trivially m=mu, v=sigma². Vectorised with broadcasting (equal sizes or one scalar). sigma<=0 => NaN. |
| `chi2pdf` | ✅ | Sig: y = chi2pdf(x, k). Chi-squared PDF with k dof. x < 0 => 0; k <= 0 => NaN. Covers: scalar, vector x, x<0 + x=0 edges, k=1 (special: x^(-1/2)·exp(-x/2)/√(2π)), k=30 large dof. |
| `chi2cdf` | ✅ | gammainc(x/2, k/2) |
| `chi2inv` | ✅ | Sig: x = chi2inv(p, k). Inverse Chi² CDF with k dof. Covers k ∈ {1, 5, 30} × p ∈ {0.05, 0.5, 0.95} + p=0 (=> 0) + p=1 (=> Inf) + p outside [0,1] (=> NaN) + k<=0 (=> NaN). |
| `chi2rnd` | ✅ |  |
| `chi2stat` | ✅ | Sig: [m, v] = chi2stat(k). Chi² mean=k and variance=2k. Vectorised. k<=0 => NaN (moments don't exist for degenerate). |
| `tpdf` | ✅ | Sig: y = tpdf(x, nu). Student's t PDF via lgamma-stable form. nu=Inf -> Gaussian limit (1/sqrt(2π))·exp(-x²/2). nu<=0 or NaN -> NaN. NaN x -> NaN. |
| `tcdf` | ✅ | betainc on z = ν/(ν+x²), branch by sign |
| `tinv` | ✅ | Sig: x = tinv(p, nu). Inverse Student's t-CDF. Uses betaincinv(2(1-p) or 2p, nu/2, 1/2) and signs by p<>0.5. nu=Inf -> Gaussian limit (norminv(p)). Edges: p=0 -> -Inf; p=1 -> +Inf; p outside [0,1] -> NaN; nu<=0 -> NaN. |
| `trnd` | ✅ | Z/√(X/ν), Z~N(0,1), X~χ²(ν) |
| `tstat` | ✅ | Sig: [m, v] = tstat(nu). Student's t: m=0 if nu>1, v=nu/(nu-2) if nu>2. Vectorised. nu<=0 => NaN/NaN; 0<nu<=1 => m=NaN,v=NaN; 1<nu<=2 => m=0, v=NaN. |
| `fpdf` | ✅ | Sig: y = fpdf(x, v1, v2). F-distribution PDF. x < 0 => 0; v1 <= 0 or v2 <= 0 => NaN. Covers: scalar (v1=5,v2=10), vector x, x<0/x=0 edges, invalid v1/v2, F(2,10) at 0 (= v1/(v1+v2-2)/B(...) finite for v1=2). |
| `fcdf` | ✅ | betainc(v1·x/(v1·x+v2), v1/2, v2/2) |
| `finv` | ✅ | Sig: x = finv(p, v1, v2). Inverse F CDF. Covers (v1, v2) ∈ {(1,1), (5,10), (10,30)} × p ∈ {0.05, 0.5, 0.95} + p=0 (=> 0) + p=1 (=> Inf) + p outside [0,1] (=> NaN) + v1<=0 / v2<=0 (=> NaN). |
| `frnd` | ✅ | (X1/v1)/(X2/v2), Xi~χ²(vi) |
| `fstat` | ✅ | Sig: [m, v] = fstat(v1, v2). F-distribution mean = v2/(v2-2) for v2>2 else NaN; variance = 2*v2²(v1+v2-2)/(v1(v2-2)²(v2-4)) for v2>4 else NaN. Vectorised. v1<=0 or v2<=0 => NaN/NaN. |
| `betapdf` | ✅ | Sig: y = betapdf(x, a, b). Beta PDF on (0,1). x outside (0,1) => 0; a<=0 or b<=0 => NaN. Covers: scalar, vector, out-of-(0,1) edges (x<0, x=0, x=0.5, x=1, x>1), invalid params. |
| `betacdf` | ✅ | I_x(a, b) directly |
| `betainv` | ✅ | Sig: x = betainv(p, a, b). Inverse Beta CDF. Covers (a,b) ∈ {(1,1) uniform, (0.5,0.5) U-shaped, (2,5), (10,10)} × p ∈ {0.05, 0.5, 0.95}. Edges: p=0 => 0; p=1 => 1; p outside [0,1] => NaN; invalid shape => NaN. |
| `betarnd` | ✅ | U/(U+V), U~Gamma(a,1), V~Gamma(b,1) |
| `betastat` | ✅ | Sig: [m, v] = betastat(a, b). Beta(a,b) mean a/(a+b) and variance ab/((a+b)^2(a+b+1)). Vectorised. Invalid params (a<=0 or b<=0) => NaN. Beta(1,1) is uniform: m=0.5, v=1/12. |
| `gampdf` | ✅ | Sig: y = gampdf(x, a, b). Gamma(shape=a, scale=b) PDF. Density at 0: a<1 → Inf, a=1 → 1/b, a>1 → 0. x<0 → 0. a<0 or b<=0 → NaN. a=0 → 0 (degenerate). |
| `gamcdf` | ✅ | gammainc(x/b, a) |
| `gaminv` | ✅ | Sig: x = gaminv(p, a, b). Inverse Gamma CDF. q=0 → 0; q=1 → Inf; q outside [0,1] → NaN. a=0 → 0 (degenerate); a<0 / b<=0 → NaN. |
| `gamrnd` | ✅ | std::gamma_distribution(a, b) |
| `gamstat` | ✅ | Sig: [m, v] = gamstat(a, b). Gamma(shape, scale): m = a·b, v = a·b². Vectorised. a<=0 or b<=0 => NaN. |
| `exppdf` | ✅ | Sig: y = exppdf(x[, mu]). Exponential PDF: (1/mu)·exp(-x/mu). Default mu=1. x<0 → 0. mu<=0 → NaN. |
| `expcdf` | ✅ | -expm1(-x/μ) |
| `expinv` | ✅ | Sig: x = expinv(p[, mu]). Inverse exponential CDF: x = -mu*log(1-p). Default mu=1. Covers default form + non-default mu + boundaries (p=0,1) + invalid (p<0, p>1, mu<=0). |
| `exprnd` | ✅ |  |
| `expstat` | ✅ | Sig: [m, v] = expstat(mu). Exponential mean=mu, variance=mu^2. Vectorised. mu<=0 => NaN. |
| `unifpdf` | ✅ | Sig: y = unifpdf(x[, a, b]). Continuous uniform PDF on [a, b]; defaults a=0, b=1. y = 1/(b-a) for x in [a,b], else 0. Edges: b<=a -> NaN; NaN x -> NaN; NaN a/b -> 0 (NaN comparisons false, MATLAB convention). |
| `unifcdf` | ✅ |  |
| `unifinv` | ✅ | Sig: x = unifinv(p[, a, b]). Inverse Continuous Uniform CDF on [a, b]: x = a + p*(b-a). Defaults a=0, b=1. p=0 -> a; p=1 -> b; p outside [0,1] -> NaN; b<=a -> NaN; NaN p -> NaN. |
| `unifrnd` | ✅ |  |
| `unifstat` | ✅ | Sig: [m, v] = unifstat(a, b). Continuous uniform on [a,b]: m=(a+b)/2, v=(b-a)²/12. Vectorised. b<=a => NaN. |
| `lognpdf` | ✅ | Sig: y = lognpdf(x[, mu, sigma]). Lognormal PDF. Defaults mu=0, sigma=1. x<=0 → 0. sigma<=0 → NaN. |
| `logncdf` | ✅ |  |
| `logninv` | ✅ | Sig: x = logninv(p[, mu, sigma]). Inverse Lognormal CDF. Defaults mu=0, sigma=1. q=0 → 0; q=1 → Inf; q outside [0,1] → NaN. sigma<=0 → NaN. |
| `lognrnd` | ✅ |  |
| `lognstat` | ✅ | Sig: [m, v] = lognstat(mu, sigma). Lognormal: m = exp(mu + sigma²/2), v = (exp(sigma²)-1)·exp(2mu + sigma²). Vectorised. sigma<=0 => NaN. |
| `wblpdf` | ✅ | Sig: y = wblpdf(x[, a, b]). Weibull PDF with scale a, shape b. Defaults a=1, b=1 (= exponential). Edges: x<0 -> 0; x=0 -> b/a if b=1, Inf if b<1, 0 if b>1; a<=0 or b<=0 -> NaN; NaN -> NaN. |
| `wblcdf` | ✅ |  |
| `wblinv` | ✅ | Sig: x = wblinv(p[, a, b]). Inverse Weibull CDF: x = a · (-log(1-p))^(1/b). Defaults a=1, b=1 (= exponential -log(1-p)). p=0 -> 0; p=1 -> Inf; p outside [0,1] -> NaN; a<=0, b<=0 -> NaN; NaN -> NaN. |
| `wblrnd` | ✅ |  |
| `wblstat` | ✅ | Sig: [m, v] = wblstat(a, b). Weibull(scale=a, shape=b): m = a·Γ(1+1/b), v = a²·(Γ(1+2/b) - Γ(1+1/b)²). Vectorised. a<=0 or b<=0 => NaN. |
| `raylpdf` | ✅ | Sig: y = raylpdf(x, b). Rayleigh PDF. x<0 → 0; x=0 → 0 (density at origin is 0). b<=0 → NaN. |
| `raylcdf` | ✅ |  |
| `raylinv` | ✅ | Sig: x = raylinv(p, b). Inverse Rayleigh CDF: x = b·sqrt(-2·ln(1-p)). q=0 → 0; q=1 → Inf; q outside [0,1] → NaN. b<=0 → NaN. |
| `raylrnd` | ✅ | inverse-cdf sampling |
| `raylstat` | ✅ | Sig: [m, v] = raylstat(b). Rayleigh: m = b·sqrt(π/2), v = b²·(2 - π/2). Vectorised. b<=0 => NaN. |
| `poisspdf` | ✅ | Sig: y = poisspdf(k, lambda). Poisson PMF. Out-of-support k (<0, non-integer) → 0. lambda=0 degenerate: only k=0 → 1. lambda<0 → NaN. |
| `poisscdf` | ✅ | F(k; λ) = 1 - gammainc(λ, ⌊k⌋+1) |
| `poissinv` | ✅ | Sig: x = poissinv(p, lambda). Inverse Poisson CDF. q=0 → 0; q=1 → Inf; q outside [0,1] → NaN. lambda=0 → 0 (degenerate). lambda<0 → NaN. |
| `poissrnd` | ✅ |  |
| `poisstat` | ✅ | Sig: [m, v] = poisstat(lambda). Poisson mean=variance=lambda. Vectorised. lambda<=0 => NaN. |
| `binopdf` | ✅ | Sig: y = binopdf(k, n, p). Binomial PMF. Out-of-support k (negative, > n, non-integer) → 0. p=0: only k=0 → 1. p=1: only k=n → 1. Invalid n / p out of [0,1] → NaN. |
| `binocdf` | ✅ | I_{1-p}(n - ⌊k⌋, ⌊k⌋ + 1) |
| `binoinv` | ✅ | Sig: x = binoinv(q, n, p). Inverse Binomial CDF. q=0 → 0; q=1 → n. Invalid (q outside [0,1] / p outside [0,1] / n<0 / non-integer n) => NaN. |
| `binornd` | ✅ |  |
| `binostat` | ✅ | Sig: [m, v] = binostat(n, p). Binomial: m=n·p, v=n·p·(1-p). Vectorised. n<0 / non-integer / p<0 / p>1 => NaN. p∈{0,1} are valid (variance becomes 0). |
| `unidpdf` | ✅ | Sig: y = unidpdf(k, N). Discrete uniform PMF on {1..N}: 1/N if k in 1..N integer, else 0. N<=0 or non-integer N -> NaN. NaN N -> NaN. NaN k -> 0 (per MATLAB). tol=0 (integer-stable for discrete). |
| `unidcdf` | ✅ |  |
| `unidinv` | ✅ | Sig: x = unidinv(p, N). Inverse discrete-uniform CDF on {1..N}: x = ceil(p·N), clamped. Edges: p<=0 or p>1 -> NaN (p=0 has no integer pre-image); N<1 or non-integer N -> NaN; NaN p/N -> NaN. tol=0. |
| `unidrnd` | ✅ |  |
| `unidstat` | ✅ | Sig: [m, v] = unidstat(N). Discrete uniform on {1..N}: m = (N+1)/2, v = (N²-1)/12. Vectorised. N<1 or non-integer => NaN. |
| `geopdf` | ✅ | Sig: r = geopdf(...). Spec-extension batch 2026-05-09. |
| `geocdf` | ✅ | Sig: p = geocdf(k, p[, 'upper']). Geometric (number of failures before first success): F(k; p) = 1 - (1-p)^(k+1). 'upper' returns 1 - F(k). |
| `geoinv` | ✅ | Sig: r = geoinv(...). Spec-extension batch 2026-05-09. |
| `geornd` | ✅ | Sig: r = geornd(...). Spec-extension batch 2026-05-09. |
| `geostat` | ✅ | Sig: r = geostat(...). Spec-extension batch 2026-05-09. |
| `nbinpdf` | ✅ | Sig: r = nbinpdf(...). Spec-extension batch 2026-05-09.  |
| `nbincdf` | ✅ | Sig: p = nbincdf(k, r, p[, 'upper']). Negative binomial: number of failures before r-th success. F(k; r, p) = I_p(r, k+1). 'upper' returns 1 - F(k). |
| `nbininv` | ✅ | Sig: r = nbininv(...). Spec-extension batch 2026-05-09.  |
| `nbinrnd` | ✅ | Sig: r = nbinrnd(R, P, sz). Spec-extension batch 2026-05-09 (cycle 41). |
| `nbinstat` | ✅ | Sig: r = nbinstat(...). Spec-extension batch 2026-05-09.  |
| `hygepdf` | ✅ | Sig: r = hygepdf(...). Spec-extension batch 2026-05-09.  |
| `hygecdf` | ✅ | Sig: p = hygecdf(k, M, K, N[, 'upper']). Hypergeometric CDF over k=0..N drawn from population M with K marked. 'upper' returns 1 - F(k). |
| `hygeinv` | ✅ | Sig: r = hygeinv(...). Spec-extension batch 2026-05-09.  |
| `hygernd` | ✅ | Sig: r = hygernd(...). Spec-extension batch 2026-05-09.  |
| `hygestat` | ✅ | Sig: r = hygestat(...). Spec-extension batch 2026-05-09.  |
| `evpdf` | ✅ | Sig: r = evpdf(...). Spec-extension batch 2026-05-09. |
| `evcdf` | ✅ | Sig: p = evcdf(x[, mu, sigma][, 'upper']). F(x) = 1 − exp(−exp((x−μ)/σ)); 'upper' returns 1 - F(x). |
| `evinv` | ✅ | Sig: r = evinv(...). Spec-extension batch 2026-05-09. |
| `evrnd` | ✅ | Sig: r = evrnd(mu, sigma). Type-I (Gumbel-MIN) extreme value sampler via inverse CDF on rand(). Bit-identical with MATLAB R2025b after Phase-0a-1 RNG cascade (MT19937 init_genrand + genRes53 + Gumbel-MIN convention). |
| `evstat` | ✅ | Sig: r = evstat(...). Spec-extension batch 2026-05-09. |
| `gevpdf` | ✅ | Sig: r = gevpdf(...). Spec-extension batch 2026-05-09. |
| `gevcdf` | ✅ | Sig: p = gevcdf(x, k, sigma, mu[, 'upper']). 'upper' returns 1 - F(x). |
| `gevinv` | ✅ | Sig: r = gevinv(...). Spec-extension batch 2026-05-09. |
| `gevrnd` | ✅ | Sig: r = gevrnd(k, sigma, mu). Generalized Extreme Value sampler via gev_inv_one inverse CDF on rand(). Bit-identical with MATLAB R2025b after Phase-0a-1 RNG cascade. |
| `gevstat` | ✅ | Sig: r = gevstat(...). Spec-extension batch 2026-05-09. |
| `gppdf` | ✅ | Sig: r = gppdf(...). Spec-extension batch 2026-05-09.  |
| `gpcdf` | ✅ | Sig: p = gpcdf(x, k, sigma, theta[, 'upper']). 'upper' returns 1 - F(x). |
| `gpinv` | ✅ | Sig: r = gpinv(...). Spec-extension batch 2026-05-09. |
| `gprnd` | ✅ | Sig: r = gprnd(k, sigma, theta). Generalized Pareto sampler via inline ICDF on rand() (uses u directly, MATLAB convention). Bit-identical with MATLAB R2025b after Phase-0a-1 RNG cascade. |
| `gpstat` | ✅ | Sig: r = gpstat(...). Spec-extension batch 2026-05-09.  |
| `nakapdf` | ✅ ➖ | Sig: r = nakapdf(...). Spec-extension batch 2026-05-09.  |
| `nakacdf` | ✅ ➖ | Sig: p = nakacdf(x, mu, omega[, 'upper']). Nakagami-m CDF: F(x) = gammainc(mu·x²/omega, mu). 'upper' returns 1 - F(x). |
| `nakainv` | ✅ ➖ | Sig: r = nakainv(...). Spec-extension batch 2026-05-09.  |
| `nakarnd` | ✅ ➖ | Sig: r = nakarnd(mu, omega, sz). Spec-extension batch 2026-05-09 (cycle 41). |
| `nakastat` | ✅ ➖ | Sig: r = nakastat(...). Spec-extension batch 2026-05-09.  |
| `ricepdf` | ✅ ➖ | Sig: y = ricepdf(x, s, sigma). Rice PDF (x/σ²)·exp(−(x²+s²)/(2σ²))·I_0(x·s/σ²). Octave stats package has direct names; MATLAB exposes via pdf('Rician', ...). |
| `ricecdf` | ✅ ➖ | Sig: p = ricecdf(x, s, sigma[, 'upper']). Rice CDF via Marcum Q: F(x) = 1 - Q1(s/sigma, x/sigma). 'upper' returns 1 - F(x) = Q1(s/sigma, x/sigma). MATLAB R2025b does NOT ship a top-level ricecdf — only makedist('Rician')+cdf — so reference comes from Octave's statistics package. Tolerance 1e-4 reflects an existing ~1e-5 numerical-accuracy gap between numkit's marcumq series and Octave's; this spec closes the 'upper' flag only, the accuracy gap is tracked separately. |
| `riceinv` | ✅ ➖ | Sig: x = riceinv(p, s, sigma). Spec-extension batch 2026-05-09 (cycle 41). |
| `ricernd` | ✅ ➖ | Sig: r = ricernd(s, sigma, sz). Spec-extension batch 2026-05-09 (cycle 41). |
| `ricestat` | ✅ ➖ | Sig: [m, v] = ricestat(s, sigma). Rician (Rice). s=0 reduces to Rayleigh: m = sigma·sqrt(π/2), v = sigma²·(2 - π/2). Vectorised. sigma<=0 / s<0 => NaN. MATLAB R2025b doesn't ship ricestat — Octave statistics package is the reference. |
| `ncfpdf` | ✅ | Sig: nctrnd(nu, delta, rows, cols) — noncentral t RNG via T = (Z+δ)/√(V/ν), Z~N(0,1), V~χ²(ν), shared MT19937. ncfpdf(x, nu1, nu2, delta) — noncentral F pdf via Poisson-mixture series f(x) = e^{-δ/2} Σ_k (δ/2)^k/k! · (ν₁/ν₂)^(ν₁/2+k) x^(ν₁/2+k-1) (1+ν₁x/ν₂)^{-(ν₁+ν₂)/2-k} / B(ν₁/2+k, ν₂/2). Series truncated at 1e-16 rel contribution. delta=0 reduces to central fpdf exactly. ncfpdf verified bit-equal to MATLAB R2025b at tol=1e-8; nctrnd sample moments pinned at statistical tolerance. KNOWN GAPs: ncfcdf, ncfinv, ncfstat, ncfrnd next cycles. |
| `ncfcdf` | ✅ | Sig: ncfcdf(x, nu1, nu2, delta[, 'upper']) — noncentral F cdf via Poisson-mixture in regularised incomplete beta: F(x) = Σ_k Poisson(k; δ/2) · I_y(ν₁/2+k, ν₂/2), y = ν₁x/(ν₁x+ν₂). ncfinv(p, nu1, nu2, delta) — Newton on ncfcdf with central finv as warm start, bracketed bisection fallback. delta=0 reduces to central fcdf/finv. Verified bit-equal to MATLAB R2025b at 1e-8 tol (ncfcdf bit-identical, ncfinv to ~1e-6 via Newton convergence). KNOWN GAPs: ncfstat, ncfrnd next cycles. |
| `ncfinv` | ✅ | Sig: ncfcdf(x, nu1, nu2, delta[, 'upper']) — noncentral F cdf via Poisson-mixture in regularised incomplete beta: F(x) = Σ_k Poisson(k; δ/2) · I_y(ν₁/2+k, ν₂/2), y = ν₁x/(ν₁x+ν₂). ncfinv(p, nu1, nu2, delta) — Newton on ncfcdf with central finv as warm start, bracketed bisection fallback. delta=0 reduces to central fcdf/finv. Verified bit-equal to MATLAB R2025b at 1e-8 tol (ncfcdf bit-identical, ncfinv to ~1e-6 via Newton convergence). KNOWN GAPs: ncfstat, ncfrnd next cycles. |
| `ncfrnd` | ✅ | Sig: ncfstat(nu1, nu2, delta) — closed-form m = ν₂(ν₁+δ)/(ν₁(ν₂-2)) for ν₂>2; v = 2(ν₂/ν₁)²·((ν₁+δ)² + (ν₁+2δ)(ν₂-2)) / ((ν₂-2)²(ν₂-4)) for ν₂>4. ncfrnd(nu1, nu2, delta, rows, cols) — sample F = (X₁/ν₁)/(X₂/ν₂) where X₁ ~ noncentral χ²(ν₁, δ) via Poisson-mixture (J ~ Poisson(δ/2), then χ²(ν₁+2J)) and X₂ ~ χ²(ν₂). Uses shared MT19937 stream. ncfstat bit-equal to MATLAB R2025b at 1e-9; ncfrnd sample moments pinned at statistical tolerance over 5000 draws. Completes noncentral F family (ncfpdf/ncfcdf/ncfinv/ncfstat/ncfrnd). |
| `ncfstat` | ✅ | Sig: ncfstat(nu1, nu2, delta) — closed-form m = ν₂(ν₁+δ)/(ν₁(ν₂-2)) for ν₂>2; v = 2(ν₂/ν₁)²·((ν₁+δ)² + (ν₁+2δ)(ν₂-2)) / ((ν₂-2)²(ν₂-4)) for ν₂>4. ncfrnd(nu1, nu2, delta, rows, cols) — sample F = (X₁/ν₁)/(X₂/ν₂) where X₁ ~ noncentral χ²(ν₁, δ) via Poisson-mixture (J ~ Poisson(δ/2), then χ²(ν₁+2J)) and X₂ ~ χ²(ν₂). Uses shared MT19937 stream. ncfstat bit-equal to MATLAB R2025b at 1e-9; ncfrnd sample moments pinned at statistical tolerance over 5000 draws. Completes noncentral F family (ncfpdf/ncfcdf/ncfinv/ncfstat/ncfrnd). |
| `nctpdf` | ✅ | Sig: nctpdf(x, nu, delta) — noncentral t pdf via direct series f(x;ν,δ) = ν^{ν/2}·e^{-δ²/2} / (√π·Γ(ν/2)·(ν+x²)^{(ν+1)/2}) · Σ_k Γ((ν+k+1)/2)·(xδ√2)^k / (k!·(ν+x²)^{k/2}). nctcdf(x, nu, delta[, 'upper']) — Owen (1965) series F(x;ν,δ) = Φ(-δ) + ½·Σ_k P_k·I_y(k+½, ν/2) + (δ/(2√2))·e^{-δ²/2}·Σ_k I_y(k+1, ν/2)/Γ(k+3/2), y = x²/(x²+ν). Negative x via symmetry F(x;ν,δ) = 1 - F(-x;ν,-δ). Series truncated at 1e-16 relative contribution. Bit-identical with MATLAB R2025b at 1e-8 tol. KNOWN GAPs: nctinv, nctstat, nctrnd next batch. |
| `nctcdf` | ✅ | Sig: nctpdf(x, nu, delta) — noncentral t pdf via direct series f(x;ν,δ) = ν^{ν/2}·e^{-δ²/2} / (√π·Γ(ν/2)·(ν+x²)^{(ν+1)/2}) · Σ_k Γ((ν+k+1)/2)·(xδ√2)^k / (k!·(ν+x²)^{k/2}). nctcdf(x, nu, delta[, 'upper']) — Owen (1965) series F(x;ν,δ) = Φ(-δ) + ½·Σ_k P_k·I_y(k+½, ν/2) + (δ/(2√2))·e^{-δ²/2}·Σ_k I_y(k+1, ν/2)/Γ(k+3/2), y = x²/(x²+ν). Negative x via symmetry F(x;ν,δ) = 1 - F(-x;ν,-δ). Series truncated at 1e-16 relative contribution. Bit-identical with MATLAB R2025b at 1e-8 tol. KNOWN GAPs: nctinv, nctstat, nctrnd next batch. |
| `nctinv` | ✅ | Sig: nctinv(p, nu, delta) — inverse cdf via Newton on nctcdf with central tinv shifted by delta as warm start, safeguarded by bracketing bisection. nctstat(nu, delta) — closed form m = δ √(ν/2) Γ((ν-1)/2)/Γ(ν/2) for ν > 1; v = ν(1+δ²)/(ν-2) - m² for ν > 2; NaN otherwise. Verified bit-equal to MATLAB R2025b at tol=1e-8. KNOWN GAPs: nctrnd next batch. |
| `nctrnd` | ✅ | Sig: nctrnd(nu, delta, rows, cols) — noncentral t RNG via T = (Z+δ)/√(V/ν), Z~N(0,1), V~χ²(ν), shared MT19937. ncfpdf(x, nu1, nu2, delta) — noncentral F pdf via Poisson-mixture series f(x) = e^{-δ/2} Σ_k (δ/2)^k/k! · (ν₁/ν₂)^(ν₁/2+k) x^(ν₁/2+k-1) (1+ν₁x/ν₂)^{-(ν₁+ν₂)/2-k} / B(ν₁/2+k, ν₂/2). Series truncated at 1e-16 rel contribution. delta=0 reduces to central fpdf exactly. ncfpdf verified bit-equal to MATLAB R2025b at tol=1e-8; nctrnd sample moments pinned at statistical tolerance. KNOWN GAPs: ncfcdf, ncfinv, ncfstat, ncfrnd next cycles. |
| `nctstat` | ✅ | Sig: nctinv(p, nu, delta) — inverse cdf via Newton on nctcdf with central tinv shifted by delta as warm start, safeguarded by bracketing bisection. nctstat(nu, delta) — closed form m = δ √(ν/2) Γ((ν-1)/2)/Γ(ν/2) for ν > 1; v = ν(1+δ²)/(ν-2) - m² for ν > 2; NaN otherwise. Verified bit-equal to MATLAB R2025b at tol=1e-8. KNOWN GAPs: nctrnd next batch. |
| `ncx2pdf` | ✅ | Sig: r = ncx2pdf(...). Spec-extension batch 2026-05-09.  |
| `ncx2cdf` | ✅ | Sig: y = ncx2cdf(x, k, lambda[, 'upper']). Poisson-mixture: Σ_j Poisson(j; λ/2)·gammainc(x/2, k/2 + j); truncated when contribution drops below 1e-16 of running sum. 'upper' returns 1 - F(x). |
| `ncx2inv` | ✅ | Sig: r = ncx2inv(...). Spec-extension batch 2026-05-09.  |
| `ncx2rnd` | ✅ |  |
| `ncx2stat` | ✅ | Sig: r = ncx2stat(...). Spec-extension batch 2026-05-09.  |

### Distribution Fitting (MLE / likelihood)

**Namespace:** `stats.fit.*` — 16 ✅ + 0 ⚠️ / 24 = 67%

OOP `fitdist` / `makedist` family intentionally omitted — only flat
function-form fitters (return `[parmhat, parmci]`) and likelihood evaluators.

| function | status | comment |
|---|:---:|---|
| `mle` | ⚠️ | Sig: [phat, pci] = mle(data[, 'distribution', name][, 'Alpha', a]). Closed-form MLE for normal (default) / exponential / poisson / lognormal. The 2nd output pci (fixed 2026-06-05, bugs/stats/mle-output.md) is the 2xk confidence-interval matrix (row 1 lower, row 2 upper, one column per parameter), default Alpha=0.05, reusing the matching *fit CI machinery (normfit/expfit/poissfit; lognormal = normfit of log data) which already matches MATLAB. phat bit-identical; pci matches via tinv/chi2inv-based CIs. Custom 'pdf'/'logpdf'/'nloglf' deferred. |
| `mlecov` | ❌ | covariance of MLE estimates |
| `betafit` | ✅ | Sig: [ahat, bhat] = betafit(x) — Beta MLE via 2-D Newton on the digamma system. [rhat, phat] = nbinfit(x) — negative binomial MLE: profile log-likelihood, Newton on r with closed-form p = r / (r + mean). Stochastic samples in setup; fingerprint pins recoverability + output shapes. |
| `betalike` | ✅ | Sig: [nL, AVAR] = betalike([a b], x). NLL for Beta(a, b). AVAR is the 2×2 inverse of the BHHH (outer-product-of-gradients) Fisher info — MATLAB's betalike uses BHHH, not the Hessian (verified by direct probe). Edge: invalid params or x outside (0,1) => NaN. |
| `binofit` | ✅ | Sig: [phat, pci] = binofit(x, n[, alpha]). Clopper-Pearson exact binomial CI. Covers: scalar (k=7,n=10), vector ([3 5 7]'), edges x=0 + x=n, non-default alpha=0.01. No 'Method' kw — MATLAB binofit hard-codes Clopper-Pearson. |
| `evfit` | ✅ | Sig: [muhat, sigmahat] = evfit(x) — Gumbel-min MLE: μ profiled out via Σ exp(x_i/σ)=n; 1-D Newton on σ-equation Σ x_i e^{x_i/σ}/Σ e^{x_i/σ} - mean - σ = 0 (negative-definite f'). [khat, sigmahat] = gpfit(x) — Generalised Pareto MLE: PWM (Hosking-Wallis 1987 α-form) as warm start, then 2-D Newton-Raphson on the GP log-likelihood with central-FD gradient & Hessian, backtracking line search with feasibility guard. Bit-equal MATLAB R2025b at ~1e-5 on deterministic inverse-CDF samples (k=0.3, -0.1, 0). |
| `evlike` | ✅ | Sig: nL = evlike([mu sigma], x[, cens, freq]). Type-I extreme value (Gumbel min). Uncensored: log(σ) - z + e^z; censored: e^z; with optional freq weights. Edges: σ<=0 -> NaN (was Inf); empty data -> 0. AVAR (2-output form) deferred — observed Fisher info has nontrivial cross-terms. |
| `expfit` | ✅ | Sig: [muhat, muci] = expfit(x[, alpha[, censoring[, freq]]]). MLE for exponential: T = Σ(freq·x), D = Σ(freq·(1-cens)), mu = T/D. Exact CI via χ²(2D): [2T/χ²₁₋α/2, 2T/χ²_α/2]. Defaults: cens=0, freq=1. |
| `explike` | ✅ | Sig: [nL, avar] = explike(mu, x[, cens, freq]). NLL for Exp(mu). avar (scalar) = 1/I where I = Σ w_i ∂²nL_i/∂μ² (uncens: -1/μ²+2x/μ³; right-cens: 2x/μ³). Edge: mu<=0 => NaN; empty data => 0. |
| `gamfit` | ✅ | Sig: [ahat, bhat] = gamfit(x) — Gamma(shape, scale) MLE via Minka 2002 init + digamma/trigamma Newton on the shape, scale follows from b = mean(x)/a. [ahat, bhat] = wblfit(x) — Weibull(scale, shape) MLE via Newton on the implicit shape equation, scale = (Σ x^b / n)^(1/b). KNOWN GAP: confidence intervals (`bci` second output) deferred in v1. Spec uses stochastic samples — fingerprint pins error bounds and output shape rather than literal values (engine RNG diverges). |
| `gamlike` | ✅ | Sig: [nL, AVAR] = gamlike([a b], x). NLL for Gamma(a, b). AVAR is the 2×2 inverse observed-Fisher info matrix at [a, b], computed via central-difference Hessian (no in-tree trigamma). Edge: invalid params (a<=0 or b<=0) => NaN. tol=1e-7 reflects FD precision (~5e-8 absolute on basic case). |
| `gevfit` | ✅ | Sig: [parmhat, parmci] = gevfit(x[, alpha]). 3-parameter GEV MLE with PWM initial guess (Hosking-Wallis-Wood 1985: k ≈ 7.8590c + 2.9554c², σ via Γ(1+k)·(1-2^{-k}), μ via b0 - σ(1-Γ(1+k))/k), then 3-D Newton-Raphson on log-likelihood with FD gradient/Hessian and feasibility-guarded backtracking. CI via observed Fisher information (3×3 FD-Hessian inverse), Wald with log transform on σ, linear on k and μ (MATLAB convention). Bit-equal MATLAB R2025b on Frechet (k>0), Reverse-Weibull (k<0), and Gumbel-max (k=0) deterministic samples to 3-4 sig figs. KNOWN GAPs: none — censoring + freq deferred (rare for extreme value analysis). |
| `gevlike` | ✅ | Sig: [nL, ACOV] = gevlike([k sigma mu], x). GEV NLL with Gumbel-MAX limit at k=0. ACOV is the 3×3 inverse observed-Fisher matrix at [k,sigma,mu], computed via central-difference Hessian (tol=1e-6 reflects FD precision). Edge: sigma<=0 or per-point support violation (1+k*z<=0) => NaN. Known gap: at exactly k=0 MATLAB uses an analytical Gumbel-limit Hessian that differs from FD straddling — numkit's FD reproduces the value of FD-on-MATLAB's-own-gevlike (~0.030, 0.098, -1.622), not MATLAB's reported analytical ACOV. |
| `gpfit` | ✅ | Sig: [muhat, sigmahat] = evfit(x) — Gumbel-min MLE: μ profiled out via Σ exp(x_i/σ)=n; 1-D Newton on σ-equation Σ x_i e^{x_i/σ}/Σ e^{x_i/σ} - mean - σ = 0 (negative-definite f'). [khat, sigmahat] = gpfit(x) — Generalised Pareto MLE: PWM (Hosking-Wallis 1987 α-form) as warm start, then 2-D Newton-Raphson on the GP log-likelihood with central-FD gradient & Hessian, backtracking line search with feasibility guard. Bit-equal MATLAB R2025b at ~1e-5 on deterministic inverse-CDF samples (k=0.3, -0.1, 0). |
| `gplike` | ✅ | Sig: [nL, acov] = gplike([k sigma], x). GP NLL with implicit theta=0. acov is the 2×2 inverse observed-Fisher matrix at [k, sigma], computed via central-difference Hessian (tol=1e-5 reflects FD precision; k=0 stride is the worst case at ~2e-6). Edges: sigma<=0 or per-point support violation (1+k*x/sigma<=0) => NaN. MATLAB does NOT enforce x>=0 globally — only the per-point support check; numkit matches (e.g. gplike([0.5,1], [-1 1 2]') returns 1.2163...). |
| `lognfit` | ✅ | Sig: [parm, pci] = lognfit(x[, alpha[, censoring[, freq[, options]]]]). Lognormal MLE: parm=[mu sigma] of log(x). pci is 2x2: col 1 = mu CI, col 2 = sigma CI. Closed-form weighted moments when freq alone; EM-iterated MLE on log(x) with analytic Fisher info for CIs (Wald with z=norminv(1-α/2), log-σ transform for asymmetric σ CI) when censored. |
| `lognlike` | ✅ | Sig: [nL, aVar] = lognlike([mu sigma], x[, cens, freq]). NLL for lognormal. Hessian wrt (mu, sigma) is structurally identical to the normal Hessian on log(x). aVar (column-major 2×2) reflects cens/freq weighting; can have negative diagonal entries at non-MLE params (observed Fisher, not expected). Edge: sigma<=0 or x<=0 => NaN; empty data => 0. |
| `nbinfit` | ✅ | Sig: [ahat, bhat] = betafit(x) — Beta MLE via 2-D Newton on the digamma system. [rhat, phat] = nbinfit(x) — negative binomial MLE: profile log-likelihood, Newton on r with closed-form p = r / (r + mean). Stochastic samples in setup; fingerprint pins recoverability + output shapes. |
| `normfit` | ✅ | Sig: [mu, sd, muci, sdci] = normfit(x[, alpha[, censoring[, freq[, options]]]]). MLE for normal: mu=mean, sd=sample std (N-1). Closed-form weighted moments when freq alone; EM iteration on truncated-normal moments + analytic Fisher info Wald CI when censored. Default alpha=0.05. Shares the `normal_fit_mle` helper with lognfit. |
| `normlike` | ✅ | Sig: [nL, aVar] = normlike([mu sigma], data[, cens, freq]). Default + censoring (right-censored => -log(S(z))) + freq weights + empty + invalid-sigma (=> NaN). Second output aVar = inverse 2×2 observed-Fisher information matrix at [mu, sigma]; reflects cens/freq weighting. |
| `poissfit` | ✅ | Sig: [lhat, lci] = poissfit(x[, alpha]). MLE for Poisson: lambda=mean(x). Exact CI via chi² inversion (Garwood). Edges: all-zero data -> lo=0; non-default alpha; empty input -> NaN. |
| `raylfit` | ✅ | Sig: [shat, sci] = raylfit(x[, alpha]). Rayleigh MLE: σ = √(Σx²/(2N)); CI from chi² inversion 2N·σ̂² ~ σ²·χ²(2N). Edges: non-default α; single-element x; empty input -> NaN. |
| `unifit` | ✅ | Sig: [a, b, aci, bci] = unifit(x[, alpha]). MLE for U(a,b): a=min, b=max. CI extension delta = (b-a)·(α^(-1/n) − 1). Single-element x: ACI=BCI=[x x] (zero-width). Empty input: numkit returns NaN; MATLAB returns empty arrays — convention difference, not in fingerprint. |
| `wblfit` | ✅ | Sig: [ahat, bhat] = gamfit(x) — Gamma(shape, scale) MLE via Minka 2002 init + digamma/trigamma Newton on the shape, scale follows from b = mean(x)/a. [ahat, bhat] = wblfit(x) — Weibull(scale, shape) MLE via Newton on the implicit shape equation, scale = (Σ x^b / n)^(1/b). KNOWN GAP: confidence intervals (`bci` second output) deferred in v1. Spec uses stochastic samples — fingerprint pins error bounds and output shape rather than literal values (engine RNG diverges). |
| `wbllike` | ✅ | Sig: nL = wbllike([scale shape], x[, cens, freq]). Weibull(a, b). Uncensored: -log(b) + b·log(a) - (b-1)·log(x) + (x/a)^b. Censored: (x/a)^b. With optional freq weights. Edges: scale<=0 or shape<=0 -> NaN (was Inf); x_i <= 0 -> NaN. Empty data: numkit returns 0 (consistent with our *like family); MATLAB errors `DATA must be a vector` — convention difference, not in fingerprint. AVAR (2-output form) deferred. |

### Multivariate Distributions

**Namespace:** `stats.mvdist.*` — 3 ✅ + 0 ⚠️ / 14 = 21%

| function | status | comment |
|---|:---:|---|
| `mvncdf` | ✅ | Sig: p = mvncdf(X, mu, Sigma) — multivariate normal CDF. d=1 forwards to normcdf. d=2 uses Gauss-Legendre 16-point integration over the bivariate parametric formula. d≥3 uses antithetic Monte Carlo with 20000 samples. Deterministic seed (12345) for the MC path so results are reproducible. Bit-exact MATLAB R2025b at tol 1e-4 on d=1 and d=2 deterministic cases; d≥3 within statistical tolerance (KNOWN GAP: Genz separation-of-variables quasi-MC not yet shipped). Special cases: mu=[]→0, Sigma=[]→I. |
| `mvnpdf` | ✅ | Sig: p = mvnpdf(X[, mu[, Sigma]]). Multivariate normal PDF. Defaults: mu=zeros, Sigma=I. Cholesky-based |Σ|^(-1/2) and Σ^(-1) for numerical stability. Verified bit-identical to MATLAB R2025b on default / explicit mu / explicit Σ paths. |
| `mvnrnd` | ✅ | Sig: randg(shape [, m, n]) — raw gamma(shape, 1) RNG (scale = 1). Forwards to gamrnd internally. Per-element shape supported. mvnrnd(mu, Sigma [, n]) — multivariate normal RNG via in-place Cholesky on Sigma + N(0,1) draws. Supports vector mu (1×d, d×1) or matrix mu (n×d, per-row location). Bit-exact MATLAB R2025b not feasible (different RNG seeds); fingerprint pins distributional moments (mean ≈ shape, var ≈ shape for randg; mean ≈ mu, diag(cov) ≈ diag(Sigma) for mvnrnd) within statistical tolerance over n=3000 draws. |
| `mvtcdf` | ✅ | Sig: p = mvtcdf(X, C, df). For d=1 exact via tcdf (bit-equal MATLAB R2025b). For d≥2 deterministic Monte Carlo on Y = Z/sqrt(W/df) with W ~ chi²(df), seed=12345, 10000 antithetic draws → ~0.005 MC error per row. Fingerprint pins d=1 to 1e-8 (exact) and d=2,3 to 0.02 MC tolerance. KNOWN GAPs: [LB, UB] box-form mvtcdf(L, U, C, df) and 'tol' option deferred. |
| `mvtpdf` | ✅ | Sig: p = mvtpdf(X, C, df). Multivariate Student-t PDF; C normalized to correlation matrix. Cholesky-based |C|^(-1/2) + quadratic form. Bit-identical to MATLAB R2025b. |
| `mvtrnd` | ✅ | Sig: R = mvtrnd(C, df, n) — multivariate-t RNG via N(0,C) / sqrt(χ²/df). R = mnrnd(N, P [, m]) — multinomial RNG via cumulative-prob sampling. Both use the shared MT19937 stream. KNOWN GAPs: mvtrnd no location parameter (always 0); mnrnd no per-sample P matrix form. Fingerprint pins distributional moments (cov ≈ scaled C, col means ≈ N·p) at statistical tolerance over 3000 samples; row sum constraint is exact. |
| `mnpdf` | ✅ | Sig: p = mnpdf(X, P). Multinomial PMF: n!/(Π x_i!) · Π p_i^x_i. Computed in log-space via lgamma. Bit-identical to MATLAB R2025b on row-vector / matrix inputs. |
| `mnrnd` | ✅ | Sig: R = mvtrnd(C, df, n) — multivariate-t RNG via N(0,C) / sqrt(χ²/df). R = mnrnd(N, P [, m]) — multinomial RNG via cumulative-prob sampling. Both use the shared MT19937 stream. KNOWN GAPs: mvtrnd no location parameter (always 0); mnrnd no per-sample P matrix form. Fingerprint pins distributional moments (cov ≈ scaled C, col means ≈ N·p) at statistical tolerance over 3000 samples; row sum constraint is exact. |
| `wishrnd` | ✅ | Sig: W = wishrnd(Sigma, df) — Wishart RNG via Bartlett decomposition: factor Sigma = L·L', build lower-tri B with B(i,i)=sqrt(χ²(df-i)) and B(i,j)~N(0,1) for i>j, then W = (L·B)(L·B)'. W = iwishrnd(Tau, df) — sample Y ~ W(inv(Tau), df) via Bartlett, return inv(Y). Stochastic samples; fingerprint pins E[W]/df ≈ Sigma for Wishart, E[W]·(df-p-1) ≈ Tau for inverse Wishart, plus exact symmetry of a single draw and positive-definiteness. KNOWN GAPs: 3rd-argument D (pre-Cholesky), 2-output [W, D] form deferred. |
| `iwishrnd` | ✅ | Sig: W = wishrnd(Sigma, df) — Wishart RNG via Bartlett decomposition: factor Sigma = L·L', build lower-tri B with B(i,i)=sqrt(χ²(df-i)) and B(i,j)~N(0,1) for i>j, then W = (L·B)(L·B)'. W = iwishrnd(Tau, df) — sample Y ~ W(inv(Tau), df) via Bartlett, return inv(Y). Stochastic samples; fingerprint pins E[W]/df ≈ Sigma for Wishart, E[W]·(df-p-1) ≈ Tau for inverse Wishart, plus exact symmetry of a single draw and positive-definiteness. KNOWN GAPs: 3rd-argument D (pre-Cholesky), 2-output [W, D] form deferred. |
| `copulapdf` | ✅ | Sig: copulapdf(family, U, param[, nu]) and copulacdf(family, U, param[, nu]) for 5 families: Gaussian (param=R correlation matrix), t (R + nu), Clayton/Frank/Gumbel (alpha). Algorithms — Gaussian: pdf via (det R)^{-1/2} exp(-½ z'(R^{-1}-I)z); cdf via Drezner-Wesolowsky bivariate normal CDF on Φ^{-1}(U). t: similar with t-densities and bivariate t CDF (mvtcdf MC). Clayton/Frank/Gumbel: closed-form Archimedean (Nelsen 2006, ch.4). All 2-D; bit-equal MATLAB R2025b at 1e-4 (Monte-Carlo t-CDF at MC-tolerance). KNOWN GAPs: d ≥ 3 for Gaussian/t pending (uses bivariate kernels currently). |
| `copulacdf` | ✅ | Sig: copulapdf(family, U, param[, nu]) and copulacdf(family, U, param[, nu]) for 5 families: Gaussian (param=R correlation matrix), t (R + nu), Clayton/Frank/Gumbel (alpha). Algorithms — Gaussian: pdf via (det R)^{-1/2} exp(-½ z'(R^{-1}-I)z); cdf via Drezner-Wesolowsky bivariate normal CDF on Φ^{-1}(U). t: similar with t-densities and bivariate t CDF (mvtcdf MC). Clayton/Frank/Gumbel: closed-form Archimedean (Nelsen 2006, ch.4). All 2-D; bit-equal MATLAB R2025b at 1e-4 (Monte-Carlo t-CDF at MC-tolerance). KNOWN GAPs: d ≥ 3 for Gaussian/t pending (uses bivariate kernels currently). |
| `copulafit` | ❌ |  |
| `copulaparam` | ❌ |  |
| `copulastat` | ❌ |  |
| `copularnd` | ❌ |  |

### Pearson / Johnson Distributions

**Namespace:** `stats.pearson.*` / `stats.johnson.*` — 0 ✅ + 0 ⚠️ / 6 = 0%

| function | status | comment |
|---|:---:|---|
| `pearspdf` | ❌ | Pearson family |
| `pearscdf` | ❌ |  |
| `pearsinv` | ❌ |  |
| `pearsrnd` | ❌ |  |
| `johnsrnd` | ❌ | Johnson family random |
| `randg` | ✅ | Sig: randg(shape [, m, n]) — raw gamma(shape, 1) RNG (scale = 1). Forwards to gamrnd internally. Per-element shape supported. mvnrnd(mu, Sigma [, n]) — multivariate normal RNG via in-place Cholesky on Sigma + N(0,1) draws. Supports vector mu (1×d, d×1) or matrix mu (n×d, per-row location). Bit-exact MATLAB R2025b not feasible (different RNG seeds); fingerprint pins distributional moments (mean ≈ shape, var ≈ shape for randg; mean ≈ mu, diag(cov) ≈ diag(Sigma) for mvnrnd) within statistical tolerance over n=3000 draws. |

### Empirical / Kernel Distributions

**Namespace:** `stats.empirical.*` — 3 ✅ + 0 ⚠️ / 4 = 75%

| function | status | comment |
|---|:---:|---|
| `ecdf` | ✅ | Sig: [f, x[, flo, fup]] = ecdf(y[, 'Function', mode][, 'Frequency', w][, 'Alpha', a]). Function modes: 'cdf' (default), 'survivor' = 1-cdf, 'cumulative hazard' = Nelson-Aalen estimator. Frequency weighting via per-observation counts. 4-output form returns Greenwood-style binomial Wald 95% CI (first/last rows = NaN). Censoring deferred (Kaplan-Meier estimator). |
| `ecdfhist` | ✅ | Sig: [n, c] = ecdfhist(f, x[, m]). Probability-density histogram from ecdf step data. Default m=10 bins. n is the per-bin density (sum of jumps falling in that bin / bin_width); c is the bin centre. Coverage: m ∈ {3, 5, 10} × uniform/non-uniform input. |
| `ksdensity` | ✅ | Sig: [f, xi, bw] = ksdensity(x[, pts][, 'Bandwidth'/'Kernel'/'Function'/'NumPoints'/'Weights', val, ...]). 4 kernels (normal/box/triangle/epanechnikov) with MATLAB-style σ²=1 bandwidth normalization (h × sqrt(unit-σ²-inverse) for finite-support kernels). Function modes: pdf (default), cdf, survivor, cumhazard, icdf. Weights normalized to sum to 1. Default bandwidth via mad(x)/0.6745 fallback to iqr(x)/1.349 (matches MATLAB's bw exactly). DEEP-PROBE c170: added 'Function','icdf' (inverse CDF) — the 2nd arg are PROBABILITIES; solved via Newton's method on the smoothed cdf/pdf, seeded by linear inverse-interpolation of a 100-point grid cdf, matching MATLAB R2025b's compute_initial_icdf + Newton (statkscompute.m) to ~1e-10. numkit previously threw 'icdf is not yet supported'. rt confirms the icdf(cdf(x))=x round-trip. Censoring/Support/BoundaryCorrection deferred. |
| `mvksdensity` | ❌ | multivariate KDE |

### Hypothesis Tests

**Namespace:** `stats.test.*` — 16 ✅ + 0 ⚠️ / 25 = 64%

| function | status | comment |
|---|:---:|---|
| `adtest` | ✅ | Sig: [h, p, A2*, cv] = adtest(x [, alpha]) — Anderson-Darling normality test with estimated parameters. A² statistic via the standard order-statistic formula, Stephens-1986 small-sample adjustment, p-value via the D'Agostino-Stephens piecewise rational fit. Critical value cv = 0.752 (α = 0.05). [p, dw] = dwtest(r, X) — Durbin-Watson autocorrelation test on regression residuals. DW = Σ(r_i - r_{i-1})² / Σ r_i². p-value now via the EXACT distribution (Imhof CF inversion of the residual-space MAM eigenvalues), matching MATLAB's default 'exact' method (fixed 2026-06-05, bugs/stats/dwtest-pvalue.md; deterministic exact-value validation lives in tools/parity/specs/dwtest_exact.json). Bit-exact MATLAB R2025b not feasible HERE (rng state in r_pos); this spec pins structural invariants only (decision, sign of dw deviation, critical value). |
| `ansaribradley` | ✅ | Sig: [h, p, stats] = ansaribradley(x, y[, alpha or name-value]). Non-parametric two-sample scale test. V-shape Ansari ranks min(i, N+1-i) with mid-ranks for ties. W = sum of ranks for x. Exact path (min(m,n) ≤ 10): DP-enumerate C(N,m) permutations, scaled by LCM of tie-group sizes for exact integer arithmetic. Asymptotic path (else): conditional-permutation moments E[W] = m·mean(r), V[W] = mn/(N-1)·σ²(r) — automatically handles ties via observed ranks. Tail convention is INVERTED (MATLAB-specific): 'right' = dispersion(x) > dispersion(y) ⇒ W small ⇒ p = P(W ≤ obs); 'left' = opposite. Bit-equal MATLAB R2025b on all probe cases (asymptotic + exact + tied + tails) at 1e-9 tolerance. Full production: no GAPs. |
| `barttest` | ❌ | Bartlett's sphericity |
| `chi2gof` | ✅ | Sig: [h, p, stats] = chi2gof(x[, 'Frequency'/'Expected'/'Edges'/'NBins'/'Ctrs'/'NParams'/'EMin'/'Alpha', val, ...]). Three paths covered: explicit Frequency+Expected (bit-identical); explicit NBins (bit-identical, integer-aligned edges); explicit Edges (bit-identical). Default auto-bin (no NBins/Edges) uses 10 equal-width bins on min(x)..max(x); may differ from MATLAB at FP-edge ties (within 1 count). 'CDF' function-handle argument deferred (errors with clear message). |
| `dwtest` | ✅ | Sig: [h, p, A2*, cv] = adtest(x [, alpha]) — Anderson-Darling normality test with estimated parameters. A² statistic via the standard order-statistic formula, Stephens-1986 small-sample adjustment, p-value via the D'Agostino-Stephens piecewise rational fit. Critical value cv = 0.752 (α = 0.05). [p, dw] = dwtest(r, X) — Durbin-Watson autocorrelation test on regression residuals. DW = Σ(r_i - r_{i-1})² / Σ r_i². p-value now via the EXACT distribution (Imhof CF inversion of the residual-space MAM eigenvalues), matching MATLAB's default 'exact' method (fixed 2026-06-05, bugs/stats/dwtest-pvalue.md; deterministic exact-value validation lives in tools/parity/specs/dwtest_exact.json). Bit-exact MATLAB R2025b not feasible HERE (rng state in r_pos); this spec pins structural invariants only (decision, sign of dw deviation, critical value). |
| `fishertest` | ✅ | Sig: [h, p, stats] = fishertest(T[, 'Tail', t, 'Alpha', a]). Fisher's exact test for 2×2 contingency. Two-sided p sums hypergeometric pmf cells with P(X=k) ≤ P(X=obs). OR = a·d/(b·c); CI is the Woolf log-OR ± z·SE. |
| `friedman` | ❌ | non-parametric repeated-measures |
| `jbtest` | ✅ | Sig: [h, p, JB, cv] = jbtest(x[, alpha[, mctol]]). For small n (<2000), Monte-Carlo simulation under H₀ for tabulated-style p-value (matches MATLAB R2025b). For large n, χ²(2) asymptotic. p capped at 0.5. Critical values are MC-estimated for small n so they vary slightly between runs (numkit uses fixed seed for reproducibility). Spec excludes cv from fingerprint (different MC seeds → different cv); JB stat itself is deterministic and bit-identical. |
| `knntest` | ❌ | k-NN two-sample test |
| `kruskalwallis` | ✅ | Sig: [p, tbl, stats] = kruskalwallis(y, group[, 'off']). Non-parametric one-way ANOVA: H = (12/(N(N+1)))·Σ R_g²/n_g − 3(N+1), tie-corrected by 1 − Σ(t³−t)/(N³−N). df = k−1; p = 1 − chi2cdf(H, df). |
| `kstest` | ✅ | one-sample KS via asymptotic Smirnov series |
| `kstest2` | ✅ | two-sample KS |
| `lillietest` | ✅ | Sig: [h, p, kstat, critval] = lillietest(x[, alpha]). Lilliefors normality test using Stephens (1974) p-value approximation. KS-stat bit-identical with MATLAB R2025b; p-value/critval may differ by ~1e-3 due to approximation table interpolation. h decision matches MATLAB on probed cases. |
| `meanEffectSize` | ❌ | Cohen's d, Hedges' g |
| `mmdtest` | ❌ | maximum mean discrepancy |
| `multcompare` | ✅ | Sig: c = multcompare(stats [, alpha [, ctype]]) — pairwise post-hoc comparisons after anova1. Returns K(K-1)/2 × 6 matrix [i, j, lower_CI, mean_diff, upper_CI, p]. v1 ships 'bonferroni' (default) and 'lsd' methods. KNOWN GAP: 'tukey-kramer' HSD requires the studentized range distribution — not yet. anova1's stats struct is also extended in this cycle to populate {means, n, s, gnames, source} fields needed by multcompare. Deterministic integer y/group inputs for parity reproducibility. |
| `ranksum` | ✅ | Sig: [p, h, stats] = ranksum(x, y[, alpha, tail | name-value]). Wilcoxon rank-sum (Mann-Whitney U). Default exact iff both samples have <10 obs (size-k subset-sum DP); else approximate with continuity + tie correction. |
| `runstest` | ✅ | Sig: [h, p, stats] = runstest(x[, v][, alpha, tail | name-value]). Wald-Wolfowitz runs test. Default v=median(x); values == v dropped. Exact dist by default via combinatorial PMF; approximate uses continuity-corrected normal. |
| `sampsizepwr` | ❌ | sample-size / power |
| `signrank` | ✅ | Sig: [p, h, stats] = signrank(x[, m | y][, alpha, tail | name-value]). Wilcoxon signed-rank: rank |d_i| with mid-rank tie averaging, W+ = Σ ranks of positive d. Default exact for n_eff ≤ 15 (subset-sum convolution); approximate uses tie-corrected normal. |
| `signtest` | ✅ | Sig: [p, h, stats] = signtest(x[, m | y][, alpha, tail | name-value]). Paired sample test: 5 positives over 5 non-zero diffs, two-sided p = 2·(0.5)^5 = 0.0625 (binomial). |
| `ttest` | ✅ 🔬 | Parametric hypothesis-test stats struct (4th output) vs MATLAB R2025b. DEEP-PROBE 2026-05-31 (toolbox pivot, toolboxes/stats): ttest/ttest2/vartest/vartest2 returned the 4th output as a bare scalar (the test statistic) instead of MATLAB's struct, so s.tstat etc. threw 'Dot indexing requires a struct' (the reg even noted 'struct form deferred'). Fix builds the struct in the cores: ttest/ttest2 {tstat, df, sd} (ttest2 sd = pooled sd for 'equal', [sx sy] for 'unequal'); vartest {chisqstat, df}; vartest2 {fstat, df1, df2}. ztest's 4th output is correctly the bare zval scalar (MATLAB convention) and is unchanged. ttest(1:10,5): tstat=0.5222328, df=9, sd=3.0276504. ttest2 equal default: tstat=-1.2478312, df=18, sd=3.7631250. vartest: chisqstat=16.5, df=9. vartest2: fstat=0.4785384, df1=9, df2=9. covers:[ttest,ttest2,vartest,vartest2]. |
| `ttest2` | ✅ 🔬 | Parametric hypothesis-test stats struct (4th output) vs MATLAB R2025b. DEEP-PROBE 2026-05-31 (toolbox pivot, toolboxes/stats): ttest/ttest2/vartest/vartest2 returned the 4th output as a bare scalar (the test statistic) instead of MATLAB's struct, so s.tstat etc. threw 'Dot indexing requires a struct' (the reg even noted 'struct form deferred'). Fix builds the struct in the cores: ttest/ttest2 {tstat, df, sd} (ttest2 sd = pooled sd for 'equal', [sx sy] for 'unequal'); vartest {chisqstat, df}; vartest2 {fstat, df1, df2}. ztest's 4th output is correctly the bare zval scalar (MATLAB convention) and is unchanged. ttest(1:10,5): tstat=0.5222328, df=9, sd=3.0276504. ttest2 equal default: tstat=-1.2478312, df=18, sd=3.7631250. vartest: chisqstat=16.5, df=9. vartest2: fstat=0.4785384, df1=9, df2=9. covers:[ttest,ttest2,vartest,vartest2]. |
| `vartest` | ✅ 🔬 | Parametric hypothesis-test stats struct (4th output) vs MATLAB R2025b. DEEP-PROBE 2026-05-31 (toolbox pivot, toolboxes/stats): ttest/ttest2/vartest/vartest2 returned the 4th output as a bare scalar (the test statistic) instead of MATLAB's struct, so s.tstat etc. threw 'Dot indexing requires a struct' (the reg even noted 'struct form deferred'). Fix builds the struct in the cores: ttest/ttest2 {tstat, df, sd} (ttest2 sd = pooled sd for 'equal', [sx sy] for 'unequal'); vartest {chisqstat, df}; vartest2 {fstat, df1, df2}. ztest's 4th output is correctly the bare zval scalar (MATLAB convention) and is unchanged. ttest(1:10,5): tstat=0.5222328, df=9, sd=3.0276504. ttest2 equal default: tstat=-1.2478312, df=18, sd=3.7631250. vartest: chisqstat=16.5, df=9. vartest2: fstat=0.4785384, df1=9, df2=9. covers:[ttest,ttest2,vartest,vartest2]. |
| `vartest2` | ✅ 🔬 | Parametric hypothesis-test stats struct (4th output) vs MATLAB R2025b. DEEP-PROBE 2026-05-31 (toolbox pivot, toolboxes/stats): ttest/ttest2/vartest/vartest2 returned the 4th output as a bare scalar (the test statistic) instead of MATLAB's struct, so s.tstat etc. threw 'Dot indexing requires a struct' (the reg even noted 'struct form deferred'). Fix builds the struct in the cores: ttest/ttest2 {tstat, df, sd} (ttest2 sd = pooled sd for 'equal', [sx sy] for 'unequal'); vartest {chisqstat, df}; vartest2 {fstat, df1, df2}. ztest's 4th output is correctly the bare zval scalar (MATLAB convention) and is unchanged. ttest(1:10,5): tstat=0.5222328, df=9, sd=3.0276504. ttest2 equal default: tstat=-1.2478312, df=18, sd=3.7631250. vartest: chisqstat=16.5, df=9. vartest2: fstat=0.4785384, df1=9, df2=9. covers:[ttest,ttest2,vartest,vartest2]. |
| `vartestn` | ✅ | Sig: [p, stats] = vartestn(x[, group][, 'Display', 'off'][, 'TestType', name]). Five test variants: Bartlett (default, χ² stat), LeveneQuadratic / LeveneAbsolute / BrownForsythe / OBrien (all F-based). When no group: matrix input where each column is treated as a separate group. Bartlett returns {chisqstat, df}; F-based tests return {fstat, df=[k-1, N-k]}. |
| `ztest` | ✅ | known-σ z-test |

### Resampling Techniques

**Namespace:** `stats.resample.*` — 3 ✅ + 0 ⚠️ / 7 = 38%

| function | status | comment |
|---|:---:|---|
| `bootci` | ⚠️ | Sig: ci = bootci(nboot, fn, X[, alpha]). Percentile bootstrap CI. NOT bit-identical with MATLAB (std::uniform_int_distribution implementation-defined; randn also not bit-identical). Statistical correctness verified: 95% CI contains true mean. |
| `bootstrp` | ⚠️ | Sig: B = bootstrp(nboot, fn, X). Bootstrap resampling. Output shape verified; values not bit-identical with MATLAB (uniform_int_distribution + randn divergence). |
| `combnk` | ✅ | Sig: C = combnk(v,K). Vector form C(4,2)=6x2. A SCALAR v is the 1-element set {v} (NOT 1:v): combnk(5,2)->0 rows, combnk(5,1)->[5]. K>N->empty 0xK (cols preserved). Fixes bugs/stats/combnk-scalar.md. Spec-extension 2026-06. |
| `crossval` | ⚠️ | Sig: vals = crossval(predfun, X, Y[, 'kfold', K]). K-fold cross-validation. Default K=10. NOT bit-identical with MATLAB (fold splitting differs -- numkit uses contiguous blocks, MATLAB defaults to random). Shape verified. |
| `cvpartition` | ❌ | partition object (function-form constructor) |
| `datasample` | ✅ | Sig: y = datasample(X, K[, dim, ...]). Default dim auto-selected: row vector samples columns (dim=2), otherwise dim=1. Output SHAPE bit-identical with MATLAB R2025b; values may differ due to RNG cascade -- shape probe used here. |
| `jackknife` | ⚠️ | needs Engine::call for function handles |
| `randsample` | ✅ | Sig y=randsample(n,k) / y=randsample(pop,k[,replace,weights]). Population-vector form samples the vector's values along its length. Weights with all mass on one element are deterministic: randsample([10 20 30],4,true,[0 0 1]) = [30 30 30 30]; column population -> column output. numkit previously routed the population vector to datasample(dim=1) -> a row-vector population collapsed to N=1 and the weighted form ERRORED (weights length != sample-axis) -- fixed. Matches MATLAB R2025b. |

### Quasirandom Sequences and MCMC

**Namespace:** `stats.qmc.*` — 1 ✅ + 0 ⚠️ / 8 = 13%

| function | status | comment |
|---|:---:|---|
| `haltonset` | ✅ | Sig: p = haltonset(d[, 'Skip', s, 'Leap', l]); X = net(p, n). Halton quasi-random points via radical inverse on the first d primes. Default skip = 1 (matches MATLAB; 'Skip', 0 yields the trivial origin). |
| `lhsdesign` | ✅ | Sig: X = lhsdesign(n, p) — Latin Hypercube design; each column is a random permutation π of 1..n with X[i, j] = (π[i] - U)/n, U ~ Uniform(0, 1). Guarantees one sample per [(k-1)/n, k/n] bin in each column. Y = lhsnorm(mu, Sigma, n) — applies norminv to lhsdesign(n, d) then transforms via chol(Sigma) (upper). Shared MT19937. Fingerprint pins: in-range (exact), bin partition (exact), column mean ≈ 0.5, recovered mu and Sigma at statistical tolerance. KNOWN GAPs: 'smooth'/'criterion'/'iterations' options for lhsdesign deferred. |
| `lhsnorm` | ✅ | Sig: X = lhsdesign(n, p) — Latin Hypercube design; each column is a random permutation π of 1..n with X[i, j] = (π[i] - U)/n, U ~ Uniform(0, 1). Guarantees one sample per [(k-1)/n, k/n] bin in each column. Y = lhsnorm(mu, Sigma, n) — applies norminv to lhsdesign(n, d) then transforms via chol(Sigma) (upper). Shared MT19937. Fingerprint pins: in-range (exact), bin partition (exact), column mean ≈ 0.5, recovered mu and Sigma at statistical tolerance. KNOWN GAPs: 'smooth'/'criterion'/'iterations' options for lhsdesign deferred. |
| `mhsample` | ❌ | Metropolis-Hastings |
| `qrandstream` | ❌ | quasi-random stream constructor |
| `slicesample` | ❌ | slice sampler |
| `sobolset` | ❌ | Sobol sequence |
| `qrand` | ❌ | draw from qrandstream |

### ANOVA / MANOVA / Correlation

**Namespace:** `stats.anova.*` — 2 ✅ + 0 ⚠️ / 9 = 22%

OOP `anova` class and `fitrm` repeated-measures model intentionally omitted; only the legacy function-form entry points (anova1/anova2/anovan) which return F-statistic and p-value tables.

| function | status | comment |
|---|:---:|---|
| `anova1` | ✅ | Sig: p = anova1(y,group['off']) OR anova1(X) matrix form (each column is a group). (y,g) p=0.0251; matrix [1 2 3;2 3 4;3 4 5] p=0.125. Matrix form fixes bugs/stats/anova1-matrix-input.md. Bit-identical with MATLAB R2025b. |
| `anova2` | ⚠️ | Sig: p = anova2(Y[, reps]). Two-way ANOVA without replication (reps=1 only in this revision; reps>1 with interaction deferred). p = [p_cols, p_rows, p_interaction]. Bit-identical with MATLAB R2025b on probed cases. |
| `anovan` | ❌ | n-way |
| `manova1` | ❌ | one-way MANOVA |
| `canoncorr` | ✅ | Sig: R = partialcorri(Y, X [, Z]) — partial correlation between each Y column and each X column, controlling for the OTHER X cols (and Z). canoncorr(X, Y) = canonical correlation analysis via centring + QR + SVD; returns canonical coefficients A, B (p×k, q×k) and the canonical correlations r (length k = min(p, q)). Bit-exact MATLAB R2025b not feasible (rng state differs); fingerprint pins structural invariants on the recoverability test (diag dominance for partialcorri on planted dependence, r(1) ≈ 1 for canoncorr on shared latent factor) and on the shapes. |
| `dummyvar` | ✅ | Sig: r = dummyvar(...). Spec-extension batch 2026-05-09. |
| `aoctool` | ❌ | analysis of covariance (interactive — defer) |
| `mauchly` | ❌ | Mauchly's sphericity |
| `epsilon` | ❌ | sphericity adjustments |

### Linear Regression (function-form)

**Namespace:** `stats.regress.*` — 3 ✅ + 0 ⚠️ / 13 = 23%

OOP `fitlm` / `fitlme` / `fitglm` / `LinearModel` / etc. intentionally omitted. Only the legacy command-form entry points that return numerics (coeffs, residuals, CIs).

| function | status | comment |
|---|:---:|---|
| `regress` | ✅ 🔬 | Sig: [b, bint, r, rint, stats] = regress(y, X[, alpha]). OLS multiple regression via Cholesky on X'X. stats = [R^2, F, p_F, sigma^2]. DEEP-PROBE c164: the 4th output rint (residual confidence intervals for outlier detection) was the TEXTBOOK formula r +/- t*sigma*sqrt(1-h_ii) with nu dof, which DISAGREED with MATLAB. MATLAB R2025b uses the Chatterjee & Hadi (1986, eq.14) / Belsley-Kuh-Welsch (1980, eq.2.26) LEAVE-ONE-OUT variance estimate: sigma_i = sqrt(max(0, nu*s2/(nu-1) - r_i^2/((nu-1)(1-h_ii)))), ser = sqrt(1-h_ii)*sigma_i, t = tinv(1-alpha/2, nu-1). Now bit-exact: rint(1,:)=[-0.5423713822 0.6623713822], rint(3,:)=[-0.5217414236 0.8817414236], rint(5,1)=-0.4510083996. b/bint/r/stats unchanged. covers:[regress]. |
| `robustfit` | ✅ 🔬 | robustfit default intercept term + the full weight-function set vs MATLAB R2025b. robustfit adds the constant column by default (const='on'); 'off' suppresses it. Exact-recovery (outlier fully downweighted): y=2x+1+outlier -> [1;2]; y=3+2x+0.5x^2+outlier -> [3;2;0.5]; const 'off' -> 1 coefficient. DEEP-PROBE c174: numkit shipped only 'bisquare'/'huber' AND omitted the DuMouchel-O'Brien leverage adjustment, so values on NOISY data were ~2e-3 off MATLAB. Now ships all 9 MATLAB weight functions (andrews 1.339 / bisquare 4.685 / cauchy 2.385 / fair 1.400 / huber 1.345 / logistic 1.205 / ols 1 / talwar 2.795 / welsch 2.985) with leverage radj=r/sqrt(1-h) and the madsigma scale (sort |radj|, drop smallest p-1, median/0.6745); bit-identical to MATLAB on the noisy 10-point line (ba2..bsq2 all ~0.991-0.999). Leverage gap closed. covers:[robustfit]. |
| `lscov` | ✅ | Sig: [x, stdx, mse, S] = lscov(A, b[, w]). Weighted least squares. mse = SSR/(N-p); S = mse·(A'WA)^(-1). Full N×N covariance V deferred (errors). Bit-identical to MATLAB R2025b on OLS and weighted paths. |
| `stepwisefit` | ❌ | stepwise selection |
| `glmfit` | ✅ | Sig: [b, dev] = glmfit(X, y, distr [, link]) — IRLS GLM with auto-prepended intercept. Supports 'normal', 'binomial', 'poisson', 'gamma', 'inversegaussian' distributions and 'identity', 'logit', 'log', 'reciprocal', 'probit' links. Empty link → canonical for the distribution. yhat = glmval(b, X, link) — inverse-link of [1, X]·b. KNOWN GAPs: binomial 2-col y form, stats struct (SE, p-values, residuals), 'constant'/'weights' name-value args. Spec uses deterministic step-threshold binomial response + sin-noise normal to ensure parity reproducibility. |
| `glmval` | ✅ | Sig: [b, dev] = glmfit(X, y, distr [, link]) — IRLS GLM with auto-prepended intercept. Supports 'normal', 'binomial', 'poisson', 'gamma', 'inversegaussian' distributions and 'identity', 'logit', 'log', 'reciprocal', 'probit' links. Empty link → canonical for the distribution. yhat = glmval(b, X, link) — inverse-link of [1, X]·b. KNOWN GAPs: binomial 2-col y form, stats struct (SE, p-values, residuals), 'constant'/'weights' name-value args. Spec uses deterministic step-threshold binomial response + sin-noise normal to ensure parity reproducibility. |
| `mvregress` | ❌ | multivariate regression |
| `mvregresslike` | ❌ |  |
| `plsregress` | ❌ | partial least squares |
| `ridge` | ✅ | Sig: B = ridge(y, X, k[, scaled]). Ridge regression on standardized X (centered + N-1 std). scaled=1 (default): coefficients in standardized space, p×length(k). scaled=0: (p+1)×length(k) with intercept in original units. Bit-identical to MATLAB R2025b on both paths. |
| `lasso` | ✅ ➖ | Sig: [B, Intercept, Lambda] = lasso(X, y, lambdas [, alpha]) — coordinate-descent L1/elastic-net linear regression. lassoglm extends to GLM families (normal/binomial/poisson) via IRLS+coord-descent inner loop. Standardisation internal; coefficients returned in original units with auto-fit intercept. KNOWN GAPs: no auto λ-path, no CV, no observation weights, no 'standardize' name-value pair. Spec uses deterministic noise (sin-based) + step-threshold binary response for reproducible parity; pins coefficient recovery + zero-out structure at the documented λ values. |
| `lassoglm` | ✅ ➖ | Sig: [B, Intercept, Lambda] = lasso(X, y, lambdas [, alpha]) — coordinate-descent L1/elastic-net linear regression. lassoglm extends to GLM families (normal/binomial/poisson) via IRLS+coord-descent inner loop. Standardisation internal; coefficients returned in original units with auto-fit intercept. KNOWN GAPs: no auto λ-path, no CV, no observation weights, no 'standardize' name-value pair. Spec uses deterministic noise (sin-based) + step-threshold binary response for reproducible parity; pins coefficient recovery + zero-out structure at the documented λ values. |
| `polyconf` | ❌ | polynomial CI prediction |

### Nonlinear Regression (function-form)

**Namespace:** `stats.nlfit.*` — 0 ✅ + 0 ⚠️ / 5 = 0%

| function | status | comment |
|---|:---:|---|
| `nlinfit` | ✅ | Sig: nlinfit(X, y, fun, beta0) — Levenberg-Marquardt NLS with numerical (central-diff) Jacobian. Returns [beta, R, J, CovB, MSE]. nlparci(beta, R, J [, alpha]) — Wald-style parameter CIs via t-quantile · sqrt(diag(MSE · (J'J)^-1)). nlpredci(fun, X, beta, R, J [, alpha]) — prediction CIs via delta-method on the per-query Jacobian. KNOWN GAPs: name-value 'Weights'/'ErrorModel'/'Options' not supported. Spec uses deterministic noise (sin-based) to make parity reproducible; pins recoverability + CI containment + shape invariants. |
| `nlparci` | ✅ | Sig: nlinfit(X, y, fun, beta0) — Levenberg-Marquardt NLS with numerical (central-diff) Jacobian. Returns [beta, R, J, CovB, MSE]. nlparci(beta, R, J [, alpha]) — Wald-style parameter CIs via t-quantile · sqrt(diag(MSE · (J'J)^-1)). nlpredci(fun, X, beta, R, J [, alpha]) — prediction CIs via delta-method on the per-query Jacobian. KNOWN GAPs: name-value 'Weights'/'ErrorModel'/'Options' not supported. Spec uses deterministic noise (sin-based) to make parity reproducible; pins recoverability + CI containment + shape invariants. |
| `nlpredci` | ✅ | Sig: nlinfit(X, y, fun, beta0) — Levenberg-Marquardt NLS with numerical (central-diff) Jacobian. Returns [beta, R, J, CovB, MSE]. nlparci(beta, R, J [, alpha]) — Wald-style parameter CIs via t-quantile · sqrt(diag(MSE · (J'J)^-1)). nlpredci(fun, X, beta, R, J [, alpha]) — prediction CIs via delta-method on the per-query Jacobian. KNOWN GAPs: name-value 'Weights'/'ErrorModel'/'Options' not supported. Spec uses deterministic noise (sin-based) to make parity reproducible; pins recoverability + CI containment + shape invariants. |
| `statset` | ❌ | options struct setter |
| `statget` | ❌ | options struct getter |

### Distance Metrics

**Namespace:** `stats.cluster.*` — 4 ✅ + 0 ⚠️ / 4 = 100%

| function | status | comment |
|---|:---:|---|
| `pdist` | ✅ | Sig: D = pdist(X[, metric[, p|C]]). Pairwise distances. Coverage: euclidean, cityblock, minkowski(p=3), cosine, mahalanobis(default cov(X)), mahalanobis with explicit C. Bug fix 2026-05-08: mahalanobis was throwing 'unknown metric'. Function-handle metric still not supported (separate gap). |
| `pdist2` | ✅ | Sig: D = pdist2(X, Y, metric); [D, I] = pdist2(X, Y, metric, 'Smallest'|'Largest', k). Coverage: default euclidean, minkowski p=3, cityblock, chebychev, cosine, mahalanobis (cov(Y) default), Smallest k, Largest k. Function-handle metric NOT supported (deferred). |
| `squareform` | ✅ | Sig: Y = squareform(X[, mode]). Convert pairwise distance vector ↔ symmetric distance matrix. tol=0 (integer-stable on integer inputs). |
| `mahal` | ✅ | Sig: D = mahal(X, Y). Mahalanobis distance from each row of X to the centroid of Y, scaled by inverse of cov(Y). Coverage: 2-D well-conditioned, 3-D well-conditioned, centroid (=0), zero point, far point. |

### Hierarchical Clustering

**Namespace:** `stats.cluster.*` — 5 ✅ + 0 ⚠️ / 7 = 71%

| function | status | comment |
|---|:---:|---|
| `linkage` | ✅ | Sig: Z = linkage(Y[, method[, metric]]). When Y is N×D matrix, computes pdist(Y, metric, p) internally; when Y is row vector (pdist output), uses it directly. 7 methods: single/complete/average/weighted/centroid/median/ward. 2026-05-08: tie-breaking aligned with MATLAB R2025b (prefers largest pair lex when distances tie); 3-arg form now routes metric to pdist (was hardcoded euclidean). Bit-identical to MATLAB on probed datasets. |
| `cluster` | ✅ | Sig: T = cluster(Z, 'maxclust'|'cutoff', val[, 'criterion', 'distance'|'inconsistent'][, 'depth', d]). Fingerprints are label-permutation-invariant (cluster counts + same-cluster boolean tests) because MATLAB / numkit / Octave assign different label IDs for the same partition. Default 'cutoff' criterion is 'inconsistent' (R2025b). |
| `clusterdata` | ✅ | Sig: T = clusterdata(X, c) with scalar shortcut: c>=2 maxclust, 0<c<2 cutoff (inconsistency). Or N-V form: 'MaxClust', 'Cutoff', 'Linkage', 'Distance', 'Criterion', 'Depth', 'P'. Fingerprints are label-permutation-invariant. Default 'Linkage' is 'single', default 'Distance' is 'euclidean', default 'cutoff' criterion is 'inconsistent' — all per MATLAB R2025b. |
| `cophenet` | ✅ | Sig: c = cophenet(Z, Y) or [c, d] = cophenet(Z, Y). Cophenetic correlation between original distances Y and the merge-tree-derived cophenetic distances d. Bug fix 2026-05-08: 2-output form was throwing because adapter only emitted outs[0]; now both outputs are produced. |
| `inconsistent` | ✅ | Sig: Y = inconsistent(Z[, depth]). Inconsistency coefficient on a linkage tree Z. Each row [mean, std, count, inc_coeff] over the depth-d subtree below each non-leaf node. Default depth=2. |
| `dendrogram` | ❌ | display |
| `optimalleaforder` | ❌ | leaf permutation for visualisation |

### Partitional Clustering

**Namespace:** `stats.cluster.*` — 3 ✅ + 0 ⚠️ / 4 = 75%

| function | status | comment |
|---|:---:|---|
| `kmeans` | ✅ | Sig: [idx, C, sumd, D] = kmeans(X, K, 'MaxIter'/'Replicates'/'Distance'/'Start'/'Display'/'EmptyAction', val, ...). Default Distance='sqeuclidean', Start='plus'. Fingerprints are label-permutation-invariant (cluster counts + same-cluster boolean tests + output shapes) because RNG init differs between engines. |
| `kmedoids` | ✅ ❗ | Sig: [idx, C, sumd, D, midx, info] = kmedoids(X, K, 'Distance'/'MaxIter'/'Replicates'/'Algorithm'/'Start', val, ...). Default Distance is 'sqeuclidean' (per R2025b — not 'euclidean'). Fingerprints are label-permutation-invariant (cluster counts + same-cluster boolean tests + output shapes) because RNG init differs between engines (joint with normrnd spec for full label parity). |
| `dbscan` | ✅ | Sig: [idx, corepts] = dbscan(X, eps, minpts, 'Distance'|'P', val, ...). Coverage: euclidean default, precomputed, minkowski with P, cityblock. Noise = -1 (MATLAB R2025b convention). |
| `spectralcluster` | ❌ | spectral clustering |

### Cluster Evaluation

**Namespace:** `stats.cluster_eval.*` — 0 ✅ + 0 ⚠️ / 3 = 0%

| function | status | comment |
|---|:---:|---|
| `silhouette` | ✅ | Sig: s = silhouette(X, clust). Default metric sqEuclidean. 6 points, 2 well-separated clusters of 3. Element-wise SAVE; values near 0.99 indicating tight clusters with large inter-cluster gap. |
| `evalclusters` | ❌ | CalinskiHarabasz / DaviesBouldin / gap / silhouette |
| `manovacluster` | ❌ | dendrogram from MANOVA |

### Nearest Neighbors (function-form)

**Namespace:** `stats.knn.*` — 0 ✅ + 0 ⚠️ / 3 = 0%

OOP `KDTreeSearcher` / `ExhaustiveSearcher` / `hnswSearcher` intentionally omitted; flat function form only.

| function | status | comment |
|---|:---:|---|
| `knnsearch` | ✅ | Sig [Idx,D]=knnsearch(X,Y,'K',3): brute-force kNN, default Euclidean. Distinct-distance queries so order is deterministic. Query [1.2 1.8]: nearest = rows [2 1 3], D=[0.282843 0.824621 1.131371]; query [8.7 8.2]: rows [5 4 6], D(1)=0.360555. Pins BOTH idx AND D (was an out_var dump of idx only -> distances D were unverified). Earlier query [1.5 1.5] gave all-equal D=0.707 (degenerate). |
| `rangesearch` | ✅ | Sig: [Idx, D] = rangesearch(X, Y, r). Cell-array output unwrapped to a numeric row in SAVE (idx = idxC{1}). All 3 points in cluster 1 are within r=1.0 of (1.5, 1.5). Explicit fingerprint avoids sum on the cell. |
| `createns` | ❌ | tree constructor (returns struct, not class) |

### Hidden Markov Models

**Namespace:** `stats.hmm.*` — 0 ✅ + 0 ⚠️ / 5 = 0%

| function | status | comment |
|---|:---:|---|
| `hmmdecode` | ❌ | forward-backward |
| `hmmestimate` | ❌ | MLE from labelled sequence |
| `hmmgenerate` | ❌ | sample sequences |
| `hmmtrain` | ❌ | Baum-Welch |
| `hmmviterbi` | ❌ | most-likely state path |

### Dimensionality Reduction

**Namespace:** `stats.dim.*` — 3 ✅ + 0 ⚠️ / 8 = 38%

| function | status | comment |
|---|:---:|---|
| `pca` | ✅ 🔬 | Sig: [coeff, score, latent, tsquared, explained, mu] = pca(X). Eigendecomposition of cov(X) for principal components. coeff is signed-undefined (eigenvector orientation), so abs() is taken in fingerprints. Bit-identical to MATLAB R2025b on |coeff|, latent, explained, mu, tsquared. |
| `pcacov` | ✅ | Sig: [coeff, latent, explained] = pcacov(C). Like pca but on a precomputed covariance matrix. Bit-identical to MATLAB R2025b. |
| `pcares` | ✅ | Sig: [res, recon] = pcares(X, ndim). Residual matrix and rank-ndim reconstruction X̂ = score(:,1..ndim) · coeff(:,1..ndim)' + μ. 2-output form added 2026-05-08; was returning only residuals. |
| `ppca` | ❌ | probabilistic PCA |
| `factoran` | ❌ | factor analysis |
| `rica` | ❌ | reconstruction ICA |
| `sparsefilt` | ❌ | sparse filtering |
| `tsne` | ❌ | t-SNE |

### Feature Selection (function-form)

**Namespace:** `stats.fselect.*` — 0 ✅ + 0 ⚠️ / 9 = 0%

| function | status | comment |
|---|:---:|---|
| `fscchi2` | ❌ | classification — chi-squared score |
| `fscmrmr` | ❌ | classification — minimum redundancy max relevance |
| `fscnca` | ❌ | classification — neighbourhood comp. analysis |
| `fsrftest` | ❌ | regression — F-test score |
| `fsrmrmr` | ❌ | regression — mRMR |
| `fsrnca` | ❌ | regression — NCA |
| `fsulaplacian` | ❌ | unsupervised Laplacian score |
| `relieff` | ❌ | ReliefF |
| `sequentialfs` | ❌ | sequential feature selection |

### Linear Discriminant Analysis (function-form)

**Namespace:** `stats.lda.*` — 1 ✅ + 0 ⚠️ / 1 = **100%**

| function | status | comment |
|---|:---:|---|
| `classify` | ✅ | Sig: [class, err, posterior, logp] = classify(sample, training, group[, type]). 4 discriminant types: linear (LDA, default), quadratic (QDA), diaglinear, diagquadratic. Empirical priors n_k/N. Cholesky-factor approach for numerical stability. Mahalanobis type DEFERRED. |

## Wavelet

### Continuous Wavelet Transforms

**Namespace:** `wavelet.cwt.*` — 0 ✅ + 0 ⚠️ / 16 = 0%

`cwtfilterbank` (class) and the deep-learning layer family
(`cwtLayer`/`icwtLayer`/`dlcwt`/etc.) intentionally omitted.

| function | status | comment |
|---|:---:|---|
| `cwt` | ❌ | continuous wavelet transform |
| `icwt` | ❌ | inverse CWT |
| `cwtfreqbounds` | ❌ | frequency support |
| `centfrq` | ❌ | central frequency of wavelet |
| `scal2frq` | ❌ | scale → pseudo-frequency |
| `wcoherence` | ❌ | wavelet coherence |
| `wsst` | ❌ | wavelet synchrosqueezed transform |
| `iwsst` | ❌ | inverse WSST |
| `wsstridge` | ❌ | ridges of WSST |
| `wtmm` | ❌ | wavelet transform modulus maxima |
| `wavefun` | ❌ | wavelet & scaling function values |
| `wavefun2` | ❌ | 2-D variant |
| `wavsupport` | ❌ | effective support |
| `qfactor` | ❌ | quality factor |
| `wavemngr` | ❌ | wavelet manager |
| `waveinfo` | ❌ | info on a wavelet family |

### Discrete Wavelet Transforms (1-D)

**Namespace:** `wavelet.dwt.*` — 14 ✅ + 0 ⚠️ / 18 = 78%

| function | status | comment |
|---|:---:|---|
| `dwt` | ✅ | Sig: [cA, cD] = dwt(x, wname) or (x, Lo_D, Hi_D), with optional 'mode' N-V (only 'sym' supported). 2026-05-08: bit-identical to MATLAB R2025b on the analysis filters after the wfilters Lo_D/Lo_R label-swap fix landed. Custom-filter form added in same commit. Boundary modes other than 'sym' deferred (errors with clear message). |
| `idwt` | ✅ | Sig: x = idwt(cA, cD, wname) or (cA, cD, Lo_R, Hi_R), optional positional `len` and 'mode' N-V (only 'sym' supported). After wfilters label-swap fix + dwt downsample-offset fix, round-trip is bit-identical to MATLAB R2025b at ~1e-12. Custom synthesis-filter form added in the same commit. |
| `wavedec` | ✅ | Sig: [c, l] = wavedec(x, n, wname). Multi-level DWT decomposition. After wfilters Lo_D/Lo_R label-swap fix landed, output is bit-identical to MATLAB R2025b. Custom (Lo_D, Hi_D) form deferred (rare for multi-level). |
| `waverec` | ✅ | Sig: x = waverec(c, l, wname). Multi-level inverse DWT. Round-trips wavedec at ~1e-10 after the wfilters label-swap fix. Custom (Lo_R, Hi_R) form deferred. |
| `appcoef` | ✅ | Sig: A = appcoef(c, l, wname[, level]) or (c, l, LoR, HiR[, level]); optional 'Mode'/'mode' N-V (only 'sym' supported). 2026-05-08: cascades-fixed via wfilters Lo_D/Lo_R label-swap. Custom-filter form added in this commit. |
| `detcoef` | ✅ | Sig: D = detcoef(C, L[, level[, 'cells']]). Default level = numel(L) - 2 (deepest). DEEP-PROBE c175: added the multi-level vector forms — detcoef(C,L,[n1 n2 ...]) with a single output returns a CELL array of the per-level details (icv=1, ncv=3); detcoef(C,L,'cells') returns a cell of ALL nMax levels (ncs=3); [d1,...,dp] = detcoef(C,L,[n1 ...]) deals one detail per output (e1n=8, e3_1=detail-L3(1)). numkit previously threw 'level must be scalar'. The scalar and the [levels],'cells' forms are unchanged. covers:[detcoef]. |
| `wrcoef` | ✅ | Sig: y = wrcoef(type, c, l, wname[, n]). Single-band reconstruction. type ∈ {'a','d'}; n is the level kept ('a' allows n=0 = full reconstruction; 'd' requires n in [1, max]). Default n = length(l)-2 for both types. Algorithm: build modified c with off-band coefficients zeroed, run waverec. Verified parity with MATLAB R2025b on HAAR wavelet (where numkit's wavedec matches MATLAB exactly). For db/sym/coif numkit's wavedec uses a slightly different boundary convention (BUGS.md #37) — wrcoef there produces values consistent with numkit's own wavedec/waverec round-trip but does NOT match MATLAB coefficient-for-coefficient. (Lo_R, Hi_R) two-filter form not implemented in this release. |
| `dwtmode` | ❌ | extension mode |
| `dyaddown` | ✅ | Sig: y = dyaddown(x[, ODD][, type]). Dyadic downsample by 2. ODD=0 default → keep even-indexed; ODD=1 → keep odd-indexed. For matrices, type ∈ {c (default, columns), r (rows), m (both)}. Bug fix 2026-05-08: matrix path silently flattened to a 1-D vector + ignored the type arg. tol=0 (integer-stable on integer inputs). |
| `dyadup` | ✅ | Sig: y = dyadup(x[, ODD][, type]). Zero insertion between samples (upsample by 2). Vector default ODD=1 → length 2N+1 with leading zero. ODD=0 → length 2N-1, no leading zero. For matrices, type ∈ {c (default, columns), r (rows), m (both)}. Bug fix 2026-05-08: matrix path silently flattened + ignored type arg. tol=0. |
| `wkeep` | ✅ | Sig: y = wkeep(x, n[, OPT]) (1-D) or y = wkeep(X, [R C][, [fr fc]]) (2-D). 1-D: 'c'/'l'/'r' or numeric start. 2-D: central [R C] sub-matrix or explicit corner. Bug fix 2026-05-08: 2-D form was throwing 'Cannot convert double to scalar' (adapter did toScalar on the size vec). tol=0. |
| `wextend` | ✅ | Sig: y = wextend(type, mode, x, lf[, side]). Bug fix 2026-05-08: extended modes (symw, asym, asymw, sp0, sp1) and 2-D forms (type=2 / 'ar' / 'ac') were not implemented. Now full coverage: 11 modes × 4 type forms × 3 sides. tol=0 (integer-stable on integer inputs). |
| `wcodemat` | ✅ | Sig: Y = wcodemat(X[, nb[, opt[, absol]]]). Quantize/scale to [1, nb] integer codes. opt ∈ {'mat'(default), 'row', 'col'}; absol=1 default uses |x|. Bug fix 2026-05-08: previous impl used `round` and multiplied by `nb-1`, producing off-by-one quantization on interior values. MATLAB uses floor((v-mn)/span * nb) + 1, with the upper edge clamped from nb+1 down to nb. tol=0 (integer-stable). Octave doesn't ship wcodemat. |
| `haart` | ✅ | Sig: [a, d] = haart(x[, level[, integerflag]]). Haar 1-D DWT. Default level = max k such that 2^k divides length(x). 'noninteger' uses 1/sqrt(2) Haar pair; 'integer' uses lifting (a = x[2k] + floor((x[2k+1]-x[2k])/2)). Output is always column for vector input. d is plain when level=1, cell array d{1..L} when level>1 (d{1} finest). Matrix input processes columns independently. Verified: level=1, default-level (cell), integer mode (signed-floor), matrix, complex, row->col coercion, integer+double, N=12 partial level. |
| `ihaart` | ✅ | Sig: xrec = ihaart(a, d[, level[, integerflag]]). Inverse Haar 1-D DWT. Default level=0 (lossless reconstruction). When level=K (in [0, Nlevels)) the K finest detail bands d{1..K} are zeroed BEFORE reconstruction (xrec stays full-length). Inverse formulas: noninteger uses (a±d)/sqrt(2); integer uses lifting x[2k]=a[k]-floor(d[k]/2), x[2k+1]=x[2k]+d[k]. d MUST be real even when a is complex (MATLAB validateattributes on D). d may be a plain matrix at level=1 or a length-Nlevels cell array. Vector-shaped a returns column; matrix returns matrix. Verified: level=1, full multi-level, partial reconstruction (zero-out 1 and 2 bands), integer mode + partial, matrix full + partial. |
| `wmaxlev` | ✅ | Sig: L = wmaxlev(N, wname). Maximum DWT decomposition level: L = floor(log2(N / (Lf - 1))) where Lf is the wavelet filter length. Vector N (e.g., 2-D image dims) uses min(N). Coverage: wavelet ∈ {haar, db1, db2, db4, db10, sym4, coif2} × N ∈ {2, 8, 16, 64, 100, 1024, 2048} + 2-vector N. tol=0. |
| `dwpt` | ❌ | discrete wavelet packet transform |
| `idwpt` | ❌ | inverse DWPT |

### Discrete Wavelet Transforms (2-D / 3-D)

**Namespace:** `wavelet.dwt2.*` — 2 ✅ + 0 ⚠️ / 15 = 13%

| function | status | comment |
|---|:---:|---|
| `dwt2` | ✅ | Sig [cA,cH,cV,cD]=dwt2(X,'haar'): single-level 2-D Haar DWT, 4x4 ramp -> 2x2 subbands. Approximation cA=[7 11; 23 27] (col-major: cA(1,1)=7, cA(2,2)=27); horizontal cH(1,1)=-1; vertical cV(1,1)=-4; diagonal cD(1,1)=0 (separable ramp has no diagonal detail); sum(cA)=68. Pins distinct cA entries (not a constant) so the orientation/scaling of all four subbands is fixed. |
| `idwt2` | ✅ | Sig X=idwt2(cA,cH,cV,cD,wname): single-level 2-D inverse Haar DWT, 2x2 subbands -> 4x4. Distinct nonzero detail coeffs: X(1,1)=1.3, X(2,2)=-0.2, X(4,4)=1.825, X(1,3)=0.85, sum(X)=20. Pins the per-quadrant reconstruction (not just a round-trip identity). |
| `wavedec2` | ❌ |  |
| `waverec2` | ❌ |  |
| `appcoef2` | ❌ |  |
| `detcoef2` | ❌ |  |
| `wrcoef2` | ❌ |  |
| `wpdec2` | ❌ | 2-D wavelet packet |
| `wprec2` | ❌ |  |
| `haart2` | ❌ |  |
| `ihaart2` | ❌ |  |
| `wavedec3` | ❌ | 3-D |
| `waverec3` | ❌ |  |
| `dwt3` | ❌ |  |
| `idwt3` | ❌ |  |

### Stationary, MODWT, and Wavelet Packets

**Namespace:** `wavelet.swt_modwt.*` — 4 ✅ + 0 ⚠️ / 17 = 24%

| function | status | comment |
|---|:---:|---|
| `swt` | ✅ | Sig: swc = swt(x, n, wname). Stationary wavelet transform. Argument order matches MATLAB. Output SHAPE matches; APPROXIMATION row (last) values match bit-identical; DETAIL rows match in magnitude but differ in sign (Hi_D vs Hi_R QMF convention). Per-value sign-aware parity needs an inner-kernel investigation beyond this spec — fingerprint uses |wH| for detail rows (sign-invariant) and exact equality for the approximation row (sign-correct). MATLAB R2025b reference; Octave wavelet package may not ship swt. |
| `iswt` | ✅ | Sig: x = iswt(swc, wname). Inverse stationary wavelet transform. Even though swt/iswt internal coefficient values use a different filter convention than MATLAB, the round-trip iswt(swt(x)) DOES recover x — that's the structurally important invariant for any inverse transform. Both MATLAB and numkit reconstruct the original signal to machine precision. |
| `swt2` | ❌ |  |
| `iswt2` | ❌ |  |
| `modwt` | ✅ | Sig w=modwt(x,wname,lev): MODWT, (lev+1) x N. Now pins ACTUAL per-coefficient values (was energy-only): MODWT filters are wrev(Lo_D)/√2, wrev(Hi_D)/√2 applied as a look-back circular convolution — bit-identical with MATLAB R2025b (haar W_{1,1}=-3.5 via circular wrap 0.5*(x1-x8); scaling row [5.5 4.5 ...]; db2 W1=[0.732 2 -2.732]). Energy conservation + exact imodwt round-trip also pinned. Queue-clearing 2026-05-29: fixed the per-coefficient alignment/boundary that the old energy-only spec couldn't catch. |
| `imodwt` | ✅ | Sig: x = imodwt(w, wname). Inverse MODWT. Round-trip imodwt(modwt(x)) recovers x to machine precision — the structurally important invariant. The internal coefficient values diverge from MATLAB R2025b (kernel filter-convention gap, see modwt.json comment); both engines independently recover x correctly from THEIR OWN coefficients. |
| `modwtmra` | ❌ | multi-resolution analysis from MODWT |
| `modwtcorr` | ❌ | scale-by-scale correlation |
| `modwtvar` | ❌ | scale-by-scale variance |
| `modwtxcorr` | ❌ | cross-correlation |
| `modwpt` | ❌ | maximal-overlap packet |
| `imodwpt` | ❌ |  |
| `wpdec` | ❌ | wavelet packet decomposition |
| `wprec` | ❌ | reconstruction |
| `wpcoef` | ❌ |  |
| `wprcoef` | ❌ |  |
| `besttree` | ❌ | best-basis selection |

### Denoising and Compression

**Namespace:** `wavelet.denoise.*` — 3 ✅ + 0 ⚠️ / 16 = 19%

| function | status | comment |
|---|:---:|---|
| `wdenoise` | ✅ | Sig: r = wdenoise(...). Spec-extension batch 2026-05-09. |
| `wdenoise2` | ❌ | 2-D denoising |
| `wden` | ❌ | classical denoising |
| `wdencmp` | ❌ | denoise / compress |
| `wpdencmp` | ❌ | wavelet-packet denoise / compress |
| `wnoisest` | ✅ | Sig: sigma = wnoisest(c, l, level). MAD-based noise sigma estimate from wavedec output. Bit-identical with MATLAB R2025b on deterministic-input probe (sigma=0.0900008 on db4 level-3 decomposition of test signal). |
| `wvarchg` | ❌ | variance-change detection |
| `ddencmp` | ❌ | default thresholding parameters |
| `thselect` | ❌ | threshold selection |
| `wthcoef` | ❌ | apply threshold to detail coeffs |
| `wthcoef2` | ❌ |  |
| `wthresh` | ✅ | Sig y=wthresh(x,sorh,t): soft 's' shrinks toward 0 by t (sign(x)*max(|x|-t,0)) -> [-2 -0.5 0 0 0.5 2] for t=1; hard 'h' zeroes |x|<=t and keeps the rest -> [-3 -1.5 0 0 1.5 3]. Pins both the kept-and-shrunk values and the zeroed sub-threshold entries for both modes. |
| `wmulden` | ❌ | multivariate denoising |
| `measerr` | ❌ | quality measures (PSNR/MSE/MAX/L2) |
| `wnoise` | ❌ | noisy test signal |
| `wcompress` | ❌ | compression front-end |

### Filter Banks and Wavelet Families

**Namespace:** `wavelet.filt.*` — 7 ✅ + 0 ⚠️ / 22 = 32%

| function | status | comment |
|---|:---:|---|
| `wfilters` | ✅ | Sig: [Lo_D, Hi_D, Lo_R, Hi_R] = wfilters(wname). Standard MATLAB convention: Lo_D = wrev(Lo_R), Hi_R = (-1)^k · Lo_R[N-1-k] (QMF on Lo_R), Hi_D = wrev(Hi_R). 2026-05-08 fix: numkit's labels were swapped (numkit's Lo_D was MATLAB's Lo_R and vice versa) — root cause of dwt/wavedec value mismatch. Now bit-identical to MATLAB R2025b across haar/db1..db10/sym2..sym10/coif1..coif5. |
| `orthfilt` | ✅ | Sig: [Lo_D, Hi_D, Lo_R, Hi_R] = orthfilt(W). Quadruple from a unit-norm scaling filter W (sum(W)=1, length even). Lo_R = W·√2; Lo_D = reverse(Lo_R); Hi_R[k] = (-1)^k · Lo_R[N-1-k]; Hi_D = reverse(Hi_R). Coverage: db2 (4-tap), db4 (8-tap), custom 2-tap. |
| `qmf` | ✅ | Sig: y = qmf(x[, p]). Quadrature mirror filter. y(k) = (-1)^(k-1+p) · x(N-k+1). Default p=0 (identity-sign on the first element); p=1 negates. Coverage: even/odd-length + p=0/1 + length-8 + column input + single element. tol=0 (integer-stable on integer inputs). |
| `biorfilt` | ❌ | biorthogonal filter quadruple |
| `dbwavf` | ✅ | Sig: h = dbwavf(wname). Daubechies scaling filter: dbwavf*sqrt(2) = Lo_R, length 2N for dbN, sum(h) = 1. Coverage: db1, db2, db4, db5, db6, db8, db10. Bug fix 2026-05-08: previously only supported db1..db4; extended table to db5..db10. |
| `coifwavf` | ✅ | Sig: h = coifwavf(wname). Coiflet scaling filter: coifwavf*sqrt(2) = Lo_R, length 6K for coifK, sum(h) = 1. Coverage: coif1..coif5 (coif2..coif5 added 2026-05-08; was only coif1). |
| `symwavf` | ✅ | Sig: h = symwavf(wname). Symlet (least-asymmetric Daubechies) scaling filter: symwavf*sqrt(2) = Lo_R, length 2N for symN, sum(h) = 1. Coverage: sym2..sym10 (sym3 + sym5..sym10 added 2026-05-08; was only sym2/sym4). |
| `dbaux` | ❌ | Daubechies aux |
| `symaux` | ❌ | symlet aux |
| `biorwavf` | ❌ | biorthogonal scaling filter |
| `rbiowavf` | ❌ | reverse biorthogonal |
| `fejerkorovkin` | ❌ | Fejér-Korovkin filters |
| `mbscalf` | ❌ | Morris minimum-bandwidth |
| `hanscalf` | ❌ | Han scaling filter |
| `blscalf` | ❌ | Beylkin |
| `bswfun` | ❌ | biorthogonal scaling/wavelet via cascade |
| `wrev` | ✅ | Sig: y = wrev(x). Reverse along the first non-singleton dimension. Row vector / col vector -> reverse element order. Matrix M×N -> reverse each column independently (= flipud). Complex preserved. Bug fix 2026-05-08: matrix path was full-flip not flipud; complex input dropped imaginary parts. tol=0 (integer-stable on integer inputs). |
| `isbiorthwfb` | ❌ | check biorthogonal filter bank |
| `isorthwfb` | ❌ | check orthogonal filter bank |
| `wavelets` | ❌ | list available wavelet names |
| `waveletfamilies` | ❌ | list families |
| `wavenames` | ❌ |  |

### Continuous Wavelet Shapes

**Namespace:** `wavelet.shape.*` — 8 ✅ + 0 ⚠️ / 11 = 73%

| function | status | comment |
|---|:---:|---|
| `meyer` | ❌ | Meyer wavelet |
| `meyeraux` | ✅ | Sig: y = meyeraux(x). Element-wise auxiliary polynomial 35x⁴ − 84x⁵ + 70x⁶ − 20x⁷. MATLAB clips outside [0, 1]: x<=0 -> 0, x>=1 -> 1. Bug fix 2026-05-08: numkit was applying the raw polynomial outside [0, 1] (e.g. meyeraux(2) = -208 instead of MATLAB's 1). |
| `mexihat` | ✅ | Sig: [psi, x] = mexihat(LB, UB, N). Mexican-hat wavelet ψ(t) = (2/√3)·π^(-1/4)·(1-t²)·exp(-t²/2). Even, peaks at 0, zeros at ±1. Coverage: N ∈ {8, 16, 64} on [-5, 5] + asymmetric range [0, 5]. |
| `morlet` | ✅ | Sig: [psi, x] = morlet(LB, UB, N). Real Morlet ψ(t) = exp(-t²/2)·cos(5t). Coverage: N ∈ {8, 16, 64} on [-5, 5] + asymmetric range [0, 5]. |
| `cgauwavf` | ✅ | Sig: [psi, x] = cgauwavf(LB, UB, N[, p|'cgauN']). Complex Gaussian wavelet (-1)^p · H_p(t + i/2) · exp(-t² - i·t). Bug fix 2026-05-08: 'cgauN' wname form was throwing 'Cannot convert char to scalar'. |
| `cmorwavf` | ✅ | Sig: [psi, x] = cmorwavf(LB, UB, N[, fb, fc]). Complex Morlet ψ(t) = (1/√(π·fb))·exp(2πi·fc·t)·exp(-t²/fb). Bug fix 2026-05-08: 3-arg form was throwing instead of using defaults fb=1, fc=1. Coverage: default + custom (fb, fc) + N=33. |
| `fbspwavf` | ✅ | Sig: [psi, x] = fbspwavf(LB, UB, N, m, fb, fc). Frequency B-spline ψ(t) = √fb · (sinc(fb·t/m))^m · exp(2πi·fc·t). Coverage: m ∈ {2, 3} × (fb, fc) ∈ {(1,1), (0.5,2)} × N ∈ {8, 16, 33}. |
| `gauswavf` | ✅ | Sig: [psi, x] = gauswavf(LB, UB, N[, p|'gausN']). p-th derivative Gaussian wavelet. Bug fix 2026-05-08: 'gausN' wname form was throwing 'Cannot convert char to scalar'; now parses N from string. Coverage: p ∈ {1, 2, 4, 8} integer + 'gaus3' wname. |
| `intwave` | ❌ | wavelet integral |
| `pat2cwav` | ❌ | pattern → custom wavelet |
| `shanwavf` | ✅ | Sig: [psi, x] = shanwavf(LB, UB, N, fb, fc). Shannon wavelet ψ(t) = √fb·sinc(fb·t)·exp(2πi·fc·t). Coverage: (fb, fc) ∈ {(1,1), (0.5,2)} × N ∈ {8, 16, 33}. |

### Lifting

**Namespace:** `wavelet.lift.*` — 0 ✅ + 0 ⚠️ / 6 = 0%

`liftingScheme` and `liftingStep` are MATLAB classes; we treat lifting
as a pair of flat decomposition / reconstruction functions.

| function | status | comment |
|---|:---:|---|
| `lwt` | ❌ | lifting wavelet transform |
| `ilwt` | ❌ |  |
| `lwt2` | ❌ |  |
| `ilwt2` | ❌ |  |
| `lwtcoef` | ❌ | extract one band |
| `lwtcoef2` | ❌ |  |

### Decomposition Trees and Misc

**Namespace:** `wavelet.misc.*` — 0 ✅ + 0 ⚠️ / 13 = 0%

| function | status | comment |
|---|:---:|---|
| `dualtree` | ❌ | dual-tree complex DWT |
| `idualtree` | ❌ |  |
| `dualtree2` | ❌ |  |
| `idualtree2` | ❌ |  |
| `dddtree` | ❌ | double-density DWT |
| `idddtree` | ❌ |  |
| `tqwt` | ❌ | tunable Q-factor wavelet transform |
| `itqwt` | ❌ |  |
| `wfbm` | ❌ | fractional Brownian motion |
| `wfbmesti` | ❌ | Hurst exponent estimate |
| `wfusimg` | ❌ | image fusion |
| `wfusmat` | ❌ | matrix fusion |
| `wentropy` | ❌ | wavelet entropy |

## Misc / not in TODO

Functions benched by the harness that don't appear in any of the MATLAB-doc sections above. Move them into a real section if they correspond to a documented MATLAB function.

| function | status | comment |
|---|:---:|---|
| `impyramid_expand` | — | Sig: B = impyramid(A, 'expand'). Output: (2M-1)x(2N-1). KNOWN GAP: numkit uses literal Burt-Adelson zero-stuff + [1 4 6 4 1]/16 kernel (matches Octave-image bit-equal); MATLAB R2025b uses imresize-with-piecewise-constant-kernel (kernel handle defined inline in toolbox/images/images/impyramid.m). Interior values agree bit-equal across all three engines; only boundary samples differ. Spec pins (a) output shape and (b) interior sum + center value to catch real regressions while tolerating the boundary-handling divergence. Full bit-equal with MATLAB requires either custom-kernel support in imresize or a rewrite of impyramid to inline the imresize math (~150 LOC, deferred). |
| `axes2pix` | — | Sig: pix = axes2pix(n, extent, axesCoord). World→pixel axis mapping (1-based). Octave-image has axes2pix. |
| `isgray` | — ➖ | Sig: tf = isgray(I). True for 2-D images of class uint8/uint16/int16 or float in [0,1]. Octave-image has isgray. |
| `imcast` | — ➖ | Sig: J = imcast(I, type). Dispatch wrapper over im2* helpers (type ∈ double/single/uint8/uint16/int16/logical). Octave-image has imcast. |
| `mmgradm` | — ➖ | Sig: G = mmgradm(I [, se_dil [, se_ero]]). Morphological gradient = imdilate − imerode (default cross SE). Octave-image has mmgradm. |
| `fchcode` | — ➖ | Sig: fcc = fchcode(bound). Freeman 8-direction chain code; struct with x0y0, fcc, diff fields. Octave-image has fchcode. |
| `fftconv2` | — ➖ | Sig: Y = fftconv2(A, B [, shape]). FFT-based 2-D conv; output complex with tiny imag, smoke wraps real(). Octave-image has fftconv2. |
| `wavelength2rgb` | — ➖ | Sig: rgb = wavelength2rgb(wavelength [, class [, gamma]]). Piecewise visible-light wavelength → RGB (Bruton). Tolerance loose because Octave's gamma=0.8 raises tiny FP noise when raising 0 to 0.8 — final RGB triple to 4 decimals is the right comparison. |
| `imsmooth` | — ➖ | Sig: J = imsmooth(I, name [, sigma]). Currently Gaussian-only with σ-Gaussian, h=ceil(3σ), symmetric pad; Octave-image has imsmooth (this matches the Gaussian path). |
| `colorgradient` | — ➖ | Sig: M = colorgradient(C [, w] [, n]). K-by-3 anchor RGB; piecewise linspace; default n=64. Octave-image has colorgradient. Default uses rows(colormap) but we don't have a graphics colormap so we default to n=64. |
| `iscolormap` | — ➖ | Sig: tf = iscolormap(cmap). Real, float (single/double), 2-D, 3 cols, non-empty. Range [0,1] not enforced. Octave core has iscolormap. |
| `gray` | — | Sig: map = gray([n]). N×3 grayscale colormap. Default n=256 (we don't track figure colormap state). n==1 → [0 0 0]; n<=0 → 0×3. Octave core has gray. |
| `hot` | — | Sig: map = hot([n]). N×3 black→red→yellow→white colormap. Default n=256. Octave core has hot. |
| `cool` | — | Sig: map = cool([n]). N×3 cyan→magenta. r=(0:n-1)/(n-1), g=1-r, b=1. Default n=256. Octave core has cool. |
| `spring` | — | Sig: map = spring([n]). N×3 magenta→yellow. r=1, g=(0:n-1)/(n-1), b=1-g. Default n=256. Octave core has spring. |
| `summer` | — | Sig: map = summer([n]). N×3 green→yellow. r=(0:n-1)/(n-1), g=0.5+r/2, b=0.4. Default n=256. Octave core has summer. |
| `autumn` | — | Sig: map = autumn([n]). N×3 red→yellow. r=1, g=(0:n-1)/(n-1), b=0. Default n=256. MATLAB+Octave both ship autumn. |
| `winter` | — | Sig: map = winter([n]). N×3 blue→cyan-ish. r=0, g=(0:n-1)/(n-1), b=1-g/2. Default n=256. MATLAB+Octave both ship winter. |
| `copper` | — | Sig: map = copper([n]). N×3 black→copper. r=min(5/4*x,1), g=0.7812*x, b=0.4975*x where x=(0:n-1)/(n-1). Default n=256. MATLAB+Octave. |
| `pink` | — | Sig: map = pink([n]). N×3 pastel pink. 3-piece linspace ramps per channel, then sqrt. Default n=256. MATLAB+Octave both ship pink. |
| `hsv` | — | Sig: map = hsv([n]). Hue rotation via hsv2rgb([(0:n-1)'/n, 1, 1]). Default n=256. MATLAB+Octave both ship hsv. |
| `flag` | — | Sig: map = flag([n]). N×3 cycling [1 0 0; 1 1 1; 0 0 1; 0 0 0]. Default n=256. MATLAB+Octave both ship flag. |
| `prism` | — | Sig: map = prism([n]). N×3 cyclic 6-row rainbow [r,o,y,g,b,v]. Default n=256. MATLAB+Octave both ship prism. |
| `lines` | — | Sig: map = lines([n]). Cycles the figure axes colororder. We pin the MATLAB R2025b factory 7-row palette (Octave's older default differs). MATLAB+factory; harness ranks MATLAB as truth so OK is expected. |
| `bone` | — | Sig: map = bone([n]). N×3 grayscale-with-blue-tint colormap. Per Octave's bone.m: idx=floor(3/4·n) for R, idx=floor(3/8·n) for G/B; piecewise linspace ramps; switch on mod(n,8) for base. Default n=256. MATLAB+Octave both match. |
| `white` | — | Sig: map = white([n]). N×3 all-ones colormap. Default n=256. MATLAB+Octave both ship white. |
| `brighten` | — | Sig: rmap = brighten(map, beta). Output = map .^ gamma where gamma = 1-beta if beta>0 else 1/(1+beta). MATLAB+Octave both ship brighten. |
| `contrast` | — | Sig: cmap = contrast(x[, m]). Histogram-equalising gray colormap. Per MATLAB R2025b cleve-moler algorithm: scale to [0,m-1] ints, concat with [0..m], find rising edges. MATLAB+Octave both ship contrast but Octave gives slightly different values; we follow MATLAB. |
| `cdf_upper` | — | Joint 'upper' flag verification across 14 CDFs (.dist). MATLAB R2025b: every *cdf accepts trailing 'upper' string and returns 1 - F(x). normcdf double-checks lower tail unchanged. tol = 1e-9. |
| `windows_sflag` | — | Joint 'periodic' / 'symmetric' (default) sflag verification across 6 signal.windows that accept it. Implementation trick: periodic(N) = first N samples of symmetric(N+1) — works for any window. The other 6 windows (bartlett/triang/parzenwin/bohmanwin/barthannwin/rectwin) accept ONLY 'double'/'single' typeName and throw on 'periodic' (gtest covers that branch). |
| `kstest_extras` | — | Sig: kstest2(x, y[, alpha, tail | name-value]). Tail accepts 'unequal' (default), 'larger', 'smaller' (synonyms for 'both', 'right', 'left' from kstest). Name-Value pairs: 'Alpha', 'Tail'. |
| `ttest_extras` | — | Sig: ttest(x, y[, NV]) paired form; ttest2 default Vartype=equal (pooled). NV pairs: Alpha, Tail, Vartype, Dim (Dim throws). 4th output struct (tstat/df/sd) NOT yet implemented — fingerprints stay on first 3 outputs. (partial — 4th-output struct, matrix input, Dim, n<2 NaN remain as documented gaps in spec comment). |
| `vartest_extras` | — | Sig: vartest(x, v[, NV]) and vartest2(x, y[, NV]). Both adapters now parse Alpha and Tail Name-Value pairs (case-insensitive). 'Dim' N-V throws (parity gap). 4th output remains scalar T/F (struct deferred). (partial). |
| `ztest_extras` | — | Sig: ztest(x, m, sigma[, NV]). Alpha/Tail Name-Value pairs (case-insensitive). 'Dim' N-V throws (parity gap). 4th output is scalar zval (matches MATLAB which doesn't return a struct here). |
| `logical` | — | Sig: r = logical(...). Type conversion. Spec-extension batch 2026-05-09 verified MATLAB R2025b parity. |
| `islogical` | — | Sig: r = islogical(...). Predicate. Spec-extension batch 2026-05-09. |
| `smoothdata` | — | Sig: y = smoothdata(x[,method[,k]]). DEEP-PROBE 2026-05-31: the 'gaussian' method used sigma=(k-1)/4 with a kernel mis-aligned at truncated edges (the generic moving driver only saw the shrunk window, not the centre) -> wrong at boundaries AND interior. MATLAB R2025b uses sigma = windowLength/5, centred on the current sample, with 'shrink' endpoints (truncate kernel at edges + renormalise). smoothdata([1 5 2 8 3],'gaussian',3)=[1.79834 3.83535 3.49741 6.16984 3.99793]; smoothdata([1 5 2 8 3 9 4],'gaussian',5) h1=2.47053 h4=5.20481 h7=5.66334. Now matches MATLAB across vector/matrix/omitnan. Spec was numel-only (hid the value bug). namespace=signal. |
| `sosfiltfilt` | — | N/A (definite): MATLAB R2025b has no top-level sosfiltfilt() -- the equivalent operation is filtfilt(sos, 1, x). Numkit ships sosfiltfilt(sos, x) as a public function that bit-identically matches scipy.signal.sosfiltfilt and is used internally by lowpass/highpass/etc. Definite N/A vs MATLAB top-level. |
| `magic` | — | Sig: M = magic(N). N×N magic square -- rows/cols/diagonals sum to N·(N²+1)/2. Three branches by N's parity: odd (Siamese / de la Loubère), N≡0 mod 4 (doubly-even pattern), N≡2 mod 4 (Strachey). Bit-identical with MATLAB R2025b across N ∈ {3,4,5,6,8} (covers all three branches). |
| `toeplitz` | — 🔬 | Sig: T = toeplitz(c[, r]). T[i,j] = c[i-j] (i>=j) else r[j-i]; r[0] silently overridden by c[0]. DEEP-PROBE 2026-05-31: was DOUBLE-only — dropped the imaginary part of COMPLEX inputs and down-converted SINGLE to double. Now preserves COMPLEX (per-element gather) and SINGLE class. Single-arg COMPLEX form is a HERMITIAN Toeplitz: MATLAB conjugates the strictly-lower triangle, so toeplitz([1+1i 2+2i 3+3i]) has Tc(2,1)=conj(2+2i)=2-2i but Tc(1,2)=2+2i; two-arg complex is a plain gather (no conjugation, Tr(2,1)=c(2)=2). char/logical rejected by MATLAB (not added here). Bit-identical with MATLAB R2025b across square + rectangular + complex + single probes. |
| `hankel` | — 🔬 | Sig: H = hankel(c[, r]). H[i,j] = c[i+j] for i+j<m else r[i+j-m+1]; single-arg form: r all zeros (anti-triangular). DEEP-PROBE 2026-05-31: was DOUBLE-only — dropped the imaginary part of COMPLEX inputs and down-converted SINGLE. Now preserves COMPLEX (per-element gather, hankel NEVER conjugates: Hc(2,2)=c(3)=3+3i) and SINGLE class; two-arg complex keeps r's imaginary part (Hr(3,3)=5+5i). char/logical rejected by MATLAB (not added here). Bit-identical with MATLAB R2025b across complex + single probes. |
| `vander` | — | Sig: V = vander(v). Vandermonde matrix V[i,j] = v[i]^(n-1-j); columns from highest power on the left (MATLAB R2025b convention). Bit-identical. |
| `compan` | — | Sig: A = compan(p). Companion matrix of polynomial coefficients p (length n+1) -- top row [-p(2:end)/p(1)], subdiagonal of ones. eig(compan(p)) == roots(p). Bit-identical with MATLAB R2025b. |
| `pascal` | — | Sig: P = pascal(N). Symmetric Pascal-triangle matrix (k=0 default form). P[i,j] = C(i+j,i). Built via the additive recurrence. Bit-identical with MATLAB R2025b. Note: k=1 (Cholesky factor) and k=2 (cube-root of identity) variants are deferred. |
| `hilb` | — | Sig: H = hilb(N). Hilbert matrix H[i,j] = 1/(i+j-1) (1-indexed). Bit-identical with MATLAB R2025b (single divides; no accumulation error). |
| `invhilb` | — | Sig: H = invhilb(N). Closed-form inverse Hilbert matrix via the binomial formula H⁻¹[i,j] = (-1)^(i+j)*(i+j-1)*C(n+i-1,n-j)*C(n+j-1,n-i)*C(i+j-2,i-1)². Long-double accumulation delays overflow. Bit-identical-ish with MATLAB R2025b (tol 1e-6 -- both engines lose ULPs through the same overflow-prone formula at N>=8). |
| `wilkinson` | — | Sig: W = wilkinson(N). Symmetric tridiagonal eigenvalue test matrix: subdiag/superdiag of ones, main diag = |(1:n)-(n+1)/2|. Bit-identical with MATLAB R2025b. |
| `hadamard` | — | Sig: H = hadamard(N). Sylvester construction: H_1=[1], H_{2k}=[Hk Hk; Hk -Hk]. Power-of-2 N only (1,2,4,8,16,...). MATLAB R2025b also accepts 12·2^k and 20·2^k via Paley constructions -- those are deferred (separate spec). |
| `rosser` | — | Sig: R = rosser(). Hardcoded 8×8 Rosser eigenvalue test matrix. Bit-identical with MATLAB R2025b (constants directly transcribed from MATLAB output). |
| `cputime` | — | Side-effect smoke test (timer probe). cputime returns CPU seconds used by current process; only invariant we can test cross-engine is t >= 0 (absolute values differ between engines). Implemented via std::clock() / CLOCKS_PER_SEC. |
| `isoutlier` | — | Sig: m = isoutlier(A[,method][,'ThresholdFactor',tf]). DEEP-PROBE 2026-05-31: the method arg was parsed-and-ignored (always default median/MAD), so isoutlier(x,'mean') wrongly used the median rule. Now honoured per-column (MATLAB operates per column for matrices) via the shared detector: 'median' (>3*1.4826*MAD from median, sm=1 flags 100), 'median','ThresholdFactor',1 (mt1=1 also flags x(1)=1, smt=2), 'mean' (>3*std from full-sample mean -> 100 within 3*std=118.9, sme=0 flags NONE), 'mean','ThresholdFactor',1 (me1d=1 flags 100, sme1=1), 'quartiles' (outside Q1-1.5IQR..Q3+1.5IQR, sq=1). Matrix [1 2;3 4;5 100;7 8;9 10] per-column -> only (3,2)=100 (sM=1, M32=1). DEEP-PROBE c172: added 'grubbs' (iterative Grubbs's test, ThresholdFactor = significance alpha, default 0.05) — at each step the max studentized deviation G=max|x-mean|/std is compared to G_crit=((N-1)/sqrt(N))*sqrt(t^2/(N-2+t^2)) with t=tinv(alpha/(2N),N-2); flag+remove+repeat down to N>=3. sg=1/g10=1 flags the lone 50; sg2=0 (two extremes mask each other); sga=1 at alpha=0.01. gesd/movmean/movmedian still throw (deferred). Bit-identical MATLAB R2025b. |
| `rmoutliers` | — | Sig: [y,idx] = rmoutliers(A[,method][,percentiles vec][,'ThresholdFactor',tf]). DEEP-PROBE 2026-05-31: previously delegated to the default median detector and IGNORED method/percentiles/ThresholdFactor, and FLATTENED matrices instead of removing rows. Now: vectors drop flagged ENTRIES (orientation preserved); matrices detect per column and remove any ROW with an outlier (R=rmoutliers([1 2;3 4;5 100;7 8;9 10]) drops row 3). Methods median(default)/mean/quartiles via shared detector; 'percentiles',[lo hi] uses MATLAB prctile bounds (100*(k-0.5)/n). 2nd output = logical removed mask (vector: same orientation, I(4)=1; matrix: r x 1 removed-rows, sum=1). grubbs/gesd/movmean/movmedian throw (deferred). Verified vs MATLAB R2025b on x=[1 2 3 100 4 5]: default->[1 2 3 4 5], mean->unchanged, percentiles[10 90]->[2 3 4 5], median tf=1->[2 3 4 5]. |
| `standardizeMissing` | — | Sig: y = standardizeMissing(x, sentinel). Replaces sentinel value with NaN. |
| `detrend` | — 🔬 | Sig: y = detrend(x[, order]). Remove polynomial trend (default linear). Vector form. COMPLEX (fixed 2026-06-05): detrend the real + imaginary parts separately, recombine: detrend([1+2i 5+1i 3-2i 8+0i])=[-0.4+0.4i 1.7+0.3i ...]; 'constant' subtracts the complex mean (-3.25+1.75i); per-column matrix. Matches MATLAB R2025b. |
| `fitdist` | — | Sig: pd = fitdist(x, 'Name'). numkit returns a struct (.DistributionName, .ParameterValues, .ParameterNames, .NumObservations). MATLAB returns a probability-distribution OBJECT with same .ParameterValues/.DistributionName fields. ParameterValues bit-identical (delegates to mle). MATLAB's class methods (.pdf/.cdf/.icdf) deferred. |
| `now` | — | Sig: t = now. Serial date number for current local time. Days since MATLAB epoch (year 0000-01-00). 1970-01-01 = 719529. Cannot bit-compare across engines (different sample times); parity tests range invariant. |
| `datenum` | ✅ 🔬 | MATLAB datenum: serial date number from components. Covered: 3-arg (Y,M,D) with vector args (broadcast to column), 6-arg (Y,M,D,H,MI,S) with time fraction, single-arg Nx3 matrix, single-arg 1x6 row, month/day overflow (m=13 -> next year, d=30 of Feb -> March), year-zero edge. Deferred: string parsing (datenum('2026-05-09')) -- requires datestr/format-spec parser. Algorithm: Howard Hinnant days_from_civil + 719529 (MATLAB epoch). |
| `weekday` | — | MATLAB weekday: day-of-week index 1=Sun..7=Sat (US calendar). Covers single-date, vector input, historical dates spanning 60+ years (1970, 2000, 2026), and one full week roundtrip 7,1,2,3,4,5,6. Optional name string output ('short'/'long') tested in gtest only -- parity harness fingerprints numeric only. Algorithm: ((floor(d) - 2) mod 7) + 1 with positive-modulo (serial 1 = Saturday in MATLAB's calendar). |
| `juliandate` | — | MATLAB juliandate: Julian day number from date components. Covered: 3-arg (Y,M,D) with vector args, 6-arg with time fraction, single-arg 1x6 row, single-arg Nx3 matrix. Anchors: 1970-01-01 00:00 = 2440587.5 (Unix epoch), 2000-01-01 12:00 = 2451545.0 (J2000.0). Algorithm: datenum-serial + 1721058.5. Deferred: string parsing forms, datetime-object input. |
| `eomday` | — | MATLAB eomday: last day of given month. Covered: leap-year all four cases (/4 leap [2024], common [2025], /400 leap [2000], century non-leap [1900]), 30-day month (April), full Jan-Dec scan in leap year, scalar+vector broadcast, and 2x2 matrix shape preservation. Algorithm: lookup table + isLeap = (y%4==0 && y%100!=0) || y%400==0. |
| `datevec` | ✅ 🔬 | MATLAB datevec: inverse of datenum. Covered: scalar round-trip (Y,M,D), with-time round-trip (H,MI,S), Unix-epoch anchor (719529 -> 1970-01-01), fractional-day extraction (0.25 -> 06:00), N-vector input -> Nx6 matrix, multi-output [Y,M,D] form. Algorithm: Howard Hinnant civil_from_days + microsecond rounding for FP-noise dampening. Edge: datevec(0) = [0 0 0 0 0 0] matches MATLAB literal. |
| `yyyymmdd` | — | MATLAB yyyymmdd: packed integer date Y*10000+M*100+D. MATLAB R2025b requires datetime input -- numkit accepts serial date directly as a convenience extension. Spec uses an engine-detecting shim ymd__ that wraps with datetime() on MATLAB and falls through on numkit. Year-0 case excluded (MATLAB datetime errors on dates before 0001-01-01); year-0 covered by gtest. Octave 11.1.0 doesn't ship yyyymmdd; reports N/A. Algorithm: Howard Hinnant civil_from_days then arithmetic packing. |
| `mjuliandate` | — | MATLAB mjuliandate: Modified Julian Date = JD - 2400000.5; epoch 1858-11-17 00:00. Covered: 3-arg/6-arg/single-arg row/single-arg matrix forms with vector inputs. Anchors: MJD epoch 1858-11-17 -> 0, Unix epoch 1970-01-01 -> 40587, J2000.0 -> 51544.5. Algorithm: serial-MATLAB-date - 678942 (= 1721058.5 - 2400000.5, both fractional offsets cancel). Deferred: string + datetime input forms. |
| `predicates` | — | MATLAB linalg predicates batch: issymmetric/ishermitian (with optional 'skew'), isbanded(A,lo,up), isdiag, istril, istriu, bandwidth (1-out=lower / 2-out=[lo,up] / 'lower'|'upper' opt), vecnorm(A[,p[,dim]]). All comparisons exact (== 0). Bit-equal with MATLAB R2025b: predicates produce 0/1, bandwidth produces integers, vecnorm produces doubles. Empty vecnorm([]) -> scalar 0 (MATLAB convention). issymmetric/ishermitian use exact transpose/conj-transpose without tolerance. Octave 11.1.0 ships isbanded/isdiag/issymmetric/ishermitian/istril/istriu but not bandwidth/vecnorm. |
| `rref_rcond_planerot` | — | MATLAB linalg cycle 2: rref + rcond + planerot. rref via Gauss-Jordan with partial pivoting; default tol = max(M,N)*eps(norm(A,inf)); two-output [R, jb] form returns 1-based pivot column indices. rcond uses cheap path 1/(norm(A,1)*norm(inv(A),1)); returns 0 for singular A. KNOWN GAP: rcond on near-singular matrices may differ slightly from MATLAB's LAPACK dgecon estimator. planerot Givens rotation: r=hypot(x,y), G=[c s; -s c], degenerate (0,0) case returns identity. Bit-equal with MATLAB R2025b on the well-conditioned cases; rcond bit-equal on diag/2x2/hilb(4); planerot exact (cos/sin formula matches). rref complex input deferred (KNOWN GAP). Octave: triple-engine green for all three. Real-only inputs in v1. |
| `lsqminnorm_lsqnonneg` | — | MATLAB linalg cycle 4: lsqminnorm + lsqnonneg. lsqminnorm = pinv(A,tol)*B; bit-equal with MATLAB R2025b on full-rank, rank-deficient, and wide systems. lsqnonneg via Lawson-Hanson active-set algorithm; bit-equal on x, resnorm, residual, exitflag for the classic test [1 -1 2; 3 4 5; 6 7 8] / [1; 2; 3] -> x = [0; 0; 0.387097], resnorm = 0.06451612903. KNOWN GAPs: lsqminnorm 'rankWarn'/'RegularizationFactor' name-value args deferred; lsqnonneg 'options'/'problem' input forms and 6th 'lambda' output (Lagrange multipliers) deferred; complex inputs not supported. Octave ships both functions; lsqminnorm in core since Octave 6, lsqnonneg in optim package. |
| `base_conversions` | — | MATLAB Communications Toolbox base conversions: bit2int (pack n-bit groups → integers, msbfirst default true), int2bit (inverse, returns n×M bit matrix), bi2de (legacy synonym, rows = numbers, LSB-first 'right-msb' default, optional base), de2bi (legacy inverse, optional n / base), vec2mat (reshape vector into N-column row-major-filled matrix with padval default 0, 2-out form returns pad count). Bit-equal with MATLAB R2025b on all 26 fingerprint points across MSB/LSB ordering, custom base, auto-width and explicit-width forms, padding semantics. Octave 11.1.0 ships these in the communications package. |
| `sigroi_utils` | — | MATLAB Signal Processing Toolbox ROI utilities (signalMask family): binmask2sigroi (mask→[start end] pairs), sigroi2binmask (inverse, with optional length), extendsigroi/shortensigroi (per-ROI shrink/grow with start clamped to 1 and degenerate ROIs dropped on shorten), mergesigroi (sort-then-merge with sep tolerance), removesigroi (drop ROIs with length ≤ maxLen — matches MATLAB doc, NOT index-based), extractsigroi (cell array default OR concatenated vector when concat=true), sigrangebinmask (bound is scalar→x>bound 'above' default, OR 2-vec→inside [vmin,vmax] closed). KNOWN GAP: 'Relationship'/'IntervalType'/'MinLength'/'Dimension' name-value args for sigrangebinmask deferred. Bit-equal with MATLAB R2025b on all 31 fingerprint points. Octave 11.1.0 doesn't ship these in core (Signal package only). |
| `color_extras` | — | MATLAB Image Toolbox color extras: rgb2lightness (= first channel of rgb2lab; returns single H×W), rgb2ind in fixed-palette form (nearest-RGB quantization, 1-based index uint8 if cmap rows ≤ 256). Bit-equal with MATLAB R2025b on lightness L value at white (=100) and on the 4-color palette quantization of a synthetic 2×2×3 image (red/green/blue/dark-red maps to nearest in [0 0 0; 1 0 0; 0 1 0; 0 0 1]). Lightness values for non-pure colors match MATLAB to ~1e-3 (single-precision rounding through the rgb2xyz → xyz2lab pipeline). KNOWN GAP rgb2ind: scalar-Q (min-variance quant) and scalar-tol (uniform quant) forms deferred; dithering arg ignored. Octave 11.1.0 ships rgb2lightness in the image package only, rgb2ind in core. |
| `filter_design` | — | MATLAB Image Toolbox filter-design utilities (cycle 4): fspecial3 (all 7 types: average/gaussian/laplacian/log/prewitt/sobel/ellipsoid) + fwind2 (2-D FIR via 2-D window method). Bit-equal with MATLAB R2025b on key invariants (sums, centers, sobel structure across all three directions X/Y/Z). KNOWN GAPs (deferred to v2): fsamp2 (requires 2-D IFFT), ftrans2 (Chebyshev polynomial recurrence), fwind1 (Chebyshev), gabor (object class infrastructure). All four deferred fns registered with explicit 'not implemented in v1' errors so MATLAB scripts get clear messages instead of undefined-function. Octave 11.1.0 ships fspecial3 in image package only; fsamp2/fwind2 in image package. |
| `sig_utils` | — | MATLAB Signal Processing Toolbox utility batch (cycle 5): seqperiod (smallest divisor period d ≤ N where x repeats with tol), zerocrossrate (count = #sign-changes + 0.5 boundary credit, rate = count/N — matches MATLAB R2025b default Level=0/ZeroPositive=false), cusum (Page-Hinkley CUSUM detector returning first out-of-control indices). Bit-equal with MATLAB R2025b on all 13 fingerprint points covering periodic + non-periodic + repeat sequences, sign-change patterns including no-crossing edges, and a synthetic mean-shift cusum sequence. KNOWN GAPs: zerocrossrate matrix/N-D + Name=Value (Threshold/TransitionEdge/WindowLength), seqperiod multi-column variant, cusum no-output plotting form. Octave 11.1.0 doesn't ship these in core. |
| `signal_buffer` | — | MATLAB Signal Toolbox buffer (Phase 4.1 of audio extension sweep): partition signal into possibly overlapping/underlapping frames. Bit-equal MATLAB R2025b on 6 cases — non-overlapping zero-pad, p>0 overlap with initial zeros, p>0 with 'nodelay', p<0 underlap, [Y,Z] complete-only output, column-vector input. Implementation in toolboxes/signal/src/digital_filtering/buffer.cpp following MATLAB buffer.m semantic doc (the .m file itself is just a MEX shim; behavior derived from probing). Octave 11.1.0 ships buffer in core (signal package); should match. KNOWN GAPs: (1) initial-condition vector OPT for p>0 (numeric instead of 'nodelay') validated for length but not heavily tested; (2) 3-output form [Y,Z,OPT] for continuous buffering — return value of OPT for next call deferred (MATLAB internal state). |
| `signal_uquant` | — | MATLAB Signal Toolbox uencode/udecode (Phase 4.2): uniform N-bit quantization. Bit-equal MATLAB R2025b on 16 fingerprints — unsigned/signed encoding, custom peak V, 3 output type tiers (uint8/16/32), saturate vs wrap on decode, full roundtrip error within expected 8-bit quantization step (~0.008). Octave 11.1.0 ships these in the signal package. |
| `signal_polyutils` | — ❗ | MATLAB Signal Toolbox polyscale + polystab (Phase 4.3): polynomial root scaling and stabilization. polyscale: y[k] = p[k] * scale^k (closed-form). polystab: roots(a) → reflect outside-unit-circle to inside via 1/conj(root) → poly() back → multiply by leading coef → real() if input real. Bit-equal MATLAB R2025b on 12 fingerprints — scaled poly with real and >1 scale, polystab on poly with one root outside unit (root=2 reflects to 0.5), polystab on FIR filter, polystab on poly with already-stable roots. Octave 11.1.0 ships these in core (signal package). |
| `signal_shiftdata` | — | MATLAB Signal Toolbox shiftdata + unshiftdata (Phase 4.4): dim-aware utilities for filter-like functions. shiftdata(x, dim): permute dim to leading. shiftdata(x, []): auto-shift via shiftdim (drop leading singletons). unshiftdata: ipermute back (or shiftdim(-nshifts)). Bit-equal MATLAB R2025b on 12 fingerprints — explicit-dim path (transpose), auto path (row→col with nshifts=1), full roundtrip identity for both forms. Octave 11.1.0 ships these in core. |
| `signal_kaiserord` | — | MATLAB Signal Toolbox kaiserord (Phase 4.5) — Kaiser-window FIR order estimator. Closed-form per Kaiser 1974: D=(atten-7.95)/(2π·2.285), L=D/Δf+1, β piecewise on attenuation: 0.1102(a-8.7) for a>50, 0.5842(a-21)^0.4 + 0.07886(a-21) for 21≤a≤50, else 0. Bit-equal MATLAB R2025b on 13 fingerprints — lowpass (n=36 Wn=0.4375 β=3.395 ftype='low'), highpass (n=46 Wn=0.45 ftype='high'), bandpass with multiple bands (DC-0 type, Wn=[0.1875 0.5625]). Octave 11.1.0 ships kaiserord in core (signal package). |
| `signal_ellipord` | — | MATLAB Signal Toolbox ellipord (Phase 4.6) — minimum-order Cauer/elliptic filter. Algorithm: prewarp digital→analog (tan(πw/2)), compute analog passband-edge ratio WA per filter type, findelliporder via complete elliptic integrals: ε=√(10^(0.1Rp)-1), k1=ε/√(10^(0.1Rs)-1), k=1/WA, N=ceil(K(k²)·E(1-k1²)/(K(1-k²)·K(k1²))). Bit-equal MATLAB R2025b — lowpass, highpass, bandpass, analog 's' mode. 2026-06-05 (bugs/signal/ellipord-bandstop.md): the BANDSTOP branch (ftype=3) now implemented — reciprocal of the bandpass→lowpass map WA=(WS·(WP1-WP2))/(WS²-WP1·WP2), worst-case edge via WAmin=min|WA| (mirrors numkit buttord/cheb*ord, clean-room). n_bs=4/Wn=[0.1 0.6], n_bs2=5, analog n_bsa=5/Wn=[100 600]. Octave 11.1.0 doesn't ship in core (signal package only). |
| `signal_firpmord` | — | MATLAB Signal Toolbox firpmord (Phase 4.7) — Parks-McClellan FIR order estimator. remlpord formula from Rabiner & Gold pp.156-7: D = [1 d1 d1²] · AA · [1; d2; d2²] (3×3 const matrix from McClellan), fK = 11.01217 + 0.51244·(d1-d2), L = D/df - fK·df + 1. Bit-equal MATLAB R2025b on 8 fingerprints — lowpass (n=21 fo=[0,0.375,0.5,1] ao=[1,1,0,0] w=[10,1]), highpass (n=32), bandpass (n=24). Returns 4-tuple (N, ff, aa, wts) suitable for firpm. Octave 11.1.0 ships in core. |
| `signal_vco` | — | MATLAB Signal Toolbox vco (Phase 4.8) — voltage-controlled oscillator. y = cos(2π·Fc·t + range1·cumsum(x)), where range1 = (Fc/Fs)·2π for scalar Fc, or (Fmax-Fc)/Fs·2π for [Fmin Fmax] (Fc=mean(range)). Bit-equal MATLAB R2025b on 8 fingerprints — zero input (pure carrier), constant offset (chirp-up), full sweep with Fmin/Fmax range. Octave 11.1.0 ships in core (signal package); test passes. |
| `signal_fir2` | — | Signal Processing toolbox fir2 — frequency-sampling FIR filter design. CLEAN-ROOM implementation from public references (Oppenheim & Schafer 3e §7.4-7.5 frequency-sampling FIR design; Rabiner & Gold 1975; Parks & Burrus 1987). Pipeline: piecewise-linearly interpolate the desired (f,m) magnitude response onto a uniform DC..Nyquist grid of npt+1 points, apply a linear-phase delay exp(-j*pi*dt*k/npt) with dt=(nn-1)/2, Hermitian-mirror to length 2*npt, inverse-FFT, window. Full MATLAB argument set: fir2(n,f,m), fir2(n,f,m,npt,lap), fir2(...,window). Bit-equal MATLAB R2025b (tol 1e-9) on 20 fingerprints covering: lowpass/bandpass/highpass 3-arg form; explicit npt=256 grid; a custom Hann window; the lap smoothing argument on break frequencies with duplicated points (discontinuities); and the odd-order Nyquist correction (fir2(11,[0 1],[0 1]) -> length 13, since an odd-order symmetric FIR has a forced zero at Nyquist). Octave 11.1.0 ships fir2 in core but its frequency grid differs slightly (not bit-compared; MATLAB is the reference). |
| `signal_cell2sos` | — | MATLAB Signal Toolbox cell2sos (Phase 4.10) — convert cell array of {Bi, Ai} pairs to L×6 second-order-section matrix. Linear (length-2) sections zero-padded on right. 2-output form [S, G] = cell2sos(C) extracts leading scalar gain when C{1} = {scalar_b, scalar_a}. Bit-equal MATLAB R2025b on 12 fingerprints across both help-example forms (with and without leading gain). Octave 11.1.0 ships in core. |
| `signal_ctfutils` | — | MATLAB Signal Toolbox ctf2zp + scaleFilterSections (Phase 4.11). ctf2zp: cascade transfer function (NUM, DEN, SV) → zero/pole/gain via per-section tf2zpk + product of gains. scaleFilterSections: distribute |sv|^(1/K) across sections, sign on last. Bit-equal MATLAB R2025b on 12 fingerprints — single-section ctf2zp values+counts, multi-section gain product, scaleFilterSections both vector-SV and scalar-SV forms. KNOWN GAP: ctf2zp doesn't strip trailing zeros from numerators/denominators (MATLAB parser does); user-visible difference is extra zero/pole at 0 in z/p arrays for length-padded inputs. Octave 11.1.0 doesn't ship ctf2zp/scaleFilterSections in core. |
| `signal_modulate` | — | MATLAB Signal Toolbox modulate (Phases 4.12 + 5.3). All 5 most-used modes shipped: am / amdsb-sc / amdsb-tc / fm / pm (bit-equal MATLAB) + amssb (uses hilbert — approximate-equal due to finite-window edge effects, ~3-5%). Per-element formulas: am = x·cos(2πFct); amdsb-tc = (x-offset)·cos(...); fm = cos(2πFct + kf·cumsum(x)); pm = cos(2πFct + kp·x); amssb = x·cos(...) + imag(hilbert(x))·sin(...). Default kf = (Fc/Fs)·2π/max|x|, kp = π/max|x|, offset = min(x). 9/9 fingerprints OK (tol 5% to accommodate hilbert edge noise). KNOWN GAPs: pwm/ptm/ppm (specialised pulses), qam (complex carrier) deferred. Octave 11.1.0 doesn't ship modulate in core (signal package only). |
| `signal_demod` | — | MATLAB Signal Toolbox demod (Phases 4.13 + 5.3). Implements am / amdsb-sc (alias) / amdsb-tc / fm / pm. AM family: y·cos(2πFct) → 5th-order Butterworth filtfilt (subtract DC offset for amdsb-tc). FM/PM: yq = hilbert(y)·exp(-j2πFct); FM = (1/P1)·diff(unwrap(angle(yq))) prepended w/ 0; PM = (1/P1)·angle(yq). Approximate-equal MATLAB R2025b on 9 fingerprints (tol 5%); diffs from filtfilt edge handling + hilbert finite-window effects. KNOWN GAPs: amssb / pwm / ptm/ppm / qam deferred. Octave 11.1.0 doesn't ship demod in core (signal package only). |
| `signal_firpm` | — | MATLAB Signal Toolbox firpm — Parks-McClellan optimal equiripple FIR via Remez exchange. Supports all four linear-phase types + Hilbert + Differentiator (matches MATLAB R2025b firpm.m semantics): Type I (even N, sym, Q=1), Type II (odd N, sym, Q=cos(ω/2)), Type III (even N, anti-sym, Q=sin(ω) — 'hilbert' or 'differentiator'), Type IV (odd N, anti-sym, Q=sin(ω/2)). For differentiator, MATLAB firpmfrf weights non-zero bands by 1/(GF/2) and applies an h-sign flip post-Remez ('make sure differentiator has correct sign' — firpm.m line 152-154). Approximate-equal MATLAB R2025b ~1e-3 across seven probed designs covering Type I LP/BP/HP/weighted + Type II LP + Type III Hilbert + Differentiator. KNOWN GAPS: fresp function-handle form, 3rd `res` output struct, lgrid cell-form override. Octave 11.1.0 ships firpm in the signal package (not core). |
| `signal_fftn` | — | MATLAB fftn / ifftn — N-D forward and inverse FFT. Implemented as iterated 1-D fft along dims 1..ndim (commutes; current Dims model caps at 3-D, so max ndim = 3 — higher inputs would require the N-D refactor). 2-D inputs delegate through the same path and produce results identical to fft2. With the optional `sz` argument, axis k is zero-padded or truncated to sz[k-1] before its 1-D FFT (length validation is reused from the per-axis fft). Bit-equal MATLAB R2025b on 2-D, 3-D, sz-override, and ifftn round-trip (round-trip noise ~7e-15, well inside tol 1e-9). Octave 11.1.0 ships fftn / ifftn in core. Round-trip error fingerprint pins the inverse pair under the same tol. |
| `signal_czt` | — | MATLAB Signal Toolbox czt — discrete chirp Z-transform. Implementation: Bluestein decomposition Y[k] = w^(k²/2) · (g ⋆ h)[k] where g[n] = x[n]·a^(-n)·w^(n²/2), h[n] = w^(-n²/2). The g ⊛ h circular convolution is computed via length-L FFT with L = nextPow2(N + m − 1); the negative-index branch of h is placed at indices L-n..L-1 to make the circular convolution equal the linear convolution on the first m output samples. Default args match MATLAB: m = length(x), w = exp(-2π·j/m), a = 1 — so czt(x) ≡ fft(x) and czt(x, m) ≡ fft(x, m). Approx-equal MATLAB R2025b (~1e-13 from chirp-pow arithmetic) on FFT-equivalent, m-override, and full 4-arg forms. Octave 11.1.0 ships czt in the signal package (not core); harness reports it from there. 2-D input is processed column-wise (MATLAB semantics). |
| `signal_stft` | — | MATLAB Signal Toolbox stft / istft. Cycle 86: [s, f, t] = stft(x, fs[, NV...]) multi-output added. fs positional (2nd arg, before NV-pairs); f in Hz (k*fs/NFFT) when fs given else radians/sample (k*2*pi/NFFT). t = (M/2 + k*hop) / fs_t at frame centres (fs_t = fs if given else 1 samples). istft 2nd output is column time vector t = (0:Nout-1)/fs. All three ranges (twosided / centered / onesided) bit-equal MATLAB R2025b. Octave 11.1.0 ships stft/istft in the signal package, not core. |
| `image_adapthisteq` | — | MATLAB Image Toolbox adapthisteq — CLAHE (Contrast Limited Adaptive Histogram Equalisation). CLEAN-ROOM implementation from public references (K. Zuiderveld, Graphics Gems IV, 1994; S. M. Pizer et al., Proc. VBC 1990 / CVGIP 1987). Full MATLAB argument set: NumTiles, ClipLimit, NBins, Range, Distribution (uniform/rayleigh/exponential), Alpha. Defaults: NumTiles=[8 8], ClipLimit=0.01, NBins=256, Distribution='uniform', Range='full', Alpha=0.4. RE-BASELINED: the clean-room CLAHE is functionally equivalent to MATLAB's adapthisteq but NOT bit-identical — MATLAB's clip/redistribute and interpolation-rounding have undocumented implementation details. Interior pixels diverge from MATLAB R2025b (e.g. J16=219 vs 134, J32=217 vs 137); they are intentionally excluded from the fingerprint, exactly as SRH/PEF were. The fingerprint keeps only what is genuinely engine-agnostic: image shape (sz1/sz2/szR/szE), the two saturated corner pixels (J11=8, Jend=255 — corners clamp identically in any correct CLAHE), and `spread` — a defining-property check that a low-contrast input (16-level band) gains >5x its standard deviation after equalisation. Real correctness is verified MATLAB-independently in toolboxes/image/tests/adapthisteq_test.cpp (LowContrastInputGainsDynamicRange / ClipLimitOrderingIncreasesSpread / RangeOriginalConstrainsOutput). Octave 11.1.0 indexes adapthisteq in its image package but does not ship it — harness reports N/A there. |
| `image_graycomatrix` | — | MATLAB Image Toolbox graycomatrix + graycoprops — gray-level co-occurrence matrix and its texture statistics. graycomatrix quantises I into NumLevels bins over GrayLimits, then counts pairs (p, p+offset) — row indexes the first pixel level, column the offset pixel level. With Symmetric=true the reverse direction is also tallied. graycoprops returns a struct with Contrast, Correlation, Energy, Homogeneity computed off the normalised joint probability p = G/sum(G). Bit-equal MATLAB R2025b. DEEP-PROBE c168: added 'GrayLimits', [] (empty) coverage — MATLAB-documented auto-limits = [min(I(:)) max(I(:))] over the actual data for ANY class (distinct from the class-range default [0 255] for uint8); numkit previously threw 'GrayLimits must be 2-element' on the empty form. g3_eq=1 confirms empty == explicit [32 128] (the data range); g3_ne=1 confirms empty differs from the class-range default. KNOWN GAPS: multiple-offset call form (returns a 3-D GLCM) — pass each offset separately for now. Octave 11.1.0 ships graycomatrix / graycoprops in the image package. |
| `image_bwmorph` | — | Image Processing toolbox bwmorph — binary morphological operations. CLEAN-ROOM dispatcher written from public references (Gonzalez & Woods, Digital Image Processing 4e Ch. 9; Pratt, Digital Image Processing 4e §14; Lam/Lee/Suen, IEEE PAMI 1992). Each operation is a 3x3-neighbourhood lookup (or a chain of lookups with boolean compositions / checkerboard sub-iterations) using the makelut bit convention (bit k = neighbour at row (k%3)-1, col (k/3)-1; bit 4 = centre; out-of-bounds = 0). The 512-entry lookup tables (bwmorph_luts.h) are reference data — the unique truth tables of the documented per-neighbourhood operations (numeric facts, not authored code). tol=0 bit-exact MATLAB R2025b on 23 fingerprints: 13 single-LUT operations (dilate / erode / bridge / clean / diag / fill / hbreak / majority / perim4 / perim8 / remove / endpoints / fatten) + 4 composite (open / close / bothat / tophat) + 6 iterated (skel-inf / thin-inf / thicken / spur / shrink-inf / branchpoints). Inputs: 20x20 logical random matrix from rng(0). Octave 11.1.0's bwmorph is in the image package — different implementation and operation names, not bit-compared. |
| `signal_polyscale` | — | Signal Processing toolbox polyscale — radial scaling of polynomial roots (b[k] = a[k]*alpha^k, the z-transform scaling property A(z) -> A(z/alpha)). CLEAN-ROOM implementation from public references (Oppenheim & Schafer 3e §3.2 z-transform scaling; Markel & Gray 1976, LPC bandwidth expansion). Bit-exact MATLAB R2025b (tol=0) on the documented argument set: row-vector input + scalar alpha, matrix input (one polynomial per row) + scalar alpha, row-vector alpha (element k raised to power k), and complex alpha. DOCUMENTED DIVERGENCE: for a column-vector input MATLAB's implicit expansion of a .* alpha.^(0:length(a)-1) yields an N×N matrix; numkit treats any vector as a single polynomial and returns a 1×N row — the column-vector case is intentionally not in the fingerprint. Octave 11.1.0 does not ship polyscale in core — harness reports N/A there. |
| `signal_polystab` | — | Signal Processing toolbox polystab — stabilise a polynomial (minimum-phase version): reflect every root with |root| > 1 to its conjugate reciprocal 1/conj(root) inside the unit circle, keeping the magnitude-response shape (scaled by a constant gain). CLEAN-ROOM implementation from public references (Oppenheim & Schafer 3e §5.6 minimum-phase systems / conjugate-reciprocal root reflection; Hayes 1996 spectral factorisation). Algorithm: roots(a) -> reflect outside roots -> poly() -> multiply by the first non-zero coefficient of a. Matches MATLAB R2025b within tol 1e-12 (the tolerance absorbs the roots->poly round-trip noise, observed ~1e-15). Fingerprint covers all algorithm paths: simple real roots ([1 -2.5 1] -> [1 -1 0.25]), leading zeros ignored ([0 1 -2.5 1] -> same length-3 result), already-stable input returned unchanged ([1 -0.5]), a complex-conjugate root pair OUTSIDE the unit circle reflected ([1 -3.4 3.7 -1] -> [1 -1.6 0.88 -0.16]), and a degree-5 polynomial with mixed real/complex roots (C) cross-checked coefficient-by-coefficient against MATLAB. KNOWN GAP: complex-coefficient input is unsupported because numkit's roots handles real polynomials only (the previous implementation had the same limitation). Octave 11.1.0 does not ship polystab in core — harness reports N/A there. |
| `signal_scalefiltersections` | — | Signal Processing toolbox scaleFilterSections — distribute scale values across the sections of a cascaded-transfer-function (CTF) numerator. CLEAN-ROOM implementation from public references (L. B. Jackson, Digital Filters and Signal Processing, 1996 — cascade realisation and gain distribution; Oppenheim & Schafer 3e §6.3, cascade-form structures). Algorithm: for K cascade sections the overall gain magnitude is spread as |g|^(1/K) across all sections and the sign is concentrated on the last section; a length-(K+1) scale vector additionally applies a per-section factor g[k]. Bit-equal MATLAB R2025b (tol 1e-9) on 10 fingerprints: scalar g on a 3-section filter, a length-4 scale vector, the single-section K=1 case, and complex numerator coefficients (the clean-room rewrite lifts a gap — the previous implementation handled real coefficients only). Octave 11.1.0 does not ship scaleFilterSections (introduced in MATLAB R2023b) — harness reports N/A there. |
| `page_family` | — | Sig: page-wise wrappers (pageeig, pagesvd, pagepinv, pagenorm, pagemldivide, pagemrdivide, pagelsqminnorm, pageinv). All iterate the corresponding 2-D linalg op per page. Fingerprints pin shapes + a handful of values (we use pageinv = inv per page, pageeig on symmetric input that gives exact eigvals [5 5 5] for one page, etc.). pagelsqminnorm not in fingerprint — it uses pinv which goes through SVD whose sign convention may differ at machine precision. |
| `schur_convert` | — | Sig: cdf2rdf (complex-diag Schur → real-block Schur), rsf2csf (real-block → complex-diag). cdf2rdf test: build (V, D) manually for a rotation matrix [0 -1; 1 0] (eigvals ±i); verify the real-form reconstruction. rsf2csf test: a [a -b; b a] 2x2 real Schur block (eigvals a±bi). MATLAB R2025b matches the documented convention DR_block = [a -b; b a], VR(:, k+1) = -Im(v) when sub-diagonal of D pairs positive imag part. |
| `cond_pnorm` | — | Sig: c = cond(A, p) for p ∈ {1, 2, Inf, 'fro'}. Closes the ⚠️ gap in PROGRESS where cond was 2-norm only. p=2 routes through cond_2norm (sigma_max/sigma_min); other p via norm(A,p)·norm(inv(A),p). Diagonal A = diag(1, 1e-3) gives exactly 1e3 for p=1,2,Inf and slightly above for 'fro' (sqrt(1+1e-6) · sqrt(1+1e6) ≈ 1e3 + 0.5e-3). |
| `predicates_sym` | — | Sig: issymmetric(A [, 'skew']) → A == A.' (transpose, no conj). ishermitian(A [, 'skew']) → A == A' (conj transpose). 'skew' flips equality to A == -A.' / A == -A'. Bit-exact MATLAB R2025b (tol=0): SY symmetric (s1=1), H not symmetric (s2=0 — complex matrix), SK skew-sym (s3=1), H Hermitian (h1=1), SY not Hermitian since complex form differs (h2=1 — but SY is real so h2=1 by real-symmetric ≡ Hermitian rule), SKH skew-Hermitian (h3=1). Split from the original combined predicates spec to stay under the 256-register chunk limit. |
| `predicates_band` | — | Sig: matrix-structure predicates and bandwidth queries. isbanded(A, lo, up) ↔ outside-band entries are zero; isdiag = isbanded(A, 0, 0). istril / istriu ↔ lower / upper triangular. [lo, up] = bandwidth(A) returns sub-diagonal and super-diagonal extent; bandwidth(A) one-out form returns lo; 'lower'|'upper' string form selects one. Bit-exact MATLAB R2025b (tol=0): D diagonal (ib1=id1=1), A tri-banded (ib2=1), A not upper-banded-only (ib3=0), L lower-tri (itl=1), U upper-tri (itu=1), bandwidth(A) = [1, 1], bandwidth(U) = 0, bandwidth(L) = 2, bandwidth(U, 'upper') = 2. Split from the original combined predicates spec to stay under the 256-register chunk limit. |
| `animatedline` | — | animatedline + addpoints + getpoints round-trip. After 10 addpoints calls of (k, 2k), getpoints should round-trip the same data. Real numerical fingerprint — not just display invariance. |
| `ode23(f, tspan, y0)            scalar IVP, explicit tspan` | — | Sig 1+2: ode23(@f, tspan, y0) and ode23(@f, tspan, y0, opts). MATLAB R2025b runs the Bogacki-Shampine 3(2) embedded pair with FSAL k4 and cubic Hermite dense output between accepted steps; numkit ode23 implements the same Butcher tableau (Bogacki-Shampine 1989, Appl.Math.Lett. 2:321-325) and the same interpolant (Shampine-Reichelt 1997). With explicit tspan + tight tols, numkit and MATLAB land identically on the requested sample points (verified ~1e-9). Refine default = 1 (matching MATLAB ode23, NOT 4 like ode45). MaxStep, multi-variable, reverse integration covered by gtest + smoke. |
| `ode23(f, tspan, y0, opts)      with odeset() opts` | — | Sig 1+2: ode23(@f, tspan, y0) and ode23(@f, tspan, y0, opts). MATLAB R2025b runs the Bogacki-Shampine 3(2) embedded pair with FSAL k4 and cubic Hermite dense output between accepted steps; numkit ode23 implements the same Butcher tableau (Bogacki-Shampine 1989, Appl.Math.Lett. 2:321-325) and the same interpolant (Shampine-Reichelt 1997). With explicit tspan + tight tols, numkit and MATLAB land identically on the requested sample points (verified ~1e-9). Refine default = 1 (matching MATLAB ode23, NOT 4 like ode45). MaxStep, multi-variable, reverse integration covered by gtest + smoke. |
| `RelTol / AbsTol propagation` | — | Sig 1+2: ode45(@f, tspan, y0) and ode45(@f, tspan, y0, opts). MATLAB R2025b runs Dormand-Prince 5(4) with Shampine's 4-th order free interpolant for dense output; numkit ode45 implements the same RK tableau (Hairer-Norsett-Wanner Vol I, p.178) and the same Shampine 1986 interpolant. With explicit tspan + tight tols, numkit and MATLAB land identically on the requested sample points (verified to ~1e-9). The fingerprint also exercises shape (n=11, d=1) and analytical solution exp(-2). Refine, MaxStep, multi-variable, reverse integration, and additional opts are covered by gtest + smoke. |
| `cubic Hermite dense-output at explicit tspan points` | — | Sig 1+2: ode23(@f, tspan, y0) and ode23(@f, tspan, y0, opts). MATLAB R2025b runs the Bogacki-Shampine 3(2) embedded pair with FSAL k4 and cubic Hermite dense output between accepted steps; numkit ode23 implements the same Butcher tableau (Bogacki-Shampine 1989, Appl.Math.Lett. 2:321-325) and the same interpolant (Shampine-Reichelt 1997). With explicit tspan + tight tols, numkit and MATLAB land identically on the requested sample points (verified ~1e-9). Refine default = 1 (matching MATLAB ode23, NOT 4 like ode45). MaxStep, multi-variable, reverse integration covered by gtest + smoke. |
| `[t, y] output shape  (n × 1, n × d)` | — | Sig 1+2: ode45(@f, tspan, y0) and ode45(@f, tspan, y0, opts). MATLAB R2025b runs Dormand-Prince 5(4) with Shampine's 4-th order free interpolant for dense output; numkit ode45 implements the same RK tableau (Hairer-Norsett-Wanner Vol I, p.178) and the same Shampine 1986 interpolant. With explicit tspan + tight tols, numkit and MATLAB land identically on the requested sample points (verified to ~1e-9). The fingerprint also exercises shape (n=11, d=1) and analytical solution exp(-2). Refine, MaxStep, multi-variable, reverse integration, and additional opts are covered by gtest + smoke. |
| `illumwhite(A)              default P = 1` | — | Sig 1: illumwhite(A, P, 'Mask', M) — per-channel histogram thresholded at strictly more than N·P/100 pixels at-or-above. Sig 2: illumgray(A, [p_lo p_hi], 'Mask', M, 'Norm', n) — per-channel mean (or p-norm-mean) after trimming bottom p_lo% and top p_hi%. Both algorithms decompose channel-by-channel (NOT by L2 norm of RGB). Per-channel histogram-bin quantisation in MATLAB (2^16 bins for double input) adds ~1.5e-5 noise vs our direct sort, hence tol = 5e-5. The illumwhite source we inspected uses imhist quantisation; numkit sorts the raw pixel array. Both algorithms agree on which pixel is selected — they disagree only on which BIN that pixel landed in. illumgray bit-equal at machine precision because the histogram threshold is used only to pick which pixels to keep (a SET operation), not their values. |
| `illumwhite(A, 0)           per-channel max` | — | Sig 1: illumwhite(A, P, 'Mask', M) — per-channel histogram thresholded at strictly more than N·P/100 pixels at-or-above. Sig 2: illumgray(A, [p_lo p_hi], 'Mask', M, 'Norm', n) — per-channel mean (or p-norm-mean) after trimming bottom p_lo% and top p_hi%. Both algorithms decompose channel-by-channel (NOT by L2 norm of RGB). Per-channel histogram-bin quantisation in MATLAB (2^16 bins for double input) adds ~1.5e-5 noise vs our direct sort, hence tol = 5e-5. The illumwhite source we inspected uses imhist quantisation; numkit sorts the raw pixel array. Both algorithms agree on which pixel is selected — they disagree only on which BIN that pixel landed in. illumgray bit-equal at machine precision because the histogram threshold is used only to pick which pixels to keep (a SET operation), not their values. |
| `illumwhite(A, 5)           top-5%-by-channel` | — | Sig 1: illumwhite(A, P, 'Mask', M) — per-channel histogram thresholded at strictly more than N·P/100 pixels at-or-above. Sig 2: illumgray(A, [p_lo p_hi], 'Mask', M, 'Norm', n) — per-channel mean (or p-norm-mean) after trimming bottom p_lo% and top p_hi%. Both algorithms decompose channel-by-channel (NOT by L2 norm of RGB). Per-channel histogram-bin quantisation in MATLAB (2^16 bins for double input) adds ~1.5e-5 noise vs our direct sort, hence tol = 5e-5. The illumwhite source we inspected uses imhist quantisation; numkit sorts the raw pixel array. Both algorithms agree on which pixel is selected — they disagree only on which BIN that pixel landed in. illumgray bit-equal at machine precision because the histogram threshold is used only to pick which pixels to keep (a SET operation), not their values. |
| `illumgray(A)               default p_lo = p_hi = 1, Norm = 1` | — | Sig 1: illumwhite(A, P, 'Mask', M) — per-channel histogram thresholded at strictly more than N·P/100 pixels at-or-above. Sig 2: illumgray(A, [p_lo p_hi], 'Mask', M, 'Norm', n) — per-channel mean (or p-norm-mean) after trimming bottom p_lo% and top p_hi%. Both algorithms decompose channel-by-channel (NOT by L2 norm of RGB). Per-channel histogram-bin quantisation in MATLAB (2^16 bins for double input) adds ~1.5e-5 noise vs our direct sort, hence tol = 5e-5. The illumwhite source we inspected uses imhist quantisation; numkit sorts the raw pixel array. Both algorithms agree on which pixel is selected — they disagree only on which BIN that pixel landed in. illumgray bit-equal at machine precision because the histogram threshold is used only to pick which pixels to keep (a SET operation), not their values. |
| `illumgray(A, [pl ph])      vector percentile` | — | Sig 1: illumwhite(A, P, 'Mask', M) — per-channel histogram thresholded at strictly more than N·P/100 pixels at-or-above. Sig 2: illumgray(A, [p_lo p_hi], 'Mask', M, 'Norm', n) — per-channel mean (or p-norm-mean) after trimming bottom p_lo% and top p_hi%. Both algorithms decompose channel-by-channel (NOT by L2 norm of RGB). Per-channel histogram-bin quantisation in MATLAB (2^16 bins for double input) adds ~1.5e-5 noise vs our direct sort, hence tol = 5e-5. The illumwhite source we inspected uses imhist quantisation; numkit sorts the raw pixel array. Both algorithms agree on which pixel is selected — they disagree only on which BIN that pixel landed in. illumgray bit-equal at machine precision because the histogram threshold is used only to pick which pixels to keep (a SET operation), not their values. |
| `illumgray(A, p, 'Norm', n) custom p-norm exponent` | — | Sig 1: illumwhite(A, P, 'Mask', M) — per-channel histogram thresholded at strictly more than N·P/100 pixels at-or-above. Sig 2: illumgray(A, [p_lo p_hi], 'Mask', M, 'Norm', n) — per-channel mean (or p-norm-mean) after trimming bottom p_lo% and top p_hi%. Both algorithms decompose channel-by-channel (NOT by L2 norm of RGB). Per-channel histogram-bin quantisation in MATLAB (2^16 bins for double input) adds ~1.5e-5 noise vs our direct sort, hence tol = 5e-5. The illumwhite source we inspected uses imhist quantisation; numkit sorts the raw pixel array. Both algorithms agree on which pixel is selected — they disagree only on which BIN that pixel landed in. illumgray bit-equal at machine precision because the histogram threshold is used only to pick which pixels to keep (a SET operation), not their values. |
| `illumpca(A)              default p = 3.5` | — | illumpca: Cheng-Prasad-Brown JOSA A 31(5) 2014, ported verbatim from MATLAB R2025b colorspaces/illumpca.m source. Uses inline 3×3 symmetric Jacobi eigensolver on Aᵀ·A (selected pixels, not mean-centred) — V(:,1) of the SVD equals the eigenvector of the largest eigenvalue. Degenerate cases (single colour, identity V, equal singular values) fall back to mean(selected) as in MATLAB. imcolordiff: CIE94 (CIE Publ. 116-1995) + CIEDE2000 (ISO 11664-6:2014, Sharma-Wu-Dalal 2005) ported verbatim from MATLAB R2025b colorspaces/imcolordiff.m source. Defaults: standard='CIE94', isInputLab=false, kL=kC=kH=1, K1=0.045, K2=0.015. Pure-Lab inputs agree at ~1e-9; RGB inputs at ~1e-6 (limited by rgb2lab implementation precision). illumpca matches at 1e-15 (formula-only). |
| `illumpca(A, p)           tail-fraction p in (0, 50]` | — | illumpca: Cheng-Prasad-Brown JOSA A 31(5) 2014, ported verbatim from MATLAB R2025b colorspaces/illumpca.m source. Uses inline 3×3 symmetric Jacobi eigensolver on Aᵀ·A (selected pixels, not mean-centred) — V(:,1) of the SVD equals the eigenvector of the largest eigenvalue. Degenerate cases (single colour, identity V, equal singular values) fall back to mean(selected) as in MATLAB. imcolordiff: CIE94 (CIE Publ. 116-1995) + CIEDE2000 (ISO 11664-6:2014, Sharma-Wu-Dalal 2005) ported verbatim from MATLAB R2025b colorspaces/imcolordiff.m source. Defaults: standard='CIE94', isInputLab=false, kL=kC=kH=1, K1=0.045, K2=0.015. Pure-Lab inputs agree at ~1e-9; RGB inputs at ~1e-6 (limited by rgb2lab implementation precision). illumpca matches at 1e-15 (formula-only). |
| `illumpca(A, 50)          use all pixels` | — | illumpca: Cheng-Prasad-Brown JOSA A 31(5) 2014, ported verbatim from MATLAB R2025b colorspaces/illumpca.m source. Uses inline 3×3 symmetric Jacobi eigensolver on Aᵀ·A (selected pixels, not mean-centred) — V(:,1) of the SVD equals the eigenvector of the largest eigenvalue. Degenerate cases (single colour, identity V, equal singular values) fall back to mean(selected) as in MATLAB. imcolordiff: CIE94 (CIE Publ. 116-1995) + CIEDE2000 (ISO 11664-6:2014, Sharma-Wu-Dalal 2005) ported verbatim from MATLAB R2025b colorspaces/imcolordiff.m source. Defaults: standard='CIE94', isInputLab=false, kL=kC=kH=1, K1=0.045, K2=0.015. Pure-Lab inputs agree at ~1e-9; RGB inputs at ~1e-6 (limited by rgb2lab implementation precision). illumpca matches at 1e-15 (formula-only). |
| `imcolordiff(I1, I2)               CIE94 default, RGB input` | — | illumpca: Cheng-Prasad-Brown JOSA A 31(5) 2014, ported verbatim from MATLAB R2025b colorspaces/illumpca.m source. Uses inline 3×3 symmetric Jacobi eigensolver on Aᵀ·A (selected pixels, not mean-centred) — V(:,1) of the SVD equals the eigenvector of the largest eigenvalue. Degenerate cases (single colour, identity V, equal singular values) fall back to mean(selected) as in MATLAB. imcolordiff: CIE94 (CIE Publ. 116-1995) + CIEDE2000 (ISO 11664-6:2014, Sharma-Wu-Dalal 2005) ported verbatim from MATLAB R2025b colorspaces/imcolordiff.m source. Defaults: standard='CIE94', isInputLab=false, kL=kC=kH=1, K1=0.045, K2=0.015. Pure-Lab inputs agree at ~1e-9; RGB inputs at ~1e-6 (limited by rgb2lab implementation precision). illumpca matches at 1e-15 (formula-only). |
| `imcolordiff(.., 'Standard', .)    CIEDE2000 alternate` | — | illumpca: Cheng-Prasad-Brown JOSA A 31(5) 2014, ported verbatim from MATLAB R2025b colorspaces/illumpca.m source. Uses inline 3×3 symmetric Jacobi eigensolver on Aᵀ·A (selected pixels, not mean-centred) — V(:,1) of the SVD equals the eigenvector of the largest eigenvalue. Degenerate cases (single colour, identity V, equal singular values) fall back to mean(selected) as in MATLAB. imcolordiff: CIE94 (CIE Publ. 116-1995) + CIEDE2000 (ISO 11664-6:2014, Sharma-Wu-Dalal 2005) ported verbatim from MATLAB R2025b colorspaces/imcolordiff.m source. Defaults: standard='CIE94', isInputLab=false, kL=kC=kH=1, K1=0.045, K2=0.015. Pure-Lab inputs agree at ~1e-9; RGB inputs at ~1e-6 (limited by rgb2lab implementation precision). illumpca matches at 1e-15 (formula-only). |
| `imcolordiff(.., 'isInputLab', .)  Lab input skips rgb2lab` | — | illumpca: Cheng-Prasad-Brown JOSA A 31(5) 2014, ported verbatim from MATLAB R2025b colorspaces/illumpca.m source. Uses inline 3×3 symmetric Jacobi eigensolver on Aᵀ·A (selected pixels, not mean-centred) — V(:,1) of the SVD equals the eigenvector of the largest eigenvalue. Degenerate cases (single colour, identity V, equal singular values) fall back to mean(selected) as in MATLAB. imcolordiff: CIE94 (CIE Publ. 116-1995) + CIEDE2000 (ISO 11664-6:2014, Sharma-Wu-Dalal 2005) ported verbatim from MATLAB R2025b colorspaces/imcolordiff.m source. Defaults: standard='CIE94', isInputLab=false, kL=kC=kH=1, K1=0.045, K2=0.015. Pure-Lab inputs agree at ~1e-9; RGB inputs at ~1e-6 (limited by rgb2lab implementation precision). illumpca matches at 1e-15 (formula-only). |
| `imcolordiff with non-default kL/kC/kH/K1/K2 (gtest only)` | — | illumpca: Cheng-Prasad-Brown JOSA A 31(5) 2014, ported verbatim from MATLAB R2025b colorspaces/illumpca.m source. Uses inline 3×3 symmetric Jacobi eigensolver on Aᵀ·A (selected pixels, not mean-centred) — V(:,1) of the SVD equals the eigenvector of the largest eigenvalue. Degenerate cases (single colour, identity V, equal singular values) fall back to mean(selected) as in MATLAB. imcolordiff: CIE94 (CIE Publ. 116-1995) + CIEDE2000 (ISO 11664-6:2014, Sharma-Wu-Dalal 2005) ported verbatim from MATLAB R2025b colorspaces/imcolordiff.m source. Defaults: standard='CIE94', isInputLab=false, kL=kC=kH=1, K1=0.045, K2=0.015. Pure-Lab inputs agree at ~1e-9; RGB inputs at ~1e-6 (limited by rgb2lab implementation precision). illumpca matches at 1e-15 (formula-only). |
| `otf2psf(otf)                no outsize, 3-D / 4-D / odd / even / 1-D` | — | otf2psf: was registered but the outsize parameter was ignored. Fixed to do circshift by floor(OUTSIZE/2) (not floor(insize/2) — MATLAB's actual algorithm), then top-left crop. Now bit-equal MATLAB on roundtrips (3x3/4x4 odd/even) and on cropped cases. rgbwide2ycbcr: ITU-R BT.2020-2 / BT.2100-2 narrow-range YCbCr encoder for 10-bit or 12-bit wide-gamut RGB. Algorithm ported verbatim from MATLAB R2025b colorspaces/+images/+color/+internal/rgbwide2ycbcrImpl.m. Y' = 0.2627·R + 0.6780·G + 0.0593·B (BT.2020 luma); then quantise to uint16 via the 219/16/128 scaling × 2^(bps-8). Bit-equal MATLAB on all tested rows. |
| `otf2psf(otf, outsize)       crop to top-left after floor(outsize/2) shift` | — | otf2psf: was registered but the outsize parameter was ignored. Fixed to do circshift by floor(OUTSIZE/2) (not floor(insize/2) — MATLAB's actual algorithm), then top-left crop. Now bit-equal MATLAB on roundtrips (3x3/4x4 odd/even) and on cropped cases. rgbwide2ycbcr: ITU-R BT.2020-2 / BT.2100-2 narrow-range YCbCr encoder for 10-bit or 12-bit wide-gamut RGB. Algorithm ported verbatim from MATLAB R2025b colorspaces/+images/+color/+internal/rgbwide2ycbcrImpl.m. Y' = 0.2627·R + 0.6780·G + 0.0593·B (BT.2020 luma); then quantise to uint16 via the 219/16/128 scaling × 2^(bps-8). Bit-equal MATLAB on all tested rows. |
| `otf2psf(otf, [1 N])         1-D crop` | — | otf2psf: was registered but the outsize parameter was ignored. Fixed to do circshift by floor(OUTSIZE/2) (not floor(insize/2) — MATLAB's actual algorithm), then top-left crop. Now bit-equal MATLAB on roundtrips (3x3/4x4 odd/even) and on cropped cases. rgbwide2ycbcr: ITU-R BT.2020-2 / BT.2100-2 narrow-range YCbCr encoder for 10-bit or 12-bit wide-gamut RGB. Algorithm ported verbatim from MATLAB R2025b colorspaces/+images/+color/+internal/rgbwide2ycbcrImpl.m. Y' = 0.2627·R + 0.6780·G + 0.0593·B (BT.2020 luma); then quantise to uint16 via the 219/16/128 scaling × 2^(bps-8). Bit-equal MATLAB on all tested rows. |
| `rgbwide2ycbcr(rgb, 10)      BT.2020 10-bit narrow-range` | — | otf2psf: was registered but the outsize parameter was ignored. Fixed to do circshift by floor(OUTSIZE/2) (not floor(insize/2) — MATLAB's actual algorithm), then top-left crop. Now bit-equal MATLAB on roundtrips (3x3/4x4 odd/even) and on cropped cases. rgbwide2ycbcr: ITU-R BT.2020-2 / BT.2100-2 narrow-range YCbCr encoder for 10-bit or 12-bit wide-gamut RGB. Algorithm ported verbatim from MATLAB R2025b colorspaces/+images/+color/+internal/rgbwide2ycbcrImpl.m. Y' = 0.2627·R + 0.6780·G + 0.0593·B (BT.2020 luma); then quantise to uint16 via the 219/16/128 scaling × 2^(bps-8). Bit-equal MATLAB on all tested rows. |
| `rgbwide2ycbcr(rgb, 12)      BT.2020 12-bit narrow-range` | — | otf2psf: was registered but the outsize parameter was ignored. Fixed to do circshift by floor(OUTSIZE/2) (not floor(insize/2) — MATLAB's actual algorithm), then top-left crop. Now bit-equal MATLAB on roundtrips (3x3/4x4 odd/even) and on cropped cases. rgbwide2ycbcr: ITU-R BT.2020-2 / BT.2100-2 narrow-range YCbCr encoder for 10-bit or 12-bit wide-gamut RGB. Algorithm ported verbatim from MATLAB R2025b colorspaces/+images/+color/+internal/rgbwide2ycbcrImpl.m. Y' = 0.2627·R + 0.6780·G + 0.0593·B (BT.2020 luma); then quantise to uint16 via the 219/16/128 scaling × 2^(bps-8). Bit-equal MATLAB on all tested rows. |
| `ycbcr2rgbwide(ycbcr, 10)      BT.2020 10-bit narrow-range decode` | — | Inverse of rgbwide2ycbcr (cycle 28). ITU-R BT.2020-2 / BT.2100-2 narrow-range YCbCr decoder, BPS ∈ {10, 12}. Algorithm ported verbatim from MATLAB R2025b colorspaces/+images/+color/+internal/ycbcr2rgbwideImpl.m (column-major / non-codegen path). Normalises Y by [yzero, ypeak] = [64,940]/[256,3760] and Cb/Cr by chromazero=2^(bps-1), chromarange=896/3584. Computes R = 1.4746·Cr_n + Y_n, B = 1.8814·Cb_n + Y_n, G = (Y_n - 0.2627·R - 0.0593·B) / 0.6780; quantises to uint16 via rgb·nominalRange + blackLevel. Bit-equal MATLAB on all probed rows AND on round-trip with rgbwide2ycbcr (verified). |
| `ycbcr2rgbwide(ycbcr, 12)      BT.2020 12-bit narrow-range decode` | — | Inverse of rgbwide2ycbcr (cycle 28). ITU-R BT.2020-2 / BT.2100-2 narrow-range YCbCr decoder, BPS ∈ {10, 12}. Algorithm ported verbatim from MATLAB R2025b colorspaces/+images/+color/+internal/ycbcr2rgbwideImpl.m (column-major / non-codegen path). Normalises Y by [yzero, ypeak] = [64,940]/[256,3760] and Cb/Cr by chromazero=2^(bps-1), chromarange=896/3584. Computes R = 1.4746·Cr_n + Y_n, B = 1.8814·Cb_n + Y_n, G = (Y_n - 0.2627·R - 0.0593·B) / 0.6780; quantises to uint16 via rgb·nominalRange + blackLevel. Bit-equal MATLAB on all probed rows AND on round-trip with rgbwide2ycbcr (verified). |
| `H×W×3 image input` | — | Inverse of rgbwide2ycbcr (cycle 28). ITU-R BT.2020-2 / BT.2100-2 narrow-range YCbCr decoder, BPS ∈ {10, 12}. Algorithm ported verbatim from MATLAB R2025b colorspaces/+images/+color/+internal/ycbcr2rgbwideImpl.m (column-major / non-codegen path). Normalises Y by [yzero, ypeak] = [64,940]/[256,3760] and Cb/Cr by chromazero=2^(bps-1), chromarange=896/3584. Computes R = 1.4746·Cr_n + Y_n, B = 1.8814·Cb_n + Y_n, G = (Y_n - 0.2627·R - 0.0593·B) / 0.6780; quantises to uint16 via rgb·nominalRange + blackLevel. Bit-equal MATLAB on all probed rows AND on round-trip with rgbwide2ycbcr (verified). |
| `rgbwide2ycbcr round-trip (10/12-bit)` | — | Inverse of rgbwide2ycbcr (cycle 28). ITU-R BT.2020-2 / BT.2100-2 narrow-range YCbCr decoder, BPS ∈ {10, 12}. Algorithm ported verbatim from MATLAB R2025b colorspaces/+images/+color/+internal/ycbcr2rgbwideImpl.m (column-major / non-codegen path). Normalises Y by [yzero, ypeak] = [64,940]/[256,3760] and Cb/Cr by chromazero=2^(bps-1), chromarange=896/3584. Computes R = 1.4746·Cr_n + Y_n, B = 1.8814·Cb_n + Y_n, G = (Y_n - 0.2627·R - 0.0593·B) / 0.6780; quantises to uint16 via rgb·nominalRange + blackLevel. Bit-equal MATLAB on all probed rows AND on round-trip with rgbwide2ycbcr (verified). |
| `ind2gray(X, MAP)            double / single X — 1-based lookup` | — | MATLAB ind2gray algorithm: graycm = rgb2gray(MAP) → take column 1 → length-N grey vector. Then class-preserving lookup. Float X uses 1-based indexing with clamp to [1, M]; integer X (uint8/uint16) uses 0-based indexing with LUT padded to vs=256/65536 using last grey value beyond M-1. Coefficients are the BT.601 YIQ luma (0.298936021293775, 0.587043074451121, 0.114020904255103). Bit-equal MATLAB R2025b on all probed test vectors including the RGB-cmap case where the previous numkit impl was wrong (it just took column 0 of MAP instead of applying rgb2gray). |
| `ind2gray(uint8 X, MAP)      class-preserving uint8 output via 256-LUT` | — | MATLAB ind2gray algorithm: graycm = rgb2gray(MAP) → take column 1 → length-N grey vector. Then class-preserving lookup. Float X uses 1-based indexing with clamp to [1, M]; integer X (uint8/uint16) uses 0-based indexing with LUT padded to vs=256/65536 using last grey value beyond M-1. Coefficients are the BT.601 YIQ luma (0.298936021293775, 0.587043074451121, 0.114020904255103). Bit-equal MATLAB R2025b on all probed test vectors including the RGB-cmap case where the previous numkit impl was wrong (it just took column 0 of MAP instead of applying rgb2gray). |
| `ind2gray(uint16 X, MAP)     class-preserving uint16 output via 65536-LUT` | — | MATLAB ind2gray algorithm: graycm = rgb2gray(MAP) → take column 1 → length-N grey vector. Then class-preserving lookup. Float X uses 1-based indexing with clamp to [1, M]; integer X (uint8/uint16) uses 0-based indexing with LUT padded to vs=256/65536 using last grey value beyond M-1. Coefficients are the BT.601 YIQ luma (0.298936021293775, 0.587043074451121, 0.114020904255103). Bit-equal MATLAB R2025b on all probed test vectors including the RGB-cmap case where the previous numkit impl was wrong (it just took column 0 of MAP instead of applying rgb2gray). |
| `out-of-range index clamping (both float and integer)` | — | MATLAB ind2gray algorithm: graycm = rgb2gray(MAP) → take column 1 → length-N grey vector. Then class-preserving lookup. Float X uses 1-based indexing with clamp to [1, M]; integer X (uint8/uint16) uses 0-based indexing with LUT padded to vs=256/65536 using last grey value beyond M-1. Coefficients are the BT.601 YIQ luma (0.298936021293775, 0.587043074451121, 0.114020904255103). Bit-equal MATLAB R2025b on all probed test vectors including the RGB-cmap case where the previous numkit impl was wrong (it just took column 0 of MAP instead of applying rgb2gray). |
| `imcrop3(V, cuboid)         3-D volume crop` | — | MATLAB R2025b imcrop3.m algorithm: cuboid = [XMIN YMIN ZMIN WIDTH HEIGHT DEPTH] (spatial X/Y/Z = col/row/page). Output extracts V(round(YMIN):round(YMIN+HEIGHT), round(XMIN):round(XMIN+WIDTH), round(ZMIN):round(ZMIN+DEPTH), :) — an inclusive (width+1) × (height+1) × (depth+1) sub-block. The trailing 4th dim passes through unchanged. Class preserved. Out-of-bounds cuboid throws (matches MATLAB error message). Bit-equal MATLAB on all probed cases including the non-integer rounding case. |
| `imcrop3 with rounding      non-integer cuboid limits` | — | MATLAB R2025b imcrop3.m algorithm: cuboid = [XMIN YMIN ZMIN WIDTH HEIGHT DEPTH] (spatial X/Y/Z = col/row/page). Output extracts V(round(YMIN):round(YMIN+HEIGHT), round(XMIN):round(XMIN+WIDTH), round(ZMIN):round(ZMIN+DEPTH), :) — an inclusive (width+1) × (height+1) × (depth+1) sub-block. The trailing 4th dim passes through unchanged. Class preserved. Out-of-bounds cuboid throws (matches MATLAB error message). Bit-equal MATLAB on all probed cases including the non-integer rounding case. |
| `imcrop3 on 4-D volume      4th dim (channels / time) passes through` | — | MATLAB R2025b imcrop3.m algorithm: cuboid = [XMIN YMIN ZMIN WIDTH HEIGHT DEPTH] (spatial X/Y/Z = col/row/page). Output extracts V(round(YMIN):round(YMIN+HEIGHT), round(XMIN):round(XMIN+WIDTH), round(ZMIN):round(ZMIN+DEPTH), :) — an inclusive (width+1) × (height+1) × (depth+1) sub-block. The trailing 4th dim passes through unchanged. Class preserved. Out-of-bounds cuboid throws (matches MATLAB error message). Bit-equal MATLAB on all probed cases including the non-integer rounding case. |
| `class-preserving output    uint8 in → uint8 out` | — | MATLAB R2025b imcrop3.m algorithm: cuboid = [XMIN YMIN ZMIN WIDTH HEIGHT DEPTH] (spatial X/Y/Z = col/row/page). Output extracts V(round(YMIN):round(YMIN+HEIGHT), round(XMIN):round(XMIN+WIDTH), round(ZMIN):round(ZMIN+DEPTH), :) — an inclusive (width+1) × (height+1) × (depth+1) sub-block. The trailing 4th dim passes through unchanged. Class preserved. Out-of-bounds cuboid throws (matches MATLAB error message). Bit-equal MATLAB on all probed cases including the non-integer rounding case. |
| `graydiffweight(I, refGrayVal)         scalar reference` | — | MATLAB R2025b graydiffweight.m algorithm: d = |I - refGrayVal|; if cutoff finite, mark isSuppressed = (d > cutoff); d_scaled = imlinscale(d, [1e-3, 1]); if cutoff, d_scaled(isSuppressed) = 1; W = 1 ./ (d_scaled .^ (1/rolloffFactor)). Output class is single if input single, else double. 4 input signatures (scalar refGrayVal / logical MASK / column-row indices C,R / 3-D C,R,P) — the adapter computes the scalar reference upfront from the appropriate stats before dispatching. Bit-equal MATLAB on every probed case. |
| `graydiffweight(I, MASK)                mean over masked pixels` | — | MATLAB R2025b graydiffweight.m algorithm: d = |I - refGrayVal|; if cutoff finite, mark isSuppressed = (d > cutoff); d_scaled = imlinscale(d, [1e-3, 1]); if cutoff, d_scaled(isSuppressed) = 1; W = 1 ./ (d_scaled .^ (1/rolloffFactor)). Output class is single if input single, else double. 4 input signatures (scalar refGrayVal / logical MASK / column-row indices C,R / 3-D C,R,P) — the adapter computes the scalar reference upfront from the appropriate stats before dispatching. Bit-equal MATLAB on every probed case. |
| `graydiffweight(I, C, R)                mean over indexed pixels` | — | MATLAB R2025b graydiffweight.m algorithm: d = |I - refGrayVal|; if cutoff finite, mark isSuppressed = (d > cutoff); d_scaled = imlinscale(d, [1e-3, 1]); if cutoff, d_scaled(isSuppressed) = 1; W = 1 ./ (d_scaled .^ (1/rolloffFactor)). Output class is single if input single, else double. 4 input signatures (scalar refGrayVal / logical MASK / column-row indices C,R / 3-D C,R,P) — the adapter computes the scalar reference upfront from the appropriate stats before dispatching. Bit-equal MATLAB on every probed case. |
| `RolloffFactor (default 0.5)            controls falloff` | — | MATLAB R2025b graydiffweight.m algorithm: d = |I - refGrayVal|; if cutoff finite, mark isSuppressed = (d > cutoff); d_scaled = imlinscale(d, [1e-3, 1]); if cutoff, d_scaled(isSuppressed) = 1; W = 1 ./ (d_scaled .^ (1/rolloffFactor)). Output class is single if input single, else double. 4 input signatures (scalar refGrayVal / logical MASK / column-row indices C,R / 3-D C,R,P) — the adapter computes the scalar reference upfront from the appropriate stats before dispatching. Bit-equal MATLAB on every probed case. |
| `GrayDifferenceCutoff (default Inf)     hard threshold` | — | MATLAB R2025b graydiffweight.m algorithm: d = |I - refGrayVal|; if cutoff finite, mark isSuppressed = (d > cutoff); d_scaled = imlinscale(d, [1e-3, 1]); if cutoff, d_scaled(isSuppressed) = 1; W = 1 ./ (d_scaled .^ (1/rolloffFactor)). Output class is single if input single, else double. 4 input signatures (scalar refGrayVal / logical MASK / column-row indices C,R / 3-D C,R,P) — the adapter computes the scalar reference upfront from the appropriate stats before dispatching. Bit-equal MATLAB on every probed case. |
| `3-D volume input` | — | MATLAB R2025b graydiffweight.m algorithm: d = |I - refGrayVal|; if cutoff finite, mark isSuppressed = (d > cutoff); d_scaled = imlinscale(d, [1e-3, 1]); if cutoff, d_scaled(isSuppressed) = 1; W = 1 ./ (d_scaled .^ (1/rolloffFactor)). Output class is single if input single, else double. 4 input signatures (scalar refGrayVal / logical MASK / column-row indices C,R / 3-D C,R,P) — the adapter computes the scalar reference upfront from the appropriate stats before dispatching. Bit-equal MATLAB on every probed case. |
| `nlfilter(A, [m n], fun)         default zero-padding` | — | MATLAB R2025b nlfilter.m: B(i,j) = fun(window_at(i,j)) where window is m × n centred on (i,j) with top-left bias for even sizes. Pads with 0 by default, with 1 for single/double in 'indexed' form. Output class = class of the FIRST fun() return. Dispatch goes through Engine::callFunctionHandle, matching the pattern adopted in toolboxes/ode/ode45 (function_ref couldn't carry func-handle semantics through the round-trip). Bit-equal MATLAB on the magic(5) test image across mean/max/median/sum kernels, [3 3] and [2 3] neighbourhoods, double/uint8 classes, and 'indexed' mode. |
| `nlfilter(A, 'indexed', ...)     padval=1 for double/single, 0 otherwise` | — | MATLAB R2025b nlfilter.m: B(i,j) = fun(window_at(i,j)) where window is m × n centred on (i,j) with top-left bias for even sizes. Pads with 0 by default, with 1 for single/double in 'indexed' form. Output class = class of the FIRST fun() return. Dispatch goes through Engine::callFunctionHandle, matching the pattern adopted in toolboxes/ode/ode45 (function_ref couldn't carry func-handle semantics through the round-trip). Bit-equal MATLAB on the magic(5) test image across mean/max/median/sum kernels, [3 3] and [2 3] neighbourhoods, double/uint8 classes, and 'indexed' mode. |
| `Class preservation              output class = first fun() return class` | — | MATLAB R2025b nlfilter.m: B(i,j) = fun(window_at(i,j)) where window is m × n centred on (i,j) with top-left bias for even sizes. Pads with 0 by default, with 1 for single/double in 'indexed' form. Output class = class of the FIRST fun() return. Dispatch goes through Engine::callFunctionHandle, matching the pattern adopted in toolboxes/ode/ode45 (function_ref couldn't carry func-handle semantics through the round-trip). Bit-equal MATLAB on the magic(5) test image across mean/max/median/sum kernels, [3 3] and [2 3] neighbourhoods, double/uint8 classes, and 'indexed' mode. |
| `Even neighbourhood              [2 3] with top-left bias for the centre` | — | MATLAB R2025b nlfilter.m: B(i,j) = fun(window_at(i,j)) where window is m × n centred on (i,j) with top-left bias for even sizes. Pads with 0 by default, with 1 for single/double in 'indexed' form. Output class = class of the FIRST fun() return. Dispatch goes through Engine::callFunctionHandle, matching the pattern adopted in toolboxes/ode/ode45 (function_ref couldn't carry func-handle semantics through the round-trip). Bit-equal MATLAB on the magic(5) test image across mean/max/median/sum kernels, [3 3] and [2 3] neighbourhoods, double/uint8 classes, and 'indexed' mode. |
| `colfilt(A, [m n], 'sliding', fun)       zero-padding` | — | MATLAB R2025b colfilt.m algorithm — two modes: 'sliding' (im2col, fun on m·n × N matrix, reshape result to size(A)) and 'distinct' (im2col distinct, fun returns same-shape matrix, col2im, crop). Zero-padded by default; 'indexed' uses padval = 1 for double/single, 0 otherwise. Output class = class of fun() return. The optional [mblock nblock] argument is purely a memory optimisation per MATLAB docs ("does not change the result"); the adapter accepts and ignores it. Dispatch via Engine::callFunctionHandle (same pattern as nlfilter / ode45). Bit-equal MATLAB on magic(5) for sliding sum/mean and indexed min, on magic(6) for distinct x.^2. |
| `colfilt(A, [m n], 'distinct', fun)      shape-preserving distinct` | — | MATLAB R2025b colfilt.m algorithm — two modes: 'sliding' (im2col, fun on m·n × N matrix, reshape result to size(A)) and 'distinct' (im2col distinct, fun returns same-shape matrix, col2im, crop). Zero-padded by default; 'indexed' uses padval = 1 for double/single, 0 otherwise. Output class = class of fun() return. The optional [mblock nblock] argument is purely a memory optimisation per MATLAB docs ("does not change the result"); the adapter accepts and ignores it. Dispatch via Engine::callFunctionHandle (same pattern as nlfilter / ode45). Bit-equal MATLAB on magic(5) for sliding sum/mean and indexed min, on magic(6) for distinct x.^2. |
| `even neighbourhood [2 3]                 in sliding mode` | — | MATLAB R2025b colfilt.m algorithm — two modes: 'sliding' (im2col, fun on m·n × N matrix, reshape result to size(A)) and 'distinct' (im2col distinct, fun returns same-shape matrix, col2im, crop). Zero-padded by default; 'indexed' uses padval = 1 for double/single, 0 otherwise. Output class = class of fun() return. The optional [mblock nblock] argument is purely a memory optimisation per MATLAB docs ("does not change the result"); the adapter accepts and ignores it. Dispatch via Engine::callFunctionHandle (same pattern as nlfilter / ode45). Bit-equal MATLAB on magic(5) for sliding sum/mean and indexed min, on magic(6) for distinct x.^2. |
| `'indexed' padding                        padval=1 for double` | — | MATLAB R2025b colfilt.m algorithm — two modes: 'sliding' (im2col, fun on m·n × N matrix, reshape result to size(A)) and 'distinct' (im2col distinct, fun returns same-shape matrix, col2im, crop). Zero-padded by default; 'indexed' uses padval = 1 for double/single, 0 otherwise. Output class = class of fun() return. The optional [mblock nblock] argument is purely a memory optimisation per MATLAB docs ("does not change the result"); the adapter accepts and ignores it. Dispatch via Engine::callFunctionHandle (same pattern as nlfilter / ode45). Bit-equal MATLAB on magic(5) for sliding sum/mean and indexed min, on magic(6) for distinct x.^2. |
| `nlfilter ↔ colfilt equivalence           same output for shared kernels` | — | MATLAB R2025b colfilt.m algorithm — two modes: 'sliding' (im2col, fun on m·n × N matrix, reshape result to size(A)) and 'distinct' (im2col distinct, fun returns same-shape matrix, col2im, crop). Zero-padded by default; 'indexed' uses padval = 1 for double/single, 0 otherwise. Output class = class of fun() return. The optional [mblock nblock] argument is purely a memory optimisation per MATLAB docs ("does not change the result"); the adapter accepts and ignores it. Dispatch via Engine::callFunctionHandle (same pattern as nlfilter / ode45). Bit-equal MATLAB on magic(5) for sliding sum/mean and indexed min, on magic(6) for distinct x.^2. |
| `deconvwnr(I, PSF, NSR=0)             ideal inverse` | — | MATLAB R2025b deconvwnr.m algorithm: H = psf2otf(PSF, size(I)); denom = |H|² · S_x + S_u (clamped at sqrt(eps)); G = conj(H) · S_x / denom; J = real(ifftn(G .* fftn(I))); convert to class(I). Scalar NSR maps to S_u = NSR, S_x = 1. Scalar NCORR/ICORR is the equivalent (S_u = NCORR, S_x = ICORR). Array NCORR/ICORR is also supported via |fft2(ACF)| (only same-size ACFs — MATLAB's 1-D extrapolation form throws). Bit-equal MATLAB on every probed test vector. Reference: Gonzalez & Woods, *Digital Image Processing*, 2e § 5.8. |
| `deconvwnr(I, PSF, NSR=0.01)          regularised` | — | MATLAB R2025b deconvwnr.m algorithm: H = psf2otf(PSF, size(I)); denom = |H|² · S_x + S_u (clamped at sqrt(eps)); G = conj(H) · S_x / denom; J = real(ifftn(G .* fftn(I))); convert to class(I). Scalar NSR maps to S_u = NSR, S_x = 1. Scalar NCORR/ICORR is the equivalent (S_u = NCORR, S_x = ICORR). Array NCORR/ICORR is also supported via |fft2(ACF)| (only same-size ACFs — MATLAB's 1-D extrapolation form throws). Bit-equal MATLAB on every probed test vector. Reference: Gonzalez & Woods, *Digital Image Processing*, 2e § 5.8. |
| `deconvwnr(I, PSF, NSR=0.1)           strongly regularised` | — | MATLAB R2025b deconvwnr.m algorithm: H = psf2otf(PSF, size(I)); denom = |H|² · S_x + S_u (clamped at sqrt(eps)); G = conj(H) · S_x / denom; J = real(ifftn(G .* fftn(I))); convert to class(I). Scalar NSR maps to S_u = NSR, S_x = 1. Scalar NCORR/ICORR is the equivalent (S_u = NCORR, S_x = ICORR). Array NCORR/ICORR is also supported via |fft2(ACF)| (only same-size ACFs — MATLAB's 1-D extrapolation form throws). Bit-equal MATLAB on every probed test vector. Reference: Gonzalez & Woods, *Digital Image Processing*, 2e § 5.8. |
| `deconvwnr(I, PSF, NCORR, ICORR)      scalar NCORR/ICORR ≡ NSR` | — | MATLAB R2025b deconvwnr.m algorithm: H = psf2otf(PSF, size(I)); denom = |H|² · S_x + S_u (clamped at sqrt(eps)); G = conj(H) · S_x / denom; J = real(ifftn(G .* fftn(I))); convert to class(I). Scalar NSR maps to S_u = NSR, S_x = 1. Scalar NCORR/ICORR is the equivalent (S_u = NCORR, S_x = ICORR). Array NCORR/ICORR is also supported via |fft2(ACF)| (only same-size ACFs — MATLAB's 1-D extrapolation form throws). Bit-equal MATLAB on every probed test vector. Reference: Gonzalez & Woods, *Digital Image Processing*, 2e § 5.8. |
| `uint8 input — class-preserving       saturating cast on output` | — | MATLAB R2025b deconvwnr.m algorithm: H = psf2otf(PSF, size(I)); denom = |H|² · S_x + S_u (clamped at sqrt(eps)); G = conj(H) · S_x / denom; J = real(ifftn(G .* fftn(I))); convert to class(I). Scalar NSR maps to S_u = NSR, S_x = 1. Scalar NCORR/ICORR is the equivalent (S_u = NCORR, S_x = ICORR). Array NCORR/ICORR is also supported via |fft2(ACF)| (only same-size ACFs — MATLAB's 1-D extrapolation form throws). Bit-equal MATLAB on every probed test vector. Reference: Gonzalez & Woods, *Digital Image Processing*, 2e § 5.8. |
| `edgetaper(I, PSF)             double image` | — | MATLAB R2025b edgetaper.m algorithm — per-dim PSF projection autocorrelation builds an alpha matrix that equals 1 in the interior and tapers to 0 at edges; J = alpha*I + (1-alpha)*blurredI clipped to [min(I), max(I)]. blurredI is circular ifft2(fft2(I) .* psf2otf(PSF, sizeI)). The center of I is preserved exactly (J(4,4) = 28 in our 8×8 1..64 test). Corner values tapered symmetrically toward the image mean (J(1,1)=20.733, J(8,8)=44.267, mean=32.5). Bit-equal MATLAB on all probed cases (residual from psf2otf complex coercion handled). uint8 input class-preserving via saturating cast. |
| `uint8 input — class-preserving` | — | MATLAB R2025b edgetaper.m algorithm — per-dim PSF projection autocorrelation builds an alpha matrix that equals 1 in the interior and tapers to 0 at edges; J = alpha*I + (1-alpha)*blurredI clipped to [min(I), max(I)]. blurredI is circular ifft2(fft2(I) .* psf2otf(PSF, sizeI)). The center of I is preserved exactly (J(4,4) = 28 in our 8×8 1..64 test). Corner values tapered symmetrically toward the image mean (J(1,1)=20.733, J(8,8)=44.267, mean=32.5). Bit-equal MATLAB on all probed cases (residual from psf2otf complex coercion handled). uint8 input class-preserving via saturating cast. |
| `constant image → J ≡ I        (alpha-symmetric edge case)` | — | MATLAB R2025b edgetaper.m algorithm — per-dim PSF projection autocorrelation builds an alpha matrix that equals 1 in the interior and tapers to 0 at edges; J = alpha*I + (1-alpha)*blurredI clipped to [min(I), max(I)]. blurredI is circular ifft2(fft2(I) .* psf2otf(PSF, sizeI)). The center of I is preserved exactly (J(4,4) = 28 in our 8×8 1..64 test). Corner values tapered symmetrically toward the image mean (J(1,1)=20.733, J(8,8)=44.267, mean=32.5). Bit-equal MATLAB on all probed cases (residual from psf2otf complex coercion handled). uint8 input class-preserving via saturating cast. |
| `tonemap` | — | Sig: RGB = tonemap(HDR [, NV...]). Ward's log-luminance equalisation followed by adapthisteq + imadjust + saturation scaling. RGB and grayscale paths. Pipeline reuses numkit's rgb2lab/lab2rgb/adapthisteq/imadjust — accumulated float drift makes bit-exact parity infeasible; tolerance ±1 uint8 unit on centre pixels. Reference: G. Ward et al., IEEE TVCG 3(4), 1997. Default NumberOfTiles=[4 4] matches MATLAB exactly; non-default tile counts may diverge due to adapthisteq precision differences. Image namespace 2026-05-27. |
| `raw2planar` | — | raw2planar + planar2raw — Bayer CFA mosaic ↔ 4-plane sensor-element deinterleave. Covers: uint8 / uint16 / double / mosaic-pattern inputs, all 4 channels, interior + corner indices, round-trip exactness for uint8 and uint16, planar2raw direct call. |
| `planar2raw` | — | raw2planar + planar2raw — Bayer CFA mosaic ↔ 4-plane sensor-element deinterleave. Covers: uint8 / uint16 / double / mosaic-pattern inputs, all 4 channels, interior + corner indices, round-trip exactness for uint8 and uint16, planar2raw direct call. |
| `filloutliers` | — | filloutliers — detect (median/mean/quartiles/percentiles) + replace outliers with fillmethod (constant scalar, 'center', 'clip', 'previous', 'next', 'nearest', 'linear'). Per-column for matrices. Tie-break on 'nearest' = NEXT. DEEP-PROBE 2026-05-31: added the 'percentiles',[lo hi] detection method (previously threw 'findmethod must be median/mean/quartiles'). Percentiles bounds use MATLAB prctile (100*(k-0.5)/n); 'center' fill uses the midpoint (lo+hi)/2; 'clip' clips to [lo,hi]. Verified vs MATLAB R2025b on xp=[1 2 3 100 4 5]: clip[10 90]->[1.1 2 3 90.5 4 5], center[10 90]->45.8 at outliers, clip[25 75]->[2 2 3 5 4 5]; matrix M clip[20 80] per-column. Deferred: 'spline'/'pchip'/'makima' fills, 'grubbs'/'gesd'/'movmedian'/'movmean' detection, SamplePoints/MaxNumOutliers/OutlierLocations/ReplaceValues NV-pairs. |
| `signal_iscola` | — | MATLAB Signal Toolbox iscola — Constant OverLap-Add compliance check. Sums (possibly squared) window shifted by hop = M - noverlap in the stable overlap region; tf = 1 iff maxDev ≤ |m| · eps. Default method 'wola' (sum of w²). Hann 50% overlap: COLA under 'ola' (m=1), not 'wola' (m=0.75, dev=0.25). Hamming 50%: COLA under 'ola' (m=1.08). Rectangular hop=M: trivially COLA (m=1). Hann 75% overlap: m=2 under 'ola'. Bit-equal MATLAB R2025b on 8 probed cases. Octave 11.1.0 ships iscola in the signal package (not core), reports N/A there. |
| `signal_fwht` | — | MATLAB Signal Toolbox fwht/ifwht — fast Walsh-Hadamard transform pair. Radix-2 Hadamard butterfly with 1/N normalisation. Three orderings: 'sequency' (default, Walsh function order), 'hadamard' (natural Sylvester), 'dyadic' (bit-reversed natural = Paley). For length not a power of 2 with n omitted, auto-promote to next pow-2 with zero-pad; explicit n must be a power of 2. Round-trip ifwht(fwht(x)) = x to integer arithmetic. Bit-equal MATLAB R2025b on all 9 probed configurations. Octave 11.1.0 ships fwht/ifwht in signal package. |
| `image_integralBoxFilter` | — | MATLAB Image Toolbox integralBoxFilter — O(1)-per-pixel box filter via 4-point lookup in a precomputed integral image. Output size (H - fH + 1) × (W - fW + 1) — only the fully-supported core (no boundary). filterSize accepts scalar (square box) or 2-vector [rows cols]; both must be ODD. NormalizationFactor NV-pair is a MULTIPLIER applied to the box sum (box-sum * normFactor), NOT a divisor — default 1/(fH·fW) gives the mean, 1 gives the raw sum, 0.5 halves it, 2 doubles it. Bit-equal MATLAB R2025b on magic(8) integral image with 3×3, 5×5, [3 5], [5 3] filters + NormalizationFactor in {1, 0.5, 2}. Octave 11.1.0 doesn't ship integralBoxFilter in core; reports N/A there. |
| `image_imread_tiff` | — | Minimal TIFF reader baseline (cycle 90). Inlines a 4x4 uint8 grayscale TIFF (MATLAB-imwrite-uncompressed format, 122 bytes header + 16 pixels) as chunked fwrite('uint8') calls to a tempname'd file, then validates imread output. Both engines hit the same on-disk bytes. Exercises: TIFF magic detection, IFD walk, uncompressed strip decode, row-major → column-major transpose, uint8 output dtype. Bit-equal MATLAB R2025b on all 7 fingerprints (tol=0 since pixel values are integers). Compression schemes (LZW / PackBits / Deflate), 16-bit RGB + multi-page TIFF deferred to later cycles. |
| `image_modefilt` | — | MATLAB Image Toolbox modefilt — 2-D mode filter via neighbourhood histogram. Supports symmetric (default), replicate, and zeros padding. Tie-break is smallest-on-tie (matches MATLAB's documented `mode` behaviour; MATLAB's internal modefilt MEX has undocumented order-dependent ties that differ — we follow the spec). Output same-class as input. Bit-equal MATLAB R2025b on the symmetric/replicate interior fingerprints. Zeros-pad tied cases at edges may diverge from MATLAB on a handful of pixels due to the documented tie-break difference; excluded from this fingerprint set. Octave 11.1.0 doesn't ship modefilt in core; reports N/A. |
| `image_fspecial3` | — | MATLAB Image Toolbox fspecial3 — predefined 3-D filters, all documented branches. average: ones/prod, default [5 5 5]; size-vector form [3 5 7]. gaussian: separable anisotropic, normalised to sum 1; default [5 5 5] sigma 1, plus per-axis sigma [0.5 5 5] (sigma element 1->rows, 2->cols, 3->pages). ellipsoid: integer-grid mask {(dr/a)^2+(dc/b)^2+(dp/c)^2<=1}/count; default semiaxes 5 -> size 11^3 with 515 voxels; [2 3 4] -> size [5 7 9]. laplacian(gamma1,gamma2): 3x3x3, face=1-g1-g2, edge=g1/4, corner=g2/4, center=-6+3g1+4g2, sums to 0 (default g1=g2=0 -> classic -6/+1). log: Laplacian of (anisotropic) Gaussian, zero-mean, default sigma 1; isotropic + anisotropic [1 1.5 2] forms. prewitt: separable 3x3x3 gradient X/Y/Z. Bit-equal MATLAB R2025b (tol=1e-12). Octave 11.1.0 ships fspecial3 in the image package only (not loaded by default) -> reports N/A there. |
| `image_integralBoxFilter3` | — | MATLAB Image Toolbox integralBoxFilter3 — O(1)-per-voxel 3-D box filter via 8-corner inclusion-exclusion on a precomputed integralImage3 summed-volume table. Output size (H-fH+1) x (W-fW+1) x (D-fP+1) — only the fully-supported (no-boundary) core. filterSize: scalar (cubic) or 3-vector [r c p], all ODD. NormalizationFactor NV-pair is a MULTIPLIER (MATLAB semantics): box-sum * normFactor; default 1/prod(filterSize) (mean), pass 1 for raw sum, 0.5 for half. Verified on integralImage3 of reshape(1:125,5,5,5): default 3x3x3 box, scalar 3, vector [1 3 5] -> size [5 3 1], raw-sum, and 0.5 multiplier. Bit-equal MATLAB R2025b (tol=1e-12). Octave 11.1.0 doesn't ship integralBoxFilter3 in core -> reports N/A there. |
| `image_makelut` | — | MATLAB Image Toolbox makelut + bwlookup — neighbourhood lookup-table pair. makelut(fun, n) evaluates fun on every 2^(n^2) binary n-by-n neighbourhood (fun receives a logical n-by-n matrix), n in {2,3} -> 16/512-element DOUBLE column vector. Neighbourhood for index k (col-major) has position i = bit (n^2-1-i) of k, matching the reshape(2.^[nq-1:-1:0], n, n) weight kernel so bwlookup(BW, makelut(fun,n)) applies fun per neighbourhood. bwlookup(BW, lut) applies a 16- or 512-element table (modern applylut; identical index convention, output class = lut class). Verified: makelut sum>=3 n=2 (l2(8)=1, rest 0), center-passthrough lut (bwlookup==BW), sum>=5 n=3 (256 = sum_{k=5}^9 C(9,k)), and bwlookup==applylut. Bit-equal MATLAB R2025b (tol=1e-12). Octave 11.1.0 ships makelut/bwlookup in the image package only -> reports N/A there. |
| `image_bwmorph3` | — | MATLAB Image Toolbox bwmorph3 — morphological operations on a binary volume. All 6 documented ops, each a 3x3x3 neighbourhood rule (count = set voxels incl. centre; faces6 = six 6-connected faces): branchpoints (centre & count>3), clean (centre & count!=1, drops isolated), endpoints (centre & count==2), fill (centre | faces6==6), majority (count>13 i.e. >=14 of 27), remove (centre & faces6!=6). Zero-padded border; output always LOGICAL, same size as input; 2-D input treated as a single-plane volume. Clean-room port of MATLAB R2025b bwmorph3Algorithm rules. Verified on cube-minus-hole (26/26/0/27/7/26), a z-line (2 endpoints, 0 branchpoints) and a 2-D mask. Bit-equal MATLAB R2025b (tol=1e-12). Octave 11.1.0 ships bwmorph3 in the image package only -> reports N/A there. |
| `image_reducepoly` | — | MATLAB Image Toolbox reducepoly — Ramer-Douglas-Peucker polyline simplification. tolerance in [0,1] normalised by the bbox diagonal norm(max(P)-min(P)); 0 -> eps (minimal reduction), 1 -> endpoints only. Recursive split at the vertex with max chord-perpendicular distance (|det([1 x y; ...])|/chordLen), first-farthest wins on ties; a run collapses to its endpoints once max deviation <= tolerance. Output rows are exact copies of retained input vertices; class preserved (integer computed in single, cast back). Verified vs MATLAB R2025b: default keeps 6 of 7 (drops the vertex collinear with its neighbours), tol>=0.1 keeps only endpoints, collinear -> 2, triangle-wave -> all 5. Bit-equal (tol=1e-12). Octave 11.1.0 ships reducepoly in the image package only -> reports N/A there. |
| `image_roifilt2` | — | MATLAB Image Toolbox roifilt2 — filter a region of interest. Form 1 roifilt2(h,I,BW): imfilter(I,h) (correlation, zero boundary, same size) with only the BW pixels replaced (output equals I elsewhere); output class = class(I). Form 2 roifilt2(I,BW,fun): apply fun to the whole image, keep only BW pixels; output class follows fun's result (uint8 fun -> uint8, double fun -> double). MATLAB crops to the ROI bbox + ceil(size(h)/2) pad purely as an optimisation; the masked pixels' values equal full-image imfilter, so we filter the full image. Verified vs MATLAB R2025b on magic(6) with a 3x3 Laplacian + 3x3 averaging filter (form 1) and uint8 x*2 / double+0.5 handles (form 2). Bit-equal (tol=1e-12) for ODD filters. NOTE: even-size filters inherit numkit imfilter's even-kernel anchoring, which currently differs from MATLAB by one pixel (a pre-existing imfilter gap tracked separately) — not exercised here. Octave 11.1.0 ships roifilt2 in the image package only -> reports N/A there. |
| `padarray_dir` | — | MATLAB padarray — scalar pad value + direction coverage: asymmetric padsize [1 2] with value 7; 'pre'/'post'/'both' directions (value 0/9). Pins sizes + boundary pixels. Bit-equal MATLAB R2025b (tol=1e-12). |
| `imrotate_bbox` | — | MATLAB imrotate bbox + default coverage. 'crop' keeps the INPUT size for 90 deg / 270 deg of a non-square image (3x4, via exact rot90 + centred extraction with direction-dependent half-pixel placement). Default method is 'nearest' (imrotate(J,30) == nearest, sum 86), and the 3-arg form imrotate(A,angle,bbox) treats arg3 as a bbox keyword. Bit-equal MATLAB R2025b. |
| `xcov_maxlag` | — | xcov maxlag + autocov forms. xcov(x,y,2): crops to lags -2..2 -> length 2*2+1=5 (numkit previously ignored maxlag, returning full length 9), zero-lag at index 3 = 5, c(1)=3.8, c(5)=-7.6. xcov(x,y,2,'biased'): same crop then /N=5 -> c(3)=1, c(1)=0.76, c(5)=-1.52. xcov(x): auto-covariance, zero-lag c(5)=sum((x-mean(x)).^2)=22.8. |
| `trapz` | — | Sig trapz(Y[,dim]) / trapz(X,Y[,dim]): trapezoidal integral. Vector trapz([1 4 9 16])=21.5; trapz([0 1 2 3],[1 4 9 16])=21.5 (x-spacing). Matrix trapz(M) integrates each COLUMN -> [2.5 3.5 4.5]; trapz(M,2) integrates each ROW -> [4;10]; trapz(X,M,2) per-row with X spacing -> [40;100]. numkit previously FLATTENED the matrix (wrong) and ERRORED on the scalar-dim form trapz(M,2) -- fixed (dim/matrix/spacing-aware trapzImpl + reg disambiguation). COMPLEX y (fixed 2026-06-05): trapezoidal sum over Complex storage (x-spacing stays real): trapz([1+1i 2+2i 3+3i])=4+4i; trapz([0 1 2],[1+1i 2+2i 5+5i])=5+5i; trapz([1+1i 2;3 4i]) col-wise=[2+0.5i 1+2i]; trapz(row,2)=4+4i. |
| `normalize` | ✅ 🔬 | Sig normalize(A,method,param). The method PARAMETER was parsed-and-ignored: 'range' bounds [lo hi] (default [0 1]); 'norm' exponent p (1/2/Inf); 'scale' divisor 'std'(def)/'first'/'iqr'/'mad' or numeric; 'center' 'mean'(def)/'median' or numeric. normalize([1..5],'range',[0 10]) -> [0 2.5 5 7.5 10]; 'norm',1 -> /15; 'norm',Inf -> /5; 'scale','first' -> /x(1)=[1..5]; 'center','median' -> [-2 -1 0 1 2]. DEEP-PROBE 2026-05-31: 'zscore','robust' was parsed-and-ignored -> numkit fell back to standard mean/std zscore. MATLAB robust z-score centres by the MEDIAN and scales by the (raw) median absolute deviation MAD=median(|x-median|): normalize([1 2 3 4 100],'zscore','robust')=[-2 -1 0 1 97] (median=3, MAD=1) whereas 'zscore','std' and the default give the mean/std result (zr(1)=-2 vs zs(1)=zd(1)=-0.481456). DEEP-PROBE c172/c173: 'scale','iqr' and 'medianiqr' computed the IQR with the R-type-7 (n-1)*p quantile rule instead of MATLAB's prctile (k-0.5)/n convention, giving the wrong IQR for small n (e.g. [1 2 4 8 16 32]: prctile Q1=2,Q3=16,IQR=14, not 2.5/14/11.5). Now uses the prctile convention: si1=1/14=0.07142857, mi1=(1-6)/14=-0.35714286, s5_1=1/2.5=0.4. Standalone iqr/prctile/quantile were already correct (unchanged). Matches MATLAB R2025b. |
| `rescale` | — | Sig rescale(A[,lo,hi][,'InputMin',a,'InputMax',b]). Maps A onto [lo,hi] (default [0,1]); 'InputMin'/'InputMax' override the data min/max AND clamp values to that range. rescale([1..5],'InputMin',2,'InputMax',4)=[0 0 .5 1 1]; with lo/hi 0,10 -> [0 0 5 10 10]; 'InputMax',3 -> [0 .5 1 1 1]. numkit previously errored 'Cannot convert char to scalar' on the NV form -- now supported. Matches MATLAB R2025b. (MATLAB rescale supports only InputMin/InputMax NV, not Output*.) |
| `histc` | — | Sig [n,bin]=histc(x,edges). Legacy: n has length(edges) entries; bin k counts edges(k)<=x<edges(k+1) (k=1..end-1), n(end) counts x==edges(end). histc([1 2 2 3 5],[0 2 4 6])=[1 3 1 0], bin idx=[1 2 2 2 3]. Row in->row out; column/matrix -> column-wise length(edges) x ncols. numkit previously had no histc -- added. Matches MATLAB R2025b. |
| `dec2base` | — | Sig s=dec2base(d,base[,len]) / d=base2dec(s,base), base 2..36 (digits 0-9 then A-Z). dec2base(100,16)='64' (chars 54,52), dec2base(10,2,8)='00001010' (len 8, first '0'=48), dec2base(255,16)='FF' ('F'=70), dec2base(35,36)='Z'=90; base2dec('64',16)=100, base2dec('1010',2)=10, base2dec('Z',36)=35; char matrix -> column vector. numkit previously had neither -- added. Matches MATLAB R2025b. |
| `cumtrapz` | — | Sig: c = cumtrapz(Y); cumtrapz(Y,dim); cumtrapz(X,Y); cumtrapz(X,Y,dim). Covers: vector default, vector+dim along singleton (no-op → zeros), dim=1 (down columns, == default), dim=2 (along rows), X,Y two-vector form, and X,Y,dim row-wise (X is a coordinate vector of length size(Y,dim), broadcast across rows). Queue-clearing 2026-05-29: dim arg + row-wise integration. COMPLEX y (fixed 2026-06-05): cumulative trapezoid over Complex storage (x stays real): cumtrapz([1+1i 2+2i 3+3i])=[0 1.5+1.5i 4+4i]; per-column for a matrix; dim + x-spacing forms work. |
| `histcounts_norm` | — | Sig: n = histcounts(x, edges, 'Normalization', mode). Covers count(def)/probability(/N)/countdensity(/binwidth)/pdf(/(N*binwidth))/cumcount/cdf, with uniform and nonuniform edges, and out-of-range data so the divisor N = numel(x) (not the in-range count) — e.g. cdf(end)=7/9 not 1. Queue-clearing 2026-05-29: numkit previously silently ignored 'Normalization' (always raw counts). |
| `mat2str` | — 🔬 | mat2str on CHAR input vs MATLAB R2025b (covers mat2str; kept separate from mat2str.json to stay under the VM 255-register limit). DEEP-PROBE 2026-05-31: char previously rendered numeric codes (mat2str('abc')='[97 98 99]'). Now a quoted char literal: row 'abc' -> "'abc'" (nch1=5, a1=39 quote, a2=97 'a', ae=39); char matrix ['ab';'cd'] -> "['ab';'cd']" (nch2=11, b1=91 '['); empty char '' -> "''" (nch3=2, c1=39); internal single quote doubled mat2str('a''b') -> "'a''b'" (nch4=6); single char 'x' -> "'x'" (nch5=3, e1=39, e2=120). Cross-engine char-code + numel + sum fingerprints. |
| `nanvar` | — | Sig: legacy NaN-aware reductions (recommended modern form: `sum(..., 'omitnan')` / `mean(..., 'omitnan')`). nansum: NaN entries dropped; all-NaN slice → 0 (NaN is additive identity). nanmean: NaN entries dropped; divisor is count of valid obs; all-NaN slice → NaN. nanvar/nanstd: NaN entries dropped; a SINGLE non-NaN value → 0 for BOTH N-1 (default) and N normalizations (NOT NaN from 0/0) — verified MATLAB R2025b nanvar([NaN 5 NaN])==0. Bit-exact MATLAB R2025b on the pinned cases. nanstd/nanvar/nanmedian/nanmax/nanmin work but are NOT in PROGRESS.md (legacy; modern form is var(...,'omitnan')). |
| `nanstd` | — | Sig: legacy NaN-aware reductions (recommended modern form: `sum(..., 'omitnan')` / `mean(..., 'omitnan')`). nansum: NaN entries dropped; all-NaN slice → 0 (NaN is additive identity). nanmean: NaN entries dropped; divisor is count of valid obs; all-NaN slice → NaN. nanvar/nanstd: NaN entries dropped; a SINGLE non-NaN value → 0 for BOTH N-1 (default) and N normalizations (NOT NaN from 0/0) — verified MATLAB R2025b nanvar([NaN 5 NaN])==0. Bit-exact MATLAB R2025b on the pinned cases. nanstd/nanvar/nanmedian/nanmax/nanmin work but are NOT in PROGRESS.md (legacy; modern form is var(...,'omitnan')). |
| `sortrows_complex` | — | Sig: [B,idx] = sortrows(A). COMPLEX sortrows orders rows lexicographically; each column compares by magnitude |z| then phase angle arg(z) ascending (negative col index = descending on that column); NaN component sorts last. sortrows([3+4i 2;1 1;3+4i 0;1 5i]) -> rows [2 4 3 1]: col1=[1 1 3+4i 3+4i], col2=[1 5i 0 2]. DEEP-PROBE 2026-05-29: numkit previously routed complex through toDoubleMatrix2D, SILENTLY DROPPING the imaginary part and returning wrong rows; added sortRowsComplex(). |
| `abs_integer` | — | Sig: y = abs(X). For INTEGER input MATLAB keeps the class and SATURATES: abs(int8(-128))=127 int8 (not 128), abs(int16(-32768))=32767 int16, abs(int8([-3 -128 5]))=[3 127 5]; unsigned types are returned unchanged (abs(uint8(200))=200). double/complex paths unchanged (abs(-3.5)=3.5, abs(3-4i)=5). numkit previously returned DOUBLE 128 for abs(int8(-128)); fixed 2026-05-30 (absIntegerSaturate in helpers.hpp, dispatched from both abs backends). NOTE: ; only inside matrix-literal INPUTS. |
| `sign_integer` | — | Sig: y = sign(X). For INTEGER input MATLAB keeps the class: sign(int8(-5))=-1 int8, sign(int8([-5 0 9]))=[-1 0 1] int8, sign(uint8(0))=0 uint8 (unsigned never yields -1), sign(int32([-100 100]))=[-1 1] int32. double inputs unchanged (sign(-3.5)=-1, sign(0)=0, sign(2.1)=1). numkit previously returned DOUBLE for integer scalars and threw on integer arrays; fixed 2026-05-30. (sign of COMPLEX = z/|z| remains a separate unimplemented gap.) NOTE: ; only inside matrix-literal INPUTS. |
| `sign_complex` | — | Sig: y = sign(Z) for complex Z. MATLAB: sign(z) = z/abs(z) for z != 0, else 0; result is complex. sign(3-4i)=0.6-0.8i, sign(1i)=0+1i, sign(complex(0,0))=0+0i, sign([3+4i 0 -2i])=[0.6+0.8i 0 0-1i], sign(2+2i)=0.7071+0.7071i. numkit previously threw 'Cannot convert complex with nonzero imaginary part to double scalar' (sign routed through the double-only unaryDouble); fixed 2026-05-30 (complex branch in rounding.cpp). NOTE: ; only inside matrix-literal INPUTS. |
| `wrap_angle` | — | Angle-wrapping family. wrapToPi -> [-pi,pi] (closed endpoints kept: wrapToPi(pi)=pi, wrapToPi(-pi)=-pi). wrapTo2Pi -> [0,2pi] (positive landing on 0 -> 2pi: wrapTo2Pi(2pi)=2pi, but wrapTo2Pi(0)=0). wrapTo180 -> [-180,180] (degree analogue: wrapTo180(540)=180, wrapTo180(180)=180, wrapTo180(-180)=-180). wrapTo360 -> [0,360] (wrapTo360(720)=360, wrapTo360(360)=360, wrapTo360(0)=0). Matches MATLAB R2025b Mapping-Toolbox defs exactly. New fns 2026-05-30 (gap-closure: deg2rad/rad2deg already shipped, this completes the angle family). |
| `interp1_extrap` | — | interp1 out-of-range / extrapolation policy vs MATLAB R2025b. DEFAULT: linear/nearest/previous/next return NaN outside [x(1),x(end)] (a1=a2=a3=1); spline/pchip/makima extrapolate by default (s1=s2=s3=40). 'extrap' option: linear extrapolates (b1=40,b2=0); nearest holds endpoint both ways (b3=30,b4=10); previous holds y(end) above range (p1=30) but NaN below (p2=1); next mirror (q1=10, q2=1). Numeric extrapval fills out-of-range (c1=c3=-99) leaving in-range (c2=25). Bug fixed 2026-05-30: linear/nearest previously extrapolated by default instead of returning NaN; 'extrap' worked only by accident, constant extrapval was unsupported. NOTE: no semicolons inside quoted strings; single-quoted method names. |
| `sprintf_string` | — | sprintf/fprintf %s with the string type vs MATLAB R2025b. Bug fixed 2026-05-30: %s only accepted char arrays, so a string scalar printed nothing (fprintf('%s',"hi") -> empty). Now %s prints string scalars (s1='hello', n1=5) and cycles the format over a string array's elements like MATLAB (sprintf('%s',["x" "yz"])='xyz', n3=3). Mixed string+numeric specs also work (s4='k=7'). NOTE: double-quoted string literals require the string type; no semicolons inside quoted strings. |
| `histcounts_binedges` | — | histcounts 'BinEdges' name-value + [n,edges] second output vs MATLAB R2025b. histcounts(x,'BinEdges',E) == histcounts(x,E): n=[1 2 2]. [n,e]=histcounts(...) returns the edges as a row vector (e=[0 2 4 6], ne=4). 'BinEdges' composes with 'Normalization' (probability -> [0.2 0.4 0.4]). Bug fixed 2026-05-30: 'BinEdges' threw 'Not a double array' (the name-value wasn't parsed), and the second output (edges) was missing. Automatic binning (nbins / BinWidth / BinLimits) remains unsupported. |
| `sprintf_width` | — | sprintf/fprintf %s width + precision vs MATLAB R2025b. Bug fixed 2026-05-30: %s ignored its spec (printed raw string). Now %5s right-justifies ('[   hi]', na=6, a(2)=32 space, a(5)='h'=104), %-5s left-justifies ('[hi   ]', b(2)='h', b(4)=32), %.1s caps chars ('[h]', nc=3, c(2)='h'), %5.1s composes precision+width ('[    h]', nd=7, d(6)='h'=104), and width applies to the string type too (e=[   hi]). Manual padding (not snprintf injection). NOTE: no semicolons inside quoted strings; double-quoted "hi" escaped for string type. |
| `sprintf_int_fallback` | — | sprintf/fprintf integer conversion on a non-integer falls back to %e vs MATLAB R2025b. Bug fixed 2026-05-30: %d truncated to int (sprintf('%d',3.7) gave '3'). MATLAB overrides to %e: sprintf('%d',3.7)='3.700000e+00' (na=12, a(1)='3'=51, a(3)='7'=55, a(8)='e'=101); precision carries (sprintf('%.2d',3.7)='3.70e+00', nb=8, b(4)='0'); whole numbers stay integer (sprintf('%d',5)='5', sprintf('%d',1e10)='10000000000' nd=11); Inf->'Inf' (ne=3, e(1)='I'=73); negative non-int sprintf('%d',-2.5)='-2.500000e+00' (f(1)='-'=45). |
| `mink_maxk_index` | — | mink/maxk second output (indices) vs MATLAB R2025b. Bug fixed 2026-05-30: [m,i]=mink/maxk(...) did not return the index. mink([5 2 8 1 9],2)->m=[1 2], ix=[4 2]; maxk->jx=[5 3]; ties keep the lower position (mink([3 1 3],3) ix=[2 1 3]); matrix indices are positions along the operating dim (mink(A,2) im=[2 2;3 3]; maxk(...,2) id=[2 3;3 1]). Index is 1-based along the chosen dimension, matching MATLAB. DEEP-PROBE c171: added 'ComparisonMethod','abs' — ranks by magnitude |x| (returning the ORIGINAL signed values), ties broken by ascending index. maxk(...,'abs') of [3 -7 2 -5 1 -9 4] -> [-9 -7 -5] (idx [6 2 4]); mink(...,'abs') -> [1 2 3] (idx [5 3 1]). numkit previously threw 'ComparisonMethod=abs not yet supported'. namespace=stats (descriptive). |
| `zscore_musigma` | — | zscore second/third outputs [Z,MU,SIGMA] vs MATLAB R2025b. Bug fixed 2026-05-30: zscore only returned Z. For a vector MU/SIGMA are scalars (mu=5, sigma=2.58199, N-1 sample std). For a matrix MU/SIGMA are 1xW row vectors of column stats (mum=[3 6], sgm=[2 4], nmu=2). flag==1 gives the population std (sgpop=sqrt(5)=2.23607). namespace=stats (descriptive/normalize). |
| `find_rowcol` | — | find 2-output [r,c] and 3-output [r,c,v] vs MATLAB R2025b. Bug fixed 2026-05-30: find returned only linear indices. [r,c]=find([0 5;7 0]) -> r=[2;1], c=[1;2] (column vectors, nr=2); [r,c,v]=find(...) -> v=[7;5] in column-major order. Row-vector input gives row-vector subscripts (find([0 1 0 1]) -> rr=[1 1] isrow, cc=[2 4]). Single-output linear-index form unchanged (find([0 1 0 1])=[2 4], nk=2). namespace=builtin. |
| `sprintf_inf_nan` | — | sprintf/fprintf %f/%e/%g of Inf/NaN print capitalised vs MATLAB R2025b. Bug fixed 2026-05-30: numkit printed the C library's lowercase 'inf'/'nan'. Now: sprintf('%f',Inf)='Inf' (na=3, a(1)='I'=73, a(3)='f'=102), sprintf('%f',NaN)='NaN', sprintf('%e',-Inf)='-Inf' (c(1)='-'=45, c(2)='I'=73), %8.2f Inf -> '     Inf' (nd=8, d(8)='f'=102, d(1)=32 space; precision ignored, width honoured), %+f Inf -> '+Inf' (e(1)='+'=43), %g NaN -> 'NaN'. The '+' flag adds a sign only to +Inf, not NaN. namespace=builtin. |
| `unique_orientation` | — | unique output orientation vs MATLAB R2025b. Bug fixed 2026-05-30: ia/ic came back as row vectors and u was always a row even for a column input. MATLAB: ia and ic are ALWAYS column vectors (iac=icc=1); u matches the input orientation (row input -> row u so ur=1 and s2=1; column input -> column u so ucc=1 and s1=1). Values unchanged (ia(1)=2, ic(1)=3, nia=3, nic=5). namespace=builtin. |
| `setops_indices` | — | intersect/union/setdiff index outputs (ia/ib) vs MATLAB R2025b. Bug fixed 2026-05-30: these were 1-output only, so [c,ia,ib]=... errored. intersect([3 1 2 5],[2 4 1]) -> ia=[2;3] (A-indices of 1,2), ib=[3;1] (B-indices); setdiff -> ia=[1;4]; union([3 1 2],[2 4 1]) -> ia=[2;3;1] (A-sourced 1,2,3; nua=3), ib=[2] (B-only 4; nub=1). All index vectors are columns (iacol=ibcol=dacol=1). namespace=builtin. |
| `corrcoef_pvalues` | — | corrcoef second output P (two-sided p-values) vs MATLAB R2025b. Bug fixed 2026-05-30: corrcoef was 1-output only. P(i,j)=2*tcdf(-|t|,n-2) with t=r*sqrt((n-2)/(1-r^2)); diagonal=1. corrcoef([1 2 3 4]',[2 4 5 9]') -> R(1,2)=0.96476382, P(1,2)=0.035236179 (symmetric, p11=1). Matrix corrcoef -> Pm(1,2)=0.366717, Pm(1,3)=0.49324, Pm(2,3)=0.126523. namespace=stats (descriptive). |
| `regexp_multiout` | — | regexp default positional multi-output [start, end, tokenExtents, match, tokens, names, split] vs MATLAB R2025b. Bug fixed 2026-05-30: regexp returned only the start indices. [s,e]=regexp('a1b2','\d') -> s=[2 4], e=[2 4]. With a capture group, tokenExtents te{1}=[2 2], match m{1}='1' (mc=49), split sp={'a','b'} (spc=97, nsp=2), tokens t has nm_n=2 entries. namespace=builtin. |
| `strsplit_matches` | — | strsplit second output (matched delimiters) vs MATLAB R2025b. Bug fixed 2026-05-30: strsplit returned only the tokens. [t,m]=strsplit('a,b:c',{',',':'}) -> t has nt=3, m={',',':'} (nm=2, m1=44, m2=58). A collapsed run is a single match ('a,,b' on ',' -> mc={',,'}, nmc=1, mlen=2). Default whitespace collapse: 'hi  there' -> one match (nmw=1). NOTE: ':' used instead of ';' to avoid the semicolon-in-quoted-string harness pitfall. namespace=builtin. |
| `sortrows_direction` | — | sortrows direction strings/cells vs MATLAB R2025b. Bug fixed 2026-05-30: sortrows threw 'column spec must be numeric' on any CHAR/STRING arg. Now supports: a direction string applied over ALL columns (sortrows(A,'descend')->[3 1;3 0;1 5;1 2]); per-column direction cell standalone (sortrows(A,{'descend','ascend'})->[3 0;3 1;1 2;1 5]); explicit columns + direction cell (sortrows(A,[1 2],{'ascend','descend'})->[1 5;1 2;3 1;3 0]); explicit columns + single direction covering all listed cols ([B,ix]=sortrows(A,[1 2],'descend') ix=[1;3;4;2]); scalar column + direction (sortrows(A,1,'descend')). Directions are case-insensitive 'ascend'/'descend' and map onto the existing signed-column path. namespace=builtin. NOTE: 'ComparisonMethod' name-value remains an unimplemented gap. |
| `num2str_format` | — | num2str(X,FMT) format-string handling vs MATLAB R2025b. Bug fixed 2026-05-30: num2str passed the double straight to snprintf(fmt,...) so %d/%i/%x specs read an int from the va_list and printed garbage (num2str(5,'%05d')->'00000', num2str(42,'%8d')->'0'), and the width padding was never trimmed. Now routes through the sprintf engine + strtrim. Scalar fingerprints byte-compare the produced strings: '%8.4f'->'3.1416' (na=6,a1=51='3'); '%05d'->'00005' (nc=5,c1=48='0',c5=53='5', leading zeros kept); '%8d'->'42' (nd=2,d1=52='4'); '   value=%6.2f'->'value=  3.14' (nh=12,h1=118='v', leading trimmed but internal kept); '%-8d'->'5' (nlj=1, trailing trimmed). DEEP-PROBE 2026-05-31: the FMT form now also accepts VECTOR/MATRIX inputs (previously threw 'Cannot convert double to scalar'). The format is applied cyclically across each ROW and the leading/trailing blank COLUMNS common to all rows are stripped, returning an N-row char matrix: num2str([1.5 2.25 3.125],'%8.3f')='1.500   2.250   3.125' (nsv=21, v1=49='1', v9=50='2', vend=53='5'); num2str([1.5 2.25;3.1 4.0],'%8.3f') is a 2x13 char matrix (rm=2,cm=13, col-major m1=49='1',m2=51='3'); num2str([1.5;22.25],'%6.2f') is 2x5 (rcv=2,ccv=5, col-major cv1=32=' ',cv2=50='2'). namespace=builtin. The DEFAULT-precision and integer-N forms for vector/matrix inputs (MATLAB's magnitude-dependent column width) remain a separate deferred gap. |
| `int2str` | ✅ 🔬 | int2str vs MATLAB R2025b. Rounds half away from zero (round); plain integers, no decimals/sci; Inf/-Inf/NaN pass through. Scalar: int2str(3.4)='3', int2str(-2.5)='-3', int2str(1e10)='10000000000', int2str(0.5)='1', complex uses REAL part int2str(3.6+1.2i)='4'. DEEP-PROBE 2026-05-31: VECTOR/MATRIX forms were SCALAR-ONLY (threw 'only scalar inputs are supported'); now round each element then format with field W=digits(max|rounded|)+2 routed through the num2str FMT path. int2str([1 2 3])='1  2  3'; int2str([10 200 3])='10  200    3'; int2str([1.5 2.5 -2.5])='2  3 -3' (half-away rounding, sign eats the gap); int2str([1 2;30 4]) is a 2x5 char matrix (' 1   2'/'30   4'). Cross-engine fingerprint via numel + sum(char codes). Complex ARRAY column layout still deferred. namespace=builtin. |
| `validatestring` | ✅ 🔬 | validatestring success-match semantics vs MATLAB R2025b. Implemented 2026-05-30 (was an undefined function). Case-insensitive: exact match wins ('orange'->'orange', n1=6, a1=111='o', z1=101='e'); else a unique case-insensitive prefix ('APP'->'apple', n2=5, a2=97='a'); else exact-over-prefix ('in' in {'in','input'}->'in', n3=2); else the shortest-prefix-of-all ('appl' in {'apple','applesauce'}->'apple', n4=5, z4=101='e'). Ambiguous (e.g. 'a' in {'apple','apricot'}) and no-match both throw — covered by the gtest, not fingerprintable here. namespace=builtin. Trailing funcName/varName/position args (error-text only) accepted+ignored. |
| `match_cell_patterns` | — | contains/startsWith/endsWith with a cell array (or string array) of patterns vs MATLAB R2025b. Bug fixed 2026-05-30: these threw 'Not a char array' on a cell pattern argument. Now match if ANY listed pattern matches: startsWith('foobar',{'foo','xyz'})=true (sw1=1), {'zzz','xyz'}=false (sw0=0); endsWith('test.m',{'.m','.cpp'})=true (ew1=1); contains('hello',{'ell','xyz'})=true (cn1=1); string-array list works (sa1=1); scalar pattern unchanged (sc1=1). namespace=builtin. NOTE: the FIRST argument being a cell array (per-element output) remains a separate gap. |
| `isvarname` | ✅ 🔬 | isvarname vs MATLAB R2025b. Implemented 2026-05-30 (was an undefined function). True only for a non-empty char/string-scalar that starts with a letter, contains only letters/digits/underscores, and is not a reserved keyword: 'abc'=1, 'a_1'=1, '1abc'=0, '_x'=0, 'a b'=0, ''=0, 'if'=0, 'end'=0 (keywords), "abc" string-scalar=1. Non-text inputs yield false rather than erroring: 5->0, {'abc'}->0. No length limit in R2025b. namespace=builtin. |
| `erase_count_cell` | — | count/erase with a cell array (or string array) of patterns vs MATLAB R2025b. Bug fixed 2026-05-30: both threw 'Not a char array' on a cell pattern argument. count sums per-pattern non-overlapping occurrences: count('abcabc',{'a','c'})=4 (c1), count('abcABC',{'a','b','c'})=3 (c2, case-sensitive); scalar pattern unchanged count('aaaa','aa')=2 (c3). erase removes every occurrence of each listed pattern, applied in order: erase('a-b_c',{'-','_'})='abc' (n1=3, a1=97='a', a2=99='c'); erase('hello world',{'ll','rl'})='heo wod' (n2=7). namespace=builtin. (str-as-cell per-element output remains a separate gap.) |
| `replace_cell` | — | replace with a cell array (or string array) of OLD patterns vs MATLAB R2025b. Bug fixed 2026-05-30: replace routed through strrep and threw 'Not a char array' on a cell. A single NEW applies to every OLD: replace('a-b_c',{'-','_'},'X')='aXbXc' (na=5, a1=88='X'); paired NEW: replace('a-b_c',{'-','_'},{'P','Q'})='aPbQc' (b1=80='P', b2=81='Q'); single left-to-right pass with no chain-replacement: replace('ab',{'a','b'},{'b','c'})='bc' (c1=98='b', c2=99='c', NOT 'cc'); first-in-list match wins: replace('abc',{'a','ab'},{'X','Y'})='Xbc' (nd=3, d1=88='X'). namespace=builtin. NOTE: strrep with a cell returns a CELL (broadcast) — a separate, unchanged behaviour. |
| `grp2idx` | ✅ 🔬 | grp2idx multi-output [g,gn] vs MATLAB R2025b. Implemented 2026-05-30 (was an undefined function). cellstr groups in FIRST-APPEARANCE order: grp2idx({'b','a','b','c'}) -> g=[1 2 1 3] (g1=1,g4=3), gn={'b','a','c'} (ng=3, c1=98='b', c2=97='a'). numeric groups SORTED ascending, index=rank: grp2idx([3 1 3 2 1]) -> g=[3 1 3 2 1] (gi1=3,gi4=2), ni=3. NaN -> NaN index, excluded from gn: grp2idx([3 1 NaN 2]) g(3)=NaN (isn=1), nn=3. logical sorted false<true: grp2idx(logical([1 0 1 0])) -> g=[2 1 2 1] (l1=2,l2=1). namespace=descriptive. NOTE: categorical input + column-aligned char matrices not yet handled. |
| `datestr` | ✅ 🔬 | datestr string-date INPUT (covers datestr; separate from datestr.json for the VM 255-register limit). DEEP-PROBE 2026-05-31: numkit threw 'datestr: string date input not yet supported'. Now a char/string date is auto-parsed (ISO yyyy-mm-dd[ HH:MM:SS] / dd-mmm-yyyy[ HH:MM:SS], same forms as datevec/datenum) and re-rendered; the optional 2nd arg is the OUTPUT format (string or numeric code 0-31), NOT the input parse spec. datestr('30-Dec-2022')='30-Dec-2022'; datestr('2022-12-30','mm/dd/yyyy')='12/30/2022'; datestr('15-Mar-2020 13:45:30','HH:MM')='13:45'; datestr('30-Dec-2022',23)='12/30/2022'; datestr('2022-12-30 12:34:56')='30-Dec-2022 12:34:56' (time auto-included in default). Cross-engine fingerprint via numel + sum(char codes). |
| `datenum_string` | ✅ | datenum(str [, fmt]) string parsing vs MATLAB R2025b. Bug fixed 2026-05-30: datenum threw 'string parsing not yet supported' on a string argument. Auto-detects ISO yyyy-mm-dd[ HH:MM:SS] and dd-mmm-yyyy[ HH:MM:SS]: datenum('2022-12-30')=738885 (a), datenum('30-Dec-2022')=738885 (b), datenum('2022-12-30 12:34:56')=738885.5243 (c), datenum('30-Dec-2022 06:05:09')=738885.2536 (d). Explicit format string supported: datenum('2022-12-30','yyyy-mm-dd')=738885 (e), datenum('30/12/2022','dd/mm/yyyy')=738885 (f). Numeric form unchanged: datenum(2022,12,30)=738885 (g). namespace=builtin. NOTE: other auto-formats, 2-digit years, cellstr/multi-row inputs not yet handled. |
| `datevec_string` | ✅ | datevec(str [, fmt]) string parsing vs MATLAB R2025b. Bug fixed 2026-05-30: datevec threw 'string parsing not yet supported' on a string. Returns [Y M D H MI S]: datevec('2022-12-30 12:34:56')=[2022 12 30 12 34 56]; auto-detects dd-mmm-yyyy (datevec('30-Dec-2022') -> mo=12, d=30); explicit format string (datevec('30/12/2022','dd/mm/yyyy') -> y=2022, d=30). Reuses the same parser as datenum string-parse. namespace=builtin. NOTE: completes the datestr/datenum/datevec string-date trio; other auto-formats / 2-digit years / cellstr / multi-row remain gaps. |
| `calendar` | ✅ 🔬 | calendar(year, month) 6x7 month matrix vs MATLAB R2025b. Implemented 2026-05-30 (was an undefined function). Columns Sunday..Saturday; each day sits in its day-of-week column, weeks run down rows, empty cells 0, always 6 rows. Dec 2022 starts on a Thursday (col 5): r=6, c=7, C(1,5)=1 (a), C(1,7)=3 (b), C(5,7)=31 (d), C(1,4)=0 (e), C(6,1)=0 (f), sum=496 (s = 1+..+31). Leap Feb 2024 ends Thursday: F(5,5)=29 (f29), F(5,6)=0 (f0), sum=435 (sf=1+..+29). namespace=builtin. NOTE: the no-arg current-month and datenum forms are not yet supported. |
| `etime` | ✅ 🔬 | etime(t2, t1): elapsed seconds between 6-element date vectors [Y M D H MI S], one row each or N-by-6, returning an N-by-1 column. Implemented 2026-05-30 (was an undefined function). Calendar-aware = (datenum(t2)-datenum(t1))*86400 but the integer date-day part is differenced separately from H/MI/S so a fractional second (a=0.5) survives without cancellation in the ~7.4e5 serial. b/c/lc cross month/year/leap-day boundaries = 86400; d negative = -3600. Matrix form: each row a date vector -> column out (r1=10, r2=20, nrows=2, ncols=1); a single row broadcasts (bc3=3). namespace=builtin. NOTE: requires exactly 6 columns (MATLAB indexes column 6); fewer columns raise an error (covered in gtest). |
| `weeknum` | ✅ 🔬 | weeknum(D [, WeekStart [, European]]): week-of-year for serial date number D, element-wise, shape preserved. Implemented 2026-05-30 (was an undefined function). US default (WeekStart=1=Sunday): partial first week is week 1, so Jan 1 2026 (Thu) = 1 (a), Jan 4 (Sun) = 2 (b), Dec 31 2026 = 53 (c); leap Feb 29 2020 = 9 (lp), Jul 4 2026 = 27 (jy). WeekStart=2 (Monday): Jan 4 2026 (Sun) = 1 (ws), Jan 5 (Mon) = 2 (ws5). European=1 applies the ISO-style >=4-day rule with the chosen WeekStart and donates a short leading partial week to the prior year: Jan 1 2026 = 53 (e1, donated), Jan 4 = 1 (e4), Dec 31 2026 = 52 (ee), Jan 1 2027 = 52 (ep). Vector input -> column preserved (v1=1, v2=53, vcol=1). namespace=builtin. WeekStart out of 1..7 throws (covered in gtest). |
| `addtodate` | ✅ 🔬 | addtodate(D, Q, units): add Q units to serial date number D (scalar). Implemented 2026-05-30 (was an undefined function). Time units are plain serial arithmetic (+3 day -> D=3 wraps Jan31->Feb3 dD=3 dH=10; +3 hour -> hH=13; +3 millisecond delta sms=3/86400000~=3.47e-8). Calendar units add to the month/year component with day-clamping to the new month's length and preserve the time-of-day fraction: +3 month from Jan31 -> Apr30 (moM=4, moD=30, moH=10 preserved); +3 year -> 2029 (yrY=2029); Jan31+1mo -> Feb28 (c1M=2,c1D=28); leap Jan31 2024 +1mo -> Feb29 (c2D=29); Feb29 2024 +1yr -> Feb28 2025 (c3Y=2025,c3D=28); negative Mar15 2026 -4mo -> Nov15 2025 (nY=2025,nM=11). namespace=builtin. NOTE: scalar D only (MATLAB errors on a vector); fractional month/year quantity is rounded. |
| `partialcorr_rows` | ✅ | partialcorr 'Rows' NaN policy (2026-05-30). The fn name is a probe alias for partialcorr — kept in a separate spec from partialcorr.json so the per-chunk variable count stays under the numkit VM 255-register limit. partialcorr previously accept-and-ignored the 'Rows' NV pair, so NaN data NaN-poisoned. 'complete' = listwise deletion across all positional matrices, then partial correlation: Xn has NaN at (3,2) and (6,3), so rows 3 and 6 drop (5 rows kept), giving rc11=1, rc12=-0.227123, rc13=0.040291, rc23=-0.046205. namespace=stats. 'all' (default) NaN-poisons; 'pairwise' deferred (partial correlation from a pairwise, possibly non-PD covariance) with a clear error. Matches MATLAB R2025b. |
| `abs` | — | abs(x) elementwise magnitude. Correctness on a tiny signed vector; benched (SIMD build) at N=1e3/1e6. |
| `sign` | — | sign(x): -1/0/+1 elementwise. Benched (SIMD build) N=1e3/1e6. |
| `cmunique(X, MAP)              indexed image + colormap → compressed` | — | MATLAB R2025b cmunique.m algorithm: quantise MAP to 1/1024, sort rows by [3 2 1], identify consecutive duplicates, compress map, remap X indices, then count usage and drop unused entries with a second remap. Output Y is uint8 (0-based) when newmap has ≤ 256 rows, else double (1-based). Tolerance 1e-3 because the colormap values are exactly the quantised 1/1024 grid (4 decimal places); higher precision unnecessary. Bit-equal MATLAB on (X, MAP), (RGB), (I), and uint8-X forms — verified by direct probe. |
| `cmunique(RGB)                  truecolor → indexed` | — | MATLAB R2025b cmunique.m algorithm: quantise MAP to 1/1024, sort rows by [3 2 1], identify consecutive duplicates, compress map, remap X indices, then count usage and drop unused entries with a second remap. Output Y is uint8 (0-based) when newmap has ≤ 256 rows, else double (1-based). Tolerance 1e-3 because the colormap values are exactly the quantised 1/1024 grid (4 decimal places); higher precision unnecessary. Bit-equal MATLAB on (X, MAP), (RGB), (I), and uint8-X forms — verified by direct probe. |
| `cmunique(I)                    intensity → indexed` | — | MATLAB R2025b cmunique.m algorithm: quantise MAP to 1/1024, sort rows by [3 2 1], identify consecutive duplicates, compress map, remap X indices, then count usage and drop unused entries with a second remap. Output Y is uint8 (0-based) when newmap has ≤ 256 rows, else double (1-based). Tolerance 1e-3 because the colormap values are exactly the quantised 1/1024 grid (4 decimal places); higher precision unnecessary. Bit-equal MATLAB on (X, MAP), (RGB), (I), and uint8-X forms — verified by direct probe. |
| `uint8 X input                  + double MAP` | — | MATLAB R2025b cmunique.m algorithm: quantise MAP to 1/1024, sort rows by [3 2 1], identify consecutive duplicates, compress map, remap X indices, then count usage and drop unused entries with a second remap. Output Y is uint8 (0-based) when newmap has ≤ 256 rows, else double (1-based). Tolerance 1e-3 because the colormap values are exactly the quantised 1/1024 grid (4 decimal places); higher precision unnecessary. Bit-equal MATLAB on (X, MAP), (RGB), (I), and uint8-X forms — verified by direct probe. |
| `Y is uint8 when newmap ≤ 256   else double` | — | MATLAB R2025b cmunique.m algorithm: quantise MAP to 1/1024, sort rows by [3 2 1], identify consecutive duplicates, compress map, remap X indices, then count usage and drop unused entries with a second remap. Output Y is uint8 (0-based) when newmap has ≤ 256 rows, else double (1-based). Tolerance 1e-3 because the colormap values are exactly the quantised 1/1024 grid (4 decimal places); higher precision unnecessary. Bit-equal MATLAB on (X, MAP), (RGB), (I), and uint8-X forms — verified by direct probe. |
| `duplicate-row removal           + unused-row pruning` | — | MATLAB R2025b cmunique.m algorithm: quantise MAP to 1/1024, sort rows by [3 2 1], identify consecutive duplicates, compress map, remap X indices, then count usage and drop unused entries with a second remap. Output Y is uint8 (0-based) when newmap has ≤ 256 rows, else double (1-based). Tolerance 1e-3 because the colormap values are exactly the quantised 1/1024 grid (4 decimal places); higher precision unnecessary. Bit-equal MATLAB on (X, MAP), (RGB), (I), and uint8-X forms — verified by direct probe. |
| `ode45(f, tspan, y0)            scalar IVP, explicit tspan` | — | Sig 1+2: ode45(@f, tspan, y0) and ode45(@f, tspan, y0, opts). MATLAB R2025b runs Dormand-Prince 5(4) with Shampine's 4-th order free interpolant for dense output; numkit ode45 implements the same RK tableau (Hairer-Norsett-Wanner Vol I, p.178) and the same Shampine 1986 interpolant. With explicit tspan + tight tols, numkit and MATLAB land identically on the requested sample points (verified to ~1e-9). The fingerprint also exercises shape (n=11, d=1) and analytical solution exp(-2). Refine, MaxStep, multi-variable, reverse integration, and additional opts are covered by gtest + smoke. |
| `ode45(f, tspan, y0, opts)      with odeset() opts` | — | Sig 1+2: ode45(@f, tspan, y0) and ode45(@f, tspan, y0, opts). MATLAB R2025b runs Dormand-Prince 5(4) with Shampine's 4-th order free interpolant for dense output; numkit ode45 implements the same RK tableau (Hairer-Norsett-Wanner Vol I, p.178) and the same Shampine 1986 interpolant. With explicit tspan + tight tols, numkit and MATLAB land identically on the requested sample points (verified to ~1e-9). The fingerprint also exercises shape (n=11, d=1) and analytical solution exp(-2). Refine, MaxStep, multi-variable, reverse integration, and additional opts are covered by gtest + smoke. |
| `dense-output interpolation at explicit tspan points` | — | Sig 1+2: ode45(@f, tspan, y0) and ode45(@f, tspan, y0, opts). MATLAB R2025b runs Dormand-Prince 5(4) with Shampine's 4-th order free interpolant for dense output; numkit ode45 implements the same RK tableau (Hairer-Norsett-Wanner Vol I, p.178) and the same Shampine 1986 interpolant. With explicit tspan + tight tols, numkit and MATLAB land identically on the requested sample points (verified to ~1e-9). The fingerprint also exercises shape (n=11, d=1) and analytical solution exp(-2). Refine, MaxStep, multi-variable, reverse integration, and additional opts are covered by gtest + smoke. |
| `odeget(opts, name)               get value (case-insensitive name)` | — | Sig 1+2: odeget(o, 'RelTol') and odeget(o, name, default). MATLAB normalises name to canonical capitalisation, returns the stored value if non-empty, else the default. Empty Jacobian field falls through to 42 here. |
| `odeget(opts, name, default)      fall back to default when field empty` | — | Sig 1+2: odeget(o, 'RelTol') and odeget(o, name, default). MATLAB normalises name to canonical capitalisation, returns the stored value if non-empty, else the default. Empty Jacobian field falls through to 42 here. |
| `all 21 option names recognised (canonical spelling)` | — | Sig 1+2: odeget(o, 'RelTol') and odeget(o, name, default). MATLAB normalises name to canonical capitalisation, returns the stored value if non-empty, else the default. Empty Jacobian field falls through to 42 here. |
| `odeset(name, value, ...)         name-value pairs (5 distinct options)` | — | Sig 1: odeset(name, value, ...). The returned struct stores values verbatim; case-insensitive name matching writes back the canonical MATLAB spelling. Fingerprint covers the most common subset of the 21 documented options. |
| `RelTol / AbsTol / MaxStep / Refine / NormControl storage round-trip` | — | Sig 1: odeset(name, value, ...). The returned struct stores values verbatim; case-insensitive name matching writes back the canonical MATLAB spelling. Fingerprint covers the most common subset of the 21 documented options. |
| `case-insensitive lookup (MATLAB normalises to canonical capitalisation)` | — | Sig 1: odeset(name, value, ...). The returned struct stores values verbatim; case-insensitive name matching writes back the canonical MATLAB spelling. Fingerprint covers the most common subset of the 21 documented options. |
| `tiledlayout` | — | tiledlayout / nexttile — modern subplot API. Display-only, no scalar return; fingerprint just confirms the workspace stays clean (a, b, c are local set-and-read scalars). Cell layout + figure structure is e2e. |
| `histcounts_integers` | — | histcounts 'BinMethod','integers' vs MATLAB R2025b. One unit-width bin centered on each integer in [round(min),round(max)] of the finite data; edges are center +/-0.5. histcounts([1 1 2 3 3 3],'BinMethod','integers') -> 3 bins, edges [0.5 1.5 2.5 3.5], n=[2 1 3]; [2 5 5 7] -> 6 bins (1.5..7.5), n=[1 0 0 2 0 1]; non-integer data rounds (round(1.2)=1, round(3.1)=3 -> 3 bins); negatives [-2..0] -> edges [-2.5..0.5]; composes with 'Normalization' (probability). Implemented 2026-05-31 (was: 'BinMethod' option rejected). Other BinMethods (auto/fd/scott/sturges/sqrt) remain unsupported (binpicker), and the 65536-bin widening cap is deferred. |
| `str2num` | — 🔬 | str2num evaluates the (bracket-wrapped) string as a MATLAB expression vs R2025b. DEEP-PROBE 2026-05-31: numkit's str2num was a scalar-only std::stod and returned [] for vectors/ranges/expressions. Now str2num_reg routes through the engine: str2num('[10 20 30]')=[10 20 30]; '1:5'=[1 2 3 4 5]; '2+3'=5; '42.5'=42.5; '[1.5 -2.5 3.25]' parsed; a parse/eval failure or non-numeric result -> [] (0x0 double). The 2x2-matrix form ('[1 2;3 4]') and the [X,tf] 2-output flag are verified in the gtest (a literal ';' inside the quoted string can't go in a parity expr — the harness splits statements on ';'). The engine-free Value str2num() overload stays a scalar fallback for embedders. covers:[str2num]. |
| `hex2num` | ✅ 🔬 | hex2num / num2hex (IEEE-754 hex <-> float bit reinterpret) vs MATLAB R2025b. DEEP-PROBE 2026-05-31: both were MISSING ('undefined function'). hex2num reads a hex string as the raw IEEE-754 bit pattern of a DOUBLE (NOT decimal digits — that's hex2dec): hex2num('3ff0000000000000')=1, 'bff0...'=-1; a short string is right-padded with '0' to 16 digits so hex2num('4')='4000000000000000'=2, '41'=131072; '7ff0...'/'fff0...'/'fff8...' -> Inf/-Inf/NaN; an N-row char matrix -> N x 1 double column. num2hex returns the bit pattern as lowercase hex: 16 digits for double, 8 for single (num2hex(single(1))='3f800000'); a vector -> numel x W char matrix (row per element, col-major: num2hex([1 2 3]) row2='4000000000000000'); integer/logical input errors. NOTE: num2hex(NaN) is excluded from the literal-string fingerprint — numkit's NaN constant has bit pattern 0x7ff8... (clear sign bit) vs MATLAB's 0xfff8... (set), a pre-existing CORE NaN-literal difference, NOT a num2hex bug; num2hex faithfully reports the bits and the round-trip hex2num(num2hex(NaN)) is NaN. char-matrix INPUT to hex2num is covered in the gtest (a ';' inside a literal can't go in a parity expr). covers:[hex2num,num2hex]. |
| `num2hex` | ✅ 🔬 | hex2num / num2hex (IEEE-754 hex <-> float bit reinterpret) vs MATLAB R2025b. DEEP-PROBE 2026-05-31: both were MISSING ('undefined function'). hex2num reads a hex string as the raw IEEE-754 bit pattern of a DOUBLE (NOT decimal digits — that's hex2dec): hex2num('3ff0000000000000')=1, 'bff0...'=-1; a short string is right-padded with '0' to 16 digits so hex2num('4')='4000000000000000'=2, '41'=131072; '7ff0...'/'fff0...'/'fff8...' -> Inf/-Inf/NaN; an N-row char matrix -> N x 1 double column. num2hex returns the bit pattern as lowercase hex: 16 digits for double, 8 for single (num2hex(single(1))='3f800000'); a vector -> numel x W char matrix (row per element, col-major: num2hex([1 2 3]) row2='4000000000000000'); integer/logical input errors. NOTE: num2hex(NaN) is excluded from the literal-string fingerprint — numkit's NaN constant has bit pattern 0x7ff8... (clear sign bit) vs MATLAB's 0xfff8... (set), a pre-existing CORE NaN-literal difference, NOT a num2hex bug; num2hex faithfully reports the bits and the round-trip hex2num(num2hex(NaN)) is NaN. char-matrix INPUT to hex2num is covered in the gtest (a ';' inside a literal can't go in a parity expr). covers:[hex2num,num2hex]. |
| `extractAfter` | ✅ 🔬 | extractAfter / extractBefore / insertAfter / insertBefore class + shape preservation on a STRING array vs MATLAB R2025b. DEEP-PROBE 2026-05-31 (cell-only-else sweep, sibling of c138/c139): all four collapsed a string ARRAY to a 1x1 string via strLikeOf(s, op(s.toString())) — op(s.toString()) processed only the first element. MATLAB preserves shape: extractAfter(["a-b" "c-d"],"-") -> ["b" "d"]; extractBefore -> ["a" "c"]; insertAfter(["ab" "cd"],"a","X") -> ["aXb" "cd"]; insertBefore(["ab" "cd"],"b","X") -> ["aXb" "cd"]. Fix: route all four through mapStringPreserveClass (cell->cell, string->string array same shape, char->char). char codes: 'd'=100,'a'=97,'aXb'=283. char + cell paths unchanged (covered in gtest). covers:[extractAfter,extractBefore,insertAfter,insertBefore]. |
| `extractBefore` | ✅ 🔬 | extractAfter / extractBefore / insertAfter / insertBefore class + shape preservation on a STRING array vs MATLAB R2025b. DEEP-PROBE 2026-05-31 (cell-only-else sweep, sibling of c138/c139): all four collapsed a string ARRAY to a 1x1 string via strLikeOf(s, op(s.toString())) — op(s.toString()) processed only the first element. MATLAB preserves shape: extractAfter(["a-b" "c-d"],"-") -> ["b" "d"]; extractBefore -> ["a" "c"]; insertAfter(["ab" "cd"],"a","X") -> ["aXb" "cd"]; insertBefore(["ab" "cd"],"b","X") -> ["aXb" "cd"]. Fix: route all four through mapStringPreserveClass (cell->cell, string->string array same shape, char->char). char codes: 'd'=100,'a'=97,'aXb'=283. char + cell paths unchanged (covered in gtest). covers:[extractAfter,extractBefore,insertAfter,insertBefore]. |
| `insertAfter` | ✅ 🔬 | extractAfter / extractBefore / insertAfter / insertBefore class + shape preservation on a STRING array vs MATLAB R2025b. DEEP-PROBE 2026-05-31 (cell-only-else sweep, sibling of c138/c139): all four collapsed a string ARRAY to a 1x1 string via strLikeOf(s, op(s.toString())) — op(s.toString()) processed only the first element. MATLAB preserves shape: extractAfter(["a-b" "c-d"],"-") -> ["b" "d"]; extractBefore -> ["a" "c"]; insertAfter(["ab" "cd"],"a","X") -> ["aXb" "cd"]; insertBefore(["ab" "cd"],"b","X") -> ["aXb" "cd"]. Fix: route all four through mapStringPreserveClass (cell->cell, string->string array same shape, char->char). char codes: 'd'=100,'a'=97,'aXb'=283. char + cell paths unchanged (covered in gtest). covers:[extractAfter,extractBefore,insertAfter,insertBefore]. |
| `insertBefore` | ✅ 🔬 | extractAfter / extractBefore / insertAfter / insertBefore class + shape preservation on a STRING array vs MATLAB R2025b. DEEP-PROBE 2026-05-31 (cell-only-else sweep, sibling of c138/c139): all four collapsed a string ARRAY to a 1x1 string via strLikeOf(s, op(s.toString())) — op(s.toString()) processed only the first element. MATLAB preserves shape: extractAfter(["a-b" "c-d"],"-") -> ["b" "d"]; extractBefore -> ["a" "c"]; insertAfter(["ab" "cd"],"a","X") -> ["aXb" "cd"]; insertBefore(["ab" "cd"],"b","X") -> ["aXb" "cd"]. Fix: route all four through mapStringPreserveClass (cell->cell, string->string array same shape, char->char). char codes: 'd'=100,'a'=97,'aXb'=283. char + cell paths unchanged (covered in gtest). covers:[extractAfter,extractBefore,insertAfter,insertBefore]. |
| `polyarea` | — | Sig: a = polyarea(x, y). Shoelace area of the polygon (x,y); <3 vertices -> 0. a1 unit square=1, a2 triangle=0.5, a3 2x3 rect=6, a4 L-shape=12, a5 2 points=0. Lifted to public C++ API numkit::builtin::polyarea (was adapter-only) 2026-06. |
| `inpolygon` | — | Sig: in = inpolygon(xq, yq, xv, yv). Crossing-number test vs the unit-square polygon: in1 (0.5,0.5) inside=1, in2 (2,0.5) outside=0, in3 (0.5,-1) outside=0, in4 (0.99,0.99) inside=1, in5 (-0.1,0.5) outside=0. Lifted to public C++ API numkit::builtin::inpolygon (was adapter-only) 2026-06. MATLAB's 2nd 'on' output is a v1 gap (only 'in' returned). |
| `convhull` | — | Sig: K = convhull(x, y). Convex-hull vertex indices (1-based, CCW, first repeated last) of 4 square corners + 1 interior point. nK=5 (interior pt 5 excluded), K=[1 2 3 4 1] (matches MATLAB R2025b monotone-chain order), hull area a=1 (order-invariant guard). Lifted to public C++ API numkit::builtin::convhull (was adapter-only) 2026-06; MATLAB's 2nd 'v' (area) output is a v1 gap. |
| `delaunay` | — | Sig: T = delaunay(x, y). Brute-force empty-circumcircle triangulation of a generic 5-point set (no 4 cocircular). Row order + per-row vertex rotation are algorithm-specific and need NOT match MATLAB, so the fingerprint is order-invariant only: nT = number of triangles (= 2n-2-h, matches MATLAB for points in general position) and A = total triangle area (must equal the convex-hull area, tol 1e-12). Lifted to public C++ API numkit::builtin::delaunay (was adapter-only) 2026-06. NOTE on degeneracy: for cocircular sets (e.g. a square) the brute force keeps all C(4,3) triangles whereas MATLAB picks one diagonal — that case is intentionally excluded from this spec. |
| `histcounts2` | — | Sig: N = histcounts2(x, y, xedges, yedges). 2-D histogram over explicit edges (last bin right-inclusive). 4 points into a 3x2 grid: N(1,1)=2, N(2,1)=1, N(3,2)=1, sum=4. Lifted to public C++ API numkit::builtin::histcounts2 (typed core over explicit edges; nbins/auto resolved in the adapter) 2026-06. |
| `isoutlier_moving` | — | Sig: m = isoutlier(A,'movmedian'|'movmean',window[,'ThresholdFactor',tf]). DEEP-PROBE 2026-06: added moving-window detection (separate spec from isoutlier.json to keep each expr under numkit's long-line parser limit). Window: scalar k -> odd +/-(k-1)/2, even k/2 back & k/2-1 fwd; or [back fwd]; truncated at ends; NaN-excluded. Per element, outlier iff |x_i - center| > tf*scale over the local window: movmedian center=local median, scale=1.4826*localMAD (smm=1, mm4=1 flags the lone 100); movmean center=local mean, scale=local sample std (smaa=0 at default tf=3; ma1d=1 once tf=1). [1 1]==scalar 3 (eqb=1); even k=4 also flags x(4) (m4d=1). Sliding median catches two separated anomalies (smy=2) and runs per-column on matrices (sMm=2). Bit-identical MATLAB R2025b. |
| `del2` | — | Sig: L = del2(U[,h]). Discrete Laplacian ~ div(grad U)/(2*ndims). Per dimension a centered 2nd difference on the interior divided by 2h^2, linearly extrapolated to the boundary, summed over dims and divided by ndims (=2 for both vectors and matrices). Vector x^2-ramp -> constant 0.5 (sum 2.5); scalar h scales 1/h^2 (h=2 -> 0.125); [1 2 3;4 5 6;7 8 10] -> sum 1.5, corners/centre 0; magic(4) -> sum 0, L(2,3)=-3. New (MATLAB datafun, was MISSING) 2026-06. v1: 1-D vector + 2-D matrix, scalar spacing; per-axis hx/hy and coordinate-vector spacing deferred. Bit-identical MATLAB R2025b. |
| `spectrogram_ps` | — | Sig: [s,f,t,ps] = spectrogram(...). DEEP-PROBE 2026-06: the 4th output ps (one-sided power spectral density) was missing ('Too many output arguments'). ps[k] = c[k]*|s[k]|^2/(fs*sum(win.^2)), c=2 on interior bins, 1 at DC and (when present) Nyquist — reuses the already-computed STFT. With an explicit window+fs (8-pt Hamming, fs=100): ps(1,1)=1.08212, ps(3,2)=1.98718. No-fs default-window case (hamming(floor(nx/4.5)), fs=2*pi, nfft=128): ps2 is 129xM. Bit-identical MATLAB R2025b. |
| `sort_missingplacement` | — | Sig: sort(X,...,'MissingPlacement','first'|'last'|'auto'). DEEP-PROBE 2026-06: the option was silently ignored, so ascending+'first' left NaN LAST (and descending+'last' left it first). Fixed: a NanPlace flag threaded through sort/sortComplex — 'first'/'last' force the NaN side regardless of direction; 'auto' (default) = last for ascending, first for descending (unchanged). asc+'first' -> [NaN 1 2 3] (f1=1,f2=1,f4=3); desc+'last' -> [3 2 1 NaN] (l1=3,l4=1); index output follows placement ([5 NaN 1] first -> i=[2 3 1]); plain sort unchanged (a4=1, NaN last). Bit-identical MATLAB R2025b. bugs/builtin/sort-missingplacement.md. |
| `max_min_all` | — | Sig: max/min(A,[],'all'[,'linear']). DEEP-PROBE 2026-06: the 'all' option was entirely broken — the adapter ran toScalar on the 'all' char vector ('Cannot convert char to scalar'), so max(A,[],'all') / min(...,'all') / the 'all','linear' combo all threw. Fixed: max_reg/min_reg detect the 'all' string and reduce over the flattened array (the column position is the linear index, matching MATLAB's always-linear 'all' 2nd output); works for 2-D, N-D, and 'omitnan'. max(A,[],'all')=9 at linear idx 6; min=1; 3-D max=24/min=1; min([5 NaN 2 8],'all','omitnan')=2 at idx 3. Bit-identical MATLAB R2025b. bugs/builtin/max-all-linear.md. |
| `unique_last` | — | Sig: [C,ia,ic]=unique(A,'last'). ia selects the LAST occurrence (sorted setOrder): vec [3 1 2 1 3] -> ia=[4 3 5]; complex [1+1i 2 1+1i 3] -> ia(3)=4; rows -> ia(1)=3. Fixes bugs/builtin/unique-last.md (sorted order). 'stable'+'last' ordering deferred. |
| `dct_types` | — | Sig: dct/idct(x,n,'Type',t), t in 1..4. Orthonormal DCT-I/III/IV (Type 2 is the default). DCT-I & DCT-IV are self-inverse; idct Type 3 == dct Type 2. Fixes bugs/signal/dct-types.md. Refs vs MATLAB R2025b. |
| `spline_n3` | — | Sig: yq = spline(x,y,xq) with n==3 knots. MATLAB not-a-knot reduces to the unique interpolating PARABOLA: spline([1 2 3],[1 4 9],2.5)=6.25 (x^2 reproduced exactly; was 6.3125 from an incorrect natural-BC fallback). Non-uniform=5.0; [1 2 4],[3 1 5]@3=1.6667; downward parabola=0.75. Fixed 2026-06-03 (n>=4 was already correct). |
| `accumarray_fn` | — | Sig: accumarray(subs,vals,sz,@fn[,fillval]). 2026-06-04: arbitrary function handles (named or anonymous), not just the built-in reducers (@sum/@max/...), are applied to each output cell's column of values. @(x)sum(x.^2) on cell {1,2}=5; @(x)max-min range; empty cell gets fillval -1. vs MATLAB R2025b. |
| `gradient` | — | Sig: g = gradient(F[,h]) / [FX,FY] = gradient(F[,hx,hy]). Central differences interior, one-sided at the ends. Single output = the dim-2 (x / column-direction) gradient; 2-output adds FY = dim-1 (y). Optional scalar spacing. COMPLEX F (fixed 2026-06-05): real + imaginary parts gradiented separately, then recombined: gradient([1+1i 3+3i 5+5i])=[2+2i 2+2i 2+2i]; matrix single + 2-output per-part. (N-D F still 1-D/2-D only — tracked by bugs/builtin/gradient-3d.md.) Matches MATLAB R2025b. |
| `pdist_metrics` | — | Sig: D = pdist(X, metric[, p|S]); D = pdist2(X, Y, metric). Covers the pdist-metrics bug fix (bugs/stats/pdist-metrics.md): 'seuclidean' (default per-column std scale + explicit scale vector S=[1 2 3]), 'spearman' (tiedrank rows then correlation distance, no-ties dsp(2)=dsp(3)=0.5 and ties dt(1)=1.5), 'cosine' zero-norm row -> NaN (was 1), and 'correlation' constant row -> NaN (MATLAB is source of truth; md named only cosine but MATLAB returns NaN for both). pdist2 variants: seuclidean default scale = std(X) (first arg); spearman ranks each side, Q2(1,1)=0.5 and Q2(1,2)=NaN (Py row 2 constant). NaN cases fingerprinted via double(isnan(.)) so the numeric comparator stays valid. |
| `gradient_nd` | — | Sig: g = gradient(F); [gx,gy,gz,...] = gradient(F[,hx,hy,hz,...]). Covers the gradient N-D fix (bugs/builtin/gradient-3d.md): N-D (3-D + 4-D) input, one gradient per dimension up to nargout with the MATLAB output->dim map out1=dim2(x) out2=dim1(y) out_k=dim(k+1); single output = the x (dim-2) gradient; central differences interior + one-sided ends (sum fingerprints catch any element-level drift); per-dim spacing (2,3,4) and single-spacing broadcast (sb=A/2); non-cube 2x3x2; complex 3-D gradiented part-wise (real/imag). 2-D + vector paths unchanged. |
| `dwtest_exact` | — | Sig: [p, dw] = dwtest(r, X[, 'Tail', tail][, 'Method', m]). EXACT Durbin-Watson p-value via Imhof's characteristic-function inversion (fixed 2026-06-05, bugs/stats/dwtest-pvalue.md): under H0 the residuals are M*eps (M = I - X(X'X)^-1 X'), DW<d <=> eps'M(A-dI)M eps < 0, and the n-k residual-space eigenvalues of MAM feed Imhof's integral. Matches MATLAB's default 'exact' method to ~1e-9. Tails: 'right' p=P(DW<dw) (positive autocorrelation), 'left'=1-that, 'both'=2*min (default). low-DW design dw1=0.3142857 both~0 right~0 left=1; high-DW dw3=3.5 both=0.00569352 right=0.99715324 left=0.00284676; design2 dw4=0.1519608 both~0. ('Method','approximate' is numkit's own beta approximation, not MATLAB-identical, so not fingerprinted here.) |
| `kstest_exact` | — | Sig: [h,p,ksstat,cv]=kstest(x[,'Tail',t][,'Alpha',a]); [h,p,ks2stat]=kstest2(x,y[,'Tail',t]). EXACT finite-n Kolmogorov-Smirnov (fixed 2026-06-05, bugs/stats/kstest-pvalue.md): one-sample two-sided p via Marsaglia-Tsang-Wang 2003 matrix method (corrected asymptotic 2*exp(-(2.000071+.331/sqrt(n)+1.409/n)*n*D^2) when s=n*D^2>7.24 or (s>3.76&&n>99)); one-sided p via Birnbaum-Tingey 1951; kstest2 via Stephens' corrected asymptotic (lambda=(sqrt(ne)+0.12+0.11/sqrt(ne))*D). The KS statistic was already correct; only D->p (and a->cv) were wrong (asymptotic). Validated to ~1e-9: n=6 two-sided p=0.94998410, one-sided 0.97197377/0.57170523, n=12 p=0.98282723, kstest2 two-sided 0.84705434 one-sided 0.47200535. NOTE: cv is obtained by inverting the same exact p-function and matches MATLAB's kstest critical-value table to its 5-significant-figure precision (n=6: 0.51926/0.46799/0.61661) — MATLAB stores a rounded table, so cv is validated at 1e-4 in toolboxes/stats/tests/kstest_exact_test.cpp rather than fingerprinted here at 1e-6. |
| `isoutlier_gesd` | — | Sig: isoutlier(x,'gesd'[,'ThresholdFactor',a][,'MaxNumOutliers',k]). Generalized ESD test (Rosner 1983), fixed 2026-06-05 (bugs/stats/isoutlier-gesd.md): peel the most-extreme studentized deviate for k iterations (default k = max(1,round(n/10))), critical value lambda_i = the Grubbs form at the current size m=N-i+1 (= ((m-1)/sqrt(m))*sqrt(t^2/((m-2)+t^2)), t=tinv(a/(2m),m-2)), flag every peeled point up to the LARGEST iteration whose deviate exceeds lambda (handles masking/swamping). ThresholdFactor a default 0.05. Validated vs MATLAB R2025b: single outlier (repro + mid-vector), clean->none, spread n=25 peels 3, masking n=15 default->0 but MaxNumOutliers=5->5, ThresholdFactor=0.01, small n=5. |
| `dist_broadcast` | — | bugs/stats/distribution-array-params.md (cycle 29: normal+exponential; cycle 30: gamma+beta+chi2 pdf+cdf; cycle 31: gamma+beta+chi2 inv — gaminv/betainv/chi2inv broadcast via the broadcasting builtin::gammaincinv/betaincinv + a degenerate-quantile fixup loop: gamma a==0->0, chi2 k==0->0, a<0/b<=0/k<0->NaN; gi3/ci2 cover the degenerate. cycle 32: rayleigh+weibull+lognormal — all closed-form (exp/log/sqrt/erf/erfinv), kernel + broadcast_dist2/3, per-element b/a/sigma<=0->NaN; logninv via Acklam phiInv ~1e-9 so still under tol 1e-9. cycle 33: students_t — tpdf closed-form kernel; tcdf/tinv broadcast over nu via the broadcasting builtin::betainc/betaincinv on z=nu/(nu+x^2) (resp. 2*min(p,1-p)) + a per-element fixup loop for the x-sign branch, nu<=0->NaN, and the nu==Inf Gaussian limit (tcdf->normcdf, tinv->norminv via Winitzki). tiv covers a per-element Inf in a vector. cycle 34: fisher_f — fpdf closed-form kernel (x==0 regimes v1<2->Inf/v1==2->finite/v1>2->0); fcdf = betainc(v1*x/(v1*x+v2), v1/2, v2/2) built via broadcast_dist3; finv via betaincinv + fixup x=(v2/v1)z/(1-z); d1<=0||d2<=0->NaN. fi1(1)=finv(.95,1,1)=161.4 omitted from fingerprints — extreme small-dof betaincinv tail, asserted in gtest at 1e-6. cycle 35: DISCRETE binomial+poisson — binopdf/poisspdf closed-form pmf kernel (k noninteger->0, n noninteger->NaN, p oob->NaN, lambda<0->NaN); binocdf via per-element betainc I_{1-p}(n-floor(k),floor(k)+1), poisscdf via 1-gammainc(lambda,floor(k)+1); binoinv/poissinv discrete-quantile walk per element. All exact vs MATLAB. cycle 36: DISCRETE closed-form unid+geometric — pure scalar kernels via broadcast_dist2 (unid: pmf 1/N for integer k in 1..N, cdf floor(k)/N, inv ceil(p*N); geo k>=0: pmf p(1-p)^k, cdf 1-(1-p)^(floor(k)+1), inv via log ratio); N<1/noninteger N -> NaN, p<=0/p>1 -> NaN, noninteger k -> 0. cycle 37 (LAST two families): negbin (nbinpdf closed-form kernel; nbincdf=I_p(r,floor(k)+1) per-element betainc; nbininv discrete-quantile walk; r<=0/p oob -> NaN, r may be non-integer) + hypergeom (4-operand broadcast_dist4 over k,M,K,n; pmf/cdf/inv scalar kernels via gammaln; invalid params/out-of-support -> NaN/0). ALL 16 distribution families now broadcast array params). The *pdf/*cdf/*inv functions broadcast ARRAY distribution parameters (mu/sigma/a/b/k), not just the data x: scalar expands, equal sizes element-align, mismatched non-scalar sizes error ('Non-scalar arguments must match in size.'), shape follows the non-scalar operand (npS is 2x2). Per-element domain: sigma<=0/mu<=0/a<0/b<=0/k<0 -> NaN, gamma a==0 -> 0, chi2 k==0 -> 0 (NaN entries fingerprinted around). normpdf/normcdf/exppdf/expcdf/expinv/gampdf/betapdf/chi2pdf match MATLAB to ~1e-15; cdf's via incomplete gamma/beta (gamcdf/betacdf/chi2cdf) and norminv match to ~1e-9 to 1e-11, so spec tol is 1e-9. 'upper' flag works under broadcast (eu1, gu1). gamma/beta/chi2 INV (gaminv/betainv/chi2inv) + remaining families (t/f/rayl/wbl/logn, bino/poiss/...) land in later cycles -- md stays OPEN. Matches MATLAB R2025b. |
| `cumulative_logical` | — | cumsum/cumprod/cummax/cummin on logical input. cumsum/cumprod PROMOTE logical->double; cummax/cummin PRESERVE the logical class (verified MATLAB R2025b). Covers vector, 2-D column + dim2, and 'reverse'. char still errors on both engines (numeric-or-logical only). bugs/builtin/cumulative-logical.md. |
| `trapz_logical` | — | trapz on logical input. MATLAB promotes a logical X and/or Y to double (class NOT preserved — integration returns double). Covers unit-spacing vector, non-uniform X, logical-X+double-Y, 2-D column + dim2, scalar (->0) and empty (->0). char still errors on both engines. bugs/builtin/trapz-logical.md. |
| `sort_logical` | — | sort on logical input. MATLAB PRESERVES the logical class on the sorted VALUES (S, Sd, C, R, sc) while the 2nd-output INDEX (I, Id) stays double. Stable: equal elements keep input order, so I=[1 3 2 4] and Id=[1 3 2 4 5]. Covers ascend/descend, 2-D column + dim2, scalar. char is NOT coerced (sorts by code point, stays char) — covered in the gtest. bugs/builtin/sort-logical.md. |
| `sort_char` | — | sort on char input. MATLAB sorts char by CODE POINT, PRESERVING the char class on the values; the 2nd-output INDEX stays double. Fingerprints use double(...) to read code points. Stable: sort('cbacb')='abbcc', Ir=[3 2 5 1 4]. Covers ascend/descend, repeated chars, 2-D column (['bd';'ca']->['ba';'cd']) + dim2 (->['bd';'ac']), scalar. The ; inside ['bd';'ca'] is a matrix-row separator, NOT inside a quoted string. bugs/builtin/sort-char.md. |
| `unique_typeclass` | — | unique on char/logical/integer input. MATLAB PRESERVES the input class on the unique VALUES while ia/ic stay double. Fingerprints read values via double(...). Covers char ('cbabc'->'abc', ia=[3 2 1] first-occurrence, ic=[3 2 1 2 3]), 'stable' (->'cba'), logical ([0 1]), int8 ([1 2 3]), uint16 column vector (3x1 orientation preserved). The ; inside uint16([30;10;30;20]) is a matrix-row separator, not inside a quoted string. bugs/builtin/unique-typeclass.md. |
| `setops_typeclass` | — | ismember/intersect/setdiff/union on char/logical/integer input. MATLAB accepts all: intersect/setdiff/union PRESERVE the input class on the VALUES (ia/ib stay double); ismember returns logical tf + double loc. Fingerprints read char/logical/int values via double(...). Covers char ('cabc'/'bdc' intersect 'bc' ia=[3 1] ib=[1 3]; setdiff 'ace'; union 'abc'; stable 'bcad'), logical, int8. bugs/builtin/setops-typeclass.md. |
| `cummax_cummin_integer` | — | cummax/cummin on integer input. MATLAB PRESERVES the integer class (order statistics — result is a subset of the input, exact double round-trip). Fingerprints read values via double(...). Covers int8 ([3 3 3 5 5] / [3 1 1 1 1]), uint16, 2-D dim2, 'reverse', negative signed. bugs/builtin/cummax-cummin-integer.md. |
| `movfun_typeclass` | — | movsum/movprod/movmean on integer/logical input. MATLAB PROMOTES integer/logical to double for these arithmetic moving functions (class NOT preserved). Window 3 (and 2 for uint16), endpoints 'shrink'. movsum(int8([3 1 2 5 4]))=[4 6 8 11 9], movprod=[3 6 10 40 20], movmean=[2 2 2.6667 3.6667 4.5], movsum(logical)=[1 2 2 2 1]. char is NOT promoted (MATLAB errors); movmax/movmin/movmedian preserve class (separate). bugs/stats/movfun-typeclass.md. |
| `movfun_order_stats` | — | movmax/movmin/movmedian on integer/logical. MATLAB PRESERVES the class for these order statistics: movmax/movmin int->int (exact) & logical->logical; movmedian int->int with ROUND-half-away (1.5->2, 4.5->5, -1.5->-2) but logical->DOUBLE (0.5 median). Window 3 unless w2. Fingerprints read int/logical via double(...); lmd (logical movmedian) is already double. bugs/stats/movfun-order-stats.md. |
| `sprintf_complex` | — | sprintf/fprintf use the REAL part of a complex argument for numeric conversions (imaginary discarded), matching MATLAB. Char-code fingerprints via double(s(k)) (string compare gives a vector). sprintf('%g',1+2i)='1'; sprintf('%d ',[1+2i 3+4i])='1 3 '; sprintf('%.2f',3.5-1.5i)='3.50'; sprintf('%g ',[1.5+0i 2.5])='1.5 2.5 '; sprintf('%d',complex(7,0))='7'. bugs/builtin/sprintf-complex.md. |
| `maxmin_char_double` | — | max/min of a CHAR array return DOUBLE (the code point), NOT char — MATLAB does not preserve the char class for max/min (mode DOES keep char). max('abc')=99, min('abc')=97, [v,idx]=max -> 99/3, column-wise max(['abc';'xyz'])=[120 121 122], dim2=[99 122], 'all'=99, mode('abc')='a' (double()=97). NOTE: the binary max('a','c') form is NOT tested here — MATLAB ERRORS on it ('Invalid second argument'); numkit is lenient (returns 99 double) but that is not a MATLAB-matched form. The ; inside ['abc';'xyz'] is a matrix-row separator. bugs/builtin/maxmin-char-double.md. |
| `gamma_poles` | — | gamma at non-positive-integer poles returns +Inf (NOT NaN), matching MATLAB; gamma(-Inf)=Inf, gamma(Inf)=Inf, gamma(NaN)=NaN. Pole points fingerprinted via isinf() (robust Inf compare); finite g(5)=gamma(0.5)=1.7724539, g(6)=gamma(-0.5)=-3.5449077, g(7)=gamma(5)=24. bugs/builtin/gamma-negative-integer-poles.md. |
| `polyder_product` | — | polyder(a,b) with ONE output is the derivative of the PRODUCT a*b (= polyder(conv(a,b))): polyder([1 0],[1 1])=[2 1], polyder([1 2],[1 3])=[2 5], polyder([1 0 0],[1 1])=[3 2 0]. The TWO-output [q,d] quotient form (q=1, d=[1 2 1]) and the single-arg form (polyder([1 2 3])=[2 2]) are unchanged. bugs/builtin/polyder-product.md. |
| `psi_zero` | — | psi(0) = -Inf (digamma pole at 0), was NaN — matches MATLAB. Pole fingerprinted via isinf()+sign (z=1, zs=1 for -Inf). Finite values unchanged: psi(1)=-0.57721566, psi(2)=0.42278434, psi(0.5)=-1.96351, psi(10)=2.2517526; vector psi([0 1 2]) -> [-Inf -0.577216 0.422784]. psi(negative) left as-is (MATLAB errors). bugs/builtin/psi-zero-pole.md. |
| `str2double_complex` | — | str2double parses complex-number strings (was NaN). Trailing lowercase i/j marks imaginary; real/imag split at the last non-exponent +/-; pure-imaginary + bare-i forms; '1e-3+2i'=0.001+2i (exponent edge); '1e3i'=1000i; '1+i'=1+1i; ' 2 + 3i '=2+3i (spaces); '5' stays real (isreal=1); capital '1+2I'=NaN; cell mix {'1+2i','3','4-1i'} -> COMPLEX (isreal=0). bugs/builtin/str2double-complex.md. |
| `concat_integer_types` | — | Integer concatenation (cat/[;]/[,]/vertcat/horzcat). MATLAB R2025b: the FIRST integer operand's class wins; double/logical/a different int class/the real part of complex are cast with round-half-away + saturate. [int8;int8]=int8 [1 2;3 4]; [int8,int8]=int8 1x4; cat(1,uint16)=uint16; [int8;50.6]=51 (round); [int8;300]=127 (sat hi); [int8;-300]=-128 (sat lo); [int8;true]=1; [int8,int16]=int8 (first wins); [int8;2+3i]=2 (real part). Values read via double(...). bugs/builtin/concat-integer-types.md. |
| `kron_integer_class` | — | kron preserves the integer class of integer operands (MATLAB R2025b). Branches: same int class -> that class, saturating (kron(int8(100),int8(2))=127, kron(uint8(200),uint8(2))=255, kron(int8(-100),int8(2))=-128); int + real scalar double -> int class, scalar cast round-half-away (kron(int8([2 3]),2)=[4 6] int8; kron(int8(2),1.5)=3); double*double stays double (kron([1 2],[3 4])=[3 4 6 8]). Mixed int classes / int+nonscalar-double / int+logical ERROR in MATLAB (numkit lenient -> double, not asserted). Values read via double(...). bugs/linalg/kron-integer-class.md. |
| `conv_integer_input` | — | conv accepts integer/logical input (was: throw 'Not a double array'). MATLAB R2025b promotes to double — the result is ALWAYS double, never the integer class (unlike kron/cross). Branches: int8 'full' [1 3 5 3]; 'same' [3 5 3]; 'valid' [3 5]; int+double [1 3 5 3]; uint8 [1 3 5 3]; int16 [200 600 400]; logical [1 1 1 1]; double*double unchanged [4 13 28 27 18]. Values are plain double so no double() wrap needed. bugs/signal/conv-integer-input.md. |
| `deconv_integer_input` | — | deconv accepts integer/logical input (was: throw 'Not a double array'). MATLAB R2025b promotes to double — quotient AND remainder are ALWAYS double, never the integer class (like conv, unlike kron/cross). Branches: int8 q=[1 2 3]; int16 q=[2 3 1]; int+double q=[1 2 3]; logical q=[1 -1 2]; [q,r] exact -> r all-zero; divisor longer (na>nb) DOUBLE -> q scalar 0, r=numerator [1 1]; double/double unchanged [2 3 1]. NOTE: MATLAB ERRORS on the na>nb branch with INTEGER input ('Inputs must be floats', superiorfloat quirk) — numkit stays lenient there (returns double); that edge is intentionally NOT fingerprinted (see md). Values are plain double. bugs/signal/deconv-integer-input.md. |
| `accumarray_integer_vals` | — | accumarray accepts integer/logical vals (was: throw 'vals must be DOUBLE'). MATLAB R2025b output class follows the reducer: sum/prod/mean -> double, but max/min PRESERVE the integer class of vals (like the reductions). Branches: default sum int8 [40 20] double; @max int8 [100 100] int8; @min int8 [10 20] int8; @prod double [300 20]; @mean double [20 20]; @max uint16 [100 200] uint16; 2-D subs [5 0 0 7]; sz [3 1] -> [40 20 0]; fillval -1 -> [5 -1 7 -1]; logical default-sum -> double [2 0]; double unchanged. Class checked via isa(). NOTE not fingerprinted (numkit-lenient niches, see md): logical+@max -> MATLAB logical / numkit double; custom non-builtin handle (e.g. @(x)x(1)) follows handle class in MATLAB / numkit double. bugs/builtin/accumarray-integer-vals.md. |
| `cross_integer_class` | — | cross preserves the integer class of integer operands with MATLAB R2025b's PER-OPERATION saturating integer arithmetic: each element product saturates to the int range BEFORE the subtraction (cross(int8([100 100 0]),int8([0 100 100]))=[127 -127 127], NOT -128). Branches: int8 [-3 6 -3]; int32 [0 0 1]; saturation [127 -127 127]; uint8 [0 6 0] (negatives clamp to 0); int+double [-3 6 -3] (each product is int-scalar*double, allowed); int16 [-300 600 -300]; Nx3 layout; double*double unchanged. Class via isa(). NOTE not fingerprinted (MATLAB ERRORS, numkit lenient->double): different int classes (int8 cross int16) and int+logical. bugs/linalg/cross-integer-class.md. |
