# Parity gaps — consolidated inventory (migrated from PROGRESS.md)

> **What this is.** The full list of MATLAB R2025b functions numkit does **not
> yet** fully match, migrated out of `PROGRESS.md` so `bugs/` is the single
> place to look for gaps. These are **parity gaps, NOT defects** — a missing or
> partial function is a feature gap, not a bug in shipped code (see
> `bugs/README.md` for the defect catalog: `bug` / `stub` / `missing-output`).
>
> **`PROGRESS.md` stays** — it remains the parity harness's live coverage map
> (`tools/parity/run_parity.py` appends to it; `tools/parity/diff_local_ref.py`
> reads it as the canonical "what we have" list). This file is the human-facing
> gap inventory derived from it.
>
> **Kinds here:**
> - **⚠️ partial** — implemented, but a documented branch/option is deferred.
> - **❌ missing** — not implemented at all.
>
> **Individually tracked.** The notable gaps below also have their own
> `bugs/<ns>/<fn>.md` entries (and appear in the `bugs/README.md` OPEN tables):
> friedman, distribution-dispatchers, autocorr (stats); pmusic-peig, fillgaps,
> stmcb (signal); watershed, imfindcircles, corner (image); wpdec,
> wentropy/ddencmp, wenergy/upcoef, cwt, wavedec2-family, centfrq/scal2frq
> (wavelet); lqr/hinfnorm, care/dare, minreal, initial, allmargin, covar
> (control); analog-demodulators, syndtable (comm); numerical-integration-nd,
> ode-stiff (ode); funm, qz/gsvd (linalg); fsolve, nonlinear-lsq,
> constrained-solvers (optim). The rest are captured only here.

## ⚠️ Partial — implemented with a deferred branch (25)

| function | namespace / section | deferred branch (summary — see PROGRESS.md for full detail) |
|---|---|---|
| `colon` | Builtin / Matrices and Arrays | works as `:` (range) operator; not callable as named fn |
| `fillmissing` | Builtin / Tables / Timetables | fillmissing — replace NaN by method. Methods: 'constant', 'previous', 'next' (existing); 'nearest', 'linear' (cycle 74). Per-column for matrices. Tie-break in 'nearest' picks NEXT. 'linear' default extrapolates leading/trailing NaNs via slope of nearest interior good-value pair. DEEP-PROBE 2026-05-31: added the 'EndValues' name-value option (numkit previously threw 'Cannot convert char to scalar'). 'EndValues' governs ONLY endpoint missing entries (before first / after last original good value); interior runs always filled by the method. Supported: 'extrap' (default, current behavior), 'none' (leave endpoints NaN), 'nearest' (leading=first good, trailing=last good), numeric constant. 'EndValues' rejected for 'constant'/'mean'/'median' methods; 'previous'/'next' EndValues deferred. Verified vs MATLAB R2025b: a=[NaN NaN 3 5 NaN 9 NaN] linear+none=[NaN NaN 3 5 7 9 NaN], +0=[0 0 3 5 7 9 0], +nearest=[3 3 3 5 7 9 9], previous+(-7)=[-7 -7 3 5 5 9 -7]. Deferred MATLAB methods: 'spline', 'pchip', 'makima', 'movmean', 'movmedian', 'knn'. |
| `topkrows` | Builtin / Tables / Timetables | Sig: B = topkrows(A, k[, col[, direction]]); [B,I] = topkrows(...). Top k rows by column-priority sort (default: all columns, descending lex). col selects a single column or vector of columns (priority order). direction = 'ascend' |
| `genqammod` | Communications / Modulation | MATLAB genqammod / genqamdemod: integer-input lookup into a user-supplied constellation, demod = nearest-neighbour. Covered: 8-PSK constellation forward+round-trip, real PAM constellation, noisy demod still picks correct neighbour. Bit-input mode (`'InputType','bit'`) deferred -- documented. Octave 11.1.0 doesn't ship genqammod in core (signal/communications package only); reports N/A. |
| `apskmod` | Communications / Modulation | MATLAB apskmod / apskdemod with explicit identity SymbolMapping (numkit's default). Engine-detecting shim handles MATLAB's name-value form vs numkit's positional 5th arg. Standard 16-APSK [4,12] [1, 2.7] forward+round-trip + nearest-neighbour demod under small noise. Bit-equal with MATLAB R2025b. Default 'gray' SymbolMapping deferred -- MATLAB's per-ring Gray for non-power-of-2 (M=12) needs more probing. Octave 11.1.0 doesn't ship apskmod in core; reports N/A. |
| `mskmod` | Communications / Modulation | MATLAB mskmod (differential variant): minimum-shift keying. Bit-equal with MATLAB R2025b. Algorithm: cumulative-sum phase ramp interpolated linearly between symbol boundaries, then exp(i*phase). Differential mode used (MATLAB's default; passed explicitly via 'diff' arg through engine-detecting shim because MATLAB requires it). Argument order: mskmod(x, nSamp, dataenc, ini_phase) -- dataenc is positional 3rd, NOT 4th. ini_phase must be a multiple of pi/2 in MATLAB; numkit accepts arbitrary (extension). KNOWN GAP: non-differential variant deferred (uses rectpulse + I/Q stagger). Octave 11.1.0 doesn't ship mskmod in core; reports N/A. |
| `dpcmenco` | Communications / Source Coding | MATLAB dpcmenco/dpcmdeco: 1st-order DPCM (predictor=[0 1]) with 6-bin codebook + 5-threshold partition. Bit-equal with MATLAB R2025b on encoded indices, quantization error, and reconstructed signal. Round-trip qe consistency (qe from encoder == qe from decoder via codebook lookup) also verified. dpcmopt deferred (training-set optimisation needs Lloyd-Max + alternating predictor estimation -- own cycle). Octave 11.1.0 doesn't ship dpcmenco/deco in core; reports N/A. |
| `lloyds` | Communications / Source Coding | MATLAB lloyds: Lloyd-Max scalar quantizer designer. Tested on deterministic monotone training (1:10) since random-seed paths use randn which differs (Ziggurat for randn deferred). Bit-equal with MATLAB R2025b on initial-codebook form ([2 5 8]) and integer-K form (K=2, K=4). Octave 11.1.0 doesn't ship lloyds in core (signal/communications package only); reports N/A. |
| `balance` | Linear Algebra / File Name Construction | MATLAB balance: Parlett-Reinsch diagonal scaling & permutation phase (`balance(A)` and `balance(A, 'noperm')`). Complete with Parlett–Reinsch permutation phase. |
| `eig` | Linear Algebra / File Name Construction | Complete with complex Schur iteration for asymmetric matrices and complex inputs. |
| `ellipord` | Signal / Filter Design | Sig: [n, Wn] = ellipord(Wp, Ws, Rp, Rs[, 's']). Bit-equal MATLAB R2025b on lowpass / highpass / bandpass / analog. KNOWN GAP: bandstop (ftype=3) deferred. Octave: in signal package, not core. |
| `firls` | Signal / Filter Design | Sig: b = firls(N, F, A). Type-I least-squares FIR design with piecewise-linear desired amplitude. Cholesky on (M+1)x(M+1) Q matrix from closed-form integrals of cos(i*w)*cos(j*w) over each band. Bit-identical with MATLAB R2025b on lowpass design (21-tap, [0,0.4]/[0.5,1] bands). NOTE: only Type-I (even N) supported in this revision; Type-III/IV (Hilbert, differentiator) and per-band weights are deferred. |
| `corr` | Statistics / Descriptive Statistics — extras | Sig c=corr(X) / [c,p]=corr(X,Y[,'Type',T][,'Rows',R]). Pearson matrix (single-arg between cols of X; two-arg X.cols x Y.cols). 'Type' Spearman = Pearson of tied (average) ranks; 'Kendall' = tau-b (ties adjusted). corr([1;2;3;4],[1;4;9;16]) Pearson=0.984374 but Spearman=Kendall=1. With ties c,d: Spearman=0.892218, Kendall=0.824958. numkit previously IGNORED 'Type' (always Pearson) -- now honored. 2026-05-30: 'Rows' NaN policy added (previously IGNORED -- corr always NaN-poisoned). 'complete' = listwise deletion (drop any row with a NaN, then correlate): cc12=1, cc13=-0.300376. 'pairwise' = pairwise deletion (each (i,j) uses rows where both columns are non-NaN, so entries can differ): cp13=-0.290191 vs cp23=-0.300376. Two-vector 'complete' xyc=1. 'pairwise' currently Pearson-only. 2026-06-05 (bugs/closed/stats/corr-pvalue.md): 2nd output p-value added. Pearson p = 2*tcdf(- |
| `nearcorr` | Statistics / Descriptive Statistics — extras | MATLAB nearcorr: nearest correlation matrix (Higham 2002 alternating projections + Dykstra). Identity case (input already correlation) is unchanged; Higham 3x3 textbook example produces [-0.4041, 0.4988, 0.5912] off-diagonals; output is symmetric, unit-diag, PSD (min eigval ~ 0 for indefinite inputs). Defaults tol=1e-10, maxits=100; 'tolconv'/'maxits' name-value parameters deferred for v1. Uses eig_symmetric (src/toolboxes/linalg) for the PSD projection. Octave 11.1.0 doesn't ship nearcorr in core (statistics package only); reports N/A. |
| `mle` | Statistics / Distribution Fitting (MLE / likelihood) | Sig: [phat, pci] = mle(data[, 'distribution', name][, 'Alpha', a]). Closed-form MLE for normal (default) / exponential / poisson / lognormal. The 2nd output pci (fixed 2026-06-05, bugs/closed/stats/mle-output.md) is the 2xk confidence-interval matrix (row 1 lower, row 2 upper, one column per parameter), default Alpha=0.05, reusing the matching *fit CI machinery (normfit/expfit/poissfit; lognormal = normfit of log data) which already matches MATLAB. phat bit-identical; pci matches via tinv/chi2inv-based CIs. Custom 'pdf'/'logpdf'/'nloglf' deferred. |
| `bootci` | Statistics / Resampling Techniques | Sig: ci = bootci(nboot, fn, X[, alpha]). Percentile bootstrap CI. NOT bit-identical with MATLAB (std::uniform_int_distribution implementation-defined; randn also not bit-identical). Statistical correctness verified: 95% CI contains true mean. |
| `bootstrp` | Statistics / Resampling Techniques | Sig: B = bootstrp(nboot, fn, X). Bootstrap resampling. Output shape verified; values not bit-identical with MATLAB (uniform_int_distribution + randn divergence). |
| `crossval` | Statistics / Resampling Techniques | Sig: vals = crossval(predfun, X, Y[, 'kfold', K]). K-fold cross-validation. Default K=10. NOT bit-identical with MATLAB (fold splitting differs -- numkit uses contiguous blocks, MATLAB defaults to random). Shape verified. |
| `jackknife` | Statistics / Resampling Techniques | needs Engine::call for function handles |
| `anova2` | Statistics / ANOVA / MANOVA / Correlation | Sig: p = anova2(Y[, reps]). Two-way ANOVA without replication (reps=1 only in this revision; reps>1 with interaction deferred). p = [p_cols, p_rows, p_interaction]. Bit-identical with MATLAB R2025b on probed cases. |
| `cond_pnorm` | Misc / not in TODO / Decomposition Trees and Misc | Sig: c = cond(A, p) for p ∈ {1, 2, Inf, 'fro'}. Closes the ⚠️ gap in PROGRESS where cond was 2-norm only. p=2 routes through cond_2norm (sigma_max/sigma_min); other p via norm(A,p)·norm(inv(A),p). Diagonal A = diag(1, 1e-3) gives exactly 1e3 for p=1,2,Inf and slightly above for 'fro' (sqrt(1+1e-6) · sqrt(1+1e6) ≈ 1e3 + 0.5e-3). |

## ❌ Missing — not implemented (839)

### Builtin

**Entering Commands:** `commandhistory`, `commandwindow`, `diary`, `more`

**Matrices and Arrays:** `combinations`

**Control Flow:** `parfor`

**Structures:** `struct2table`, `table2struct`

**Cell Arrays:** `cell2table`, `cellplot`, `table`, `table2cell`, `timetable`

**Function Handles:** `function_handle`

**Categorical Arrays:** `addcats`, `categorical`, `categories`, `combinations`, `countcats`, `iscategory`, `isordinal`, `isprotected`, `isundefined`, `mergecats`, `removecats`, `renamecats`, `reordercats`, `setcats`, `summary`

**Tables / Timetables:** `addprop`, `addvars`, `array2table`, `cell2table`, `computebygroup`, `convertvars`, `height`, `inner2outer`, `innerjoin`, `jointables`, `mergevars`, `movevars`, `outerjoin`, `parquetread`, `parquetwrite`, `pivot`, `pivottable`, `readtable`, `removevars`, `renamevars`, `rmprop`, `rowfun`, `rows2vars`, `splitvars`, `stack`, `stackedplot`, `stacktablevariables`, `standardizemissing`, `struct2table`, `summary`, `table`, `table2array`, `table2cell`, `table2struct`, `table2timetable`, `timetable2table`, `unstack`, `unstacktablevariables`, `varfun`, `vartype`, `width`, `writetable`

**Set Operations:** `innerjoin`, `outerjoin`

**Arithmetic:** `tensorprod`

**Random Number Generation:** `randstream`

**Interpolation:** `griddedinterpolant`, `scatteredinterpolant`

**Sparse Matrices:** `amd`, `bicg`, `bicgstab`, `bicgstabl`, `cgs`, `colamd`, `dissect`, `dmperm`, `eigs`, `equilibrate`, `etree`, `etreeplot`, `full`, `gmres`, `gplot`, `ichol`, `ilu`, `lsqr`, `minres`, `nzmax`, `pcg`, `qmr`, `spalloc`, `sparse`, `spaugment`, `spconvert`, `spdiags`, `speye`, `spfun`, `spones`, `spparms`, `sprand`, `sprandn`, `sprandsym`, `sprank`, `spy`, `svds`, `symamd`, `symbfact`, `symmlq`, `tfqmr`, `treelayout`, `treeplot`, `unmesh`

**Workspace:** `openvar`, `workspacebrowser`

**Error Handling (basic):** `oncleanup`

### Communications

**Modulation:** `apskdemod`, `mskdemod`, `amdemod`, `fmdemod`, `pmdemod`, `ssbdemod`

**Sources, Sinks, and Signal Operations:** `zadoffChuSeq`, `mask2shift`, `shift2mask`, `hex2poly`, `oct2poly`, `oct2dec`, `vec2mat`

**Source Coding:** `arithdeco`, `dpcmdeco`, `huffmandeco`

**Error Detection and Correction:** `crcGenerate`, `crcDetect`, `gfweight`, `syndtable`, `bchenc`, `bchdec`, `bchgenpoly`, `bchnumerr`, `rsenc`, `rsdec`, `rsgenpoly`, `rsgenpolycoeffs`, `ldpcEncode`, `ldpcDecode`, `ldpcPCM`, `ldpcQuasiCyclicMatrix`, `tpcenc`, `tpcdec`, `convenc`, `vitdec`

**Trellis and Galois Field Utilities:** `distspec`, `iscatastrophic`, `cosets`, `isprimitive`, `minpol`, `primpoly`, `gfadd`, `gfconv`, `gfcosets`, `gfdeconv`, `gfdiv`, `gffilter`, `gflineq`, `gfminpol`, `gfmul`, `gfpretty`, `gfprimck`, `gfprimdf`, `gftuple`

**Interleaving:** `intrlv`, `deintrlv`, `algintrlv`, `algdeintrlv`, `helscanintrlv`, `helscandeintrlv`, `matintrlv`, `matdeintrlv`, `randintrlv`, `randdeintrlv`, `convintrlv`, `convdeintrlv`, `helintrlv`, `heldeintrlv`, `muxintrlv`, `muxdeintrlv`

**Pulse Shaping, Equalization, MIMO:** `mlseeq`, `ofdmEqualize`, `blkdiagbfweights`, `ofdmPrecode`

**RF and Channel Impairments:** `stdchan`, `frequencyOffset`, `iqimbal`, `iqcoef2imbal`, `iqimbal2coef`, `srmdelay`, `channelDelay`, `ofdmChannelResponse`

**Propagation Path Loss and Geometry:** `fspl`, `cranerainpl`, `rainpl`, `gaspl`, `fogpl`, `raypl`, `buildingMaterialPermittivity`, `earthSurfacePermittivity`, `los`, `doppler`, `rangeangle`, `global2localcoord`, `local2globalcoord`, `cart2sphvec`, `sph2cartvec`

**Performance Analysis:** `bercoding`, `berfading`, `berfit`, `bersync`, `semianalytic`

### Control

**LTI Models:** `dss`, `pid`, `pid2`, `pidstd`, `pidstd2`, `rss`, `drss`, `dssdata`, `piddata`, `pidstddata`

**Model Conversion & Reduction:** `c2dOptions`, `d2cOptions`, `d2d`, `d2dOptions`, `canon`, `balreal`, `prescale`, `modalreal`, `compreal`, `minreal`, `sminreal`, `balred`, `modred`, `hsvd`, `pade`

**Interconnections:** `connect`, `lft`, `sumblk`

**Time and Frequency Response:** `initial`, `lsiminfo`, `gensig`, `covar`, `bodemag`, `nichols`, `sigma`, `getPeakGain`, `getGainCrossover`

**Stability and Margins:** `allmargin`

**State-Space Design and Estimation:** `lqr`, `lqry`, `lqi`, `dlqr`, `lqrd`, `lqg`, `lqgreg`, `lqgtrack`, `estim`, `kalman`, `kalmd`, `reg`, `gram`, `ctrbf`, `obsvf`

**Matrix Equations:** `lyapchol`, `dlyapchol`, `care`, `dare`, `gcare`, `gdare`

**PID Tuning and Modal Analysis:** `pidtune`, `pidtuneOptions`, `getPIDLoopResponse`, `modalsep`, `stabsep`, `freqsep`, `spectralfact`

### Fitting

**Splines:** `bspline`, `csape`, `csaps`, `cscvn`, `rscvn`, `spapi`, `spaps`, `spap2`, `spcrv`, `tpaps`, `rpmak`, `rsmak`, `spmak`, `stmak`, `fn2fm`, `fnchg`, `fndir`, `fnjmp`, `fnmin`, `fnplt`, `fnrfn`, `fntlr`, `fnxtr`, `fnzeros`, `bkbrk`, `slvblk`, `spcol`, `stcol`, `aptknt`, `chbpnt`, `newknt`, `optknt`, `smooth`, `quad2d`

### Graphics

**Line Plots:** `area`, `fimplicit`, `plot3`, `stackedplot`

**Polar Plots:** `compassplot`, `fpolarplot`, `polaraxes`, `polarbubblechart`, `polarhistogram`, `polarregion`, `polarscatter`, `radiusregion`, `rtickangle`, `rtickformat`, `rticklabels`, `rticks`, `thetaregion`, `thetatickformat`, `thetaticklabels`, `thetaticks`

**Contour Plots:** `clabel`, `contour3`, `contourc`, `contourslice`

**Vector Fields:** `compassplot`, `feather`, `quiver`, `quiver3`, `streamline`, `streamslice`

**Surface and Mesh Plots:** `contour3`, `fimplicit3`, `hidden`, `meshc`, `meshz`, `ribbon`, `surf2patch`, `surface`, `surfc`, `surfl`, `surfnorm`, `waterfall`

**Volume Visualization:** `contourslice`, `curl`, `divergence`, `flow`, `interpstreamspeed`, `isocaps`, `isocolors`, `isonormals`, `reducepatch`, `reducevolume`, `shrinkfaces`, `smooth3`, `stream2`, `stream3`, `streamline`, `streamparticles`, `streamribbon`, `streamslice`, `subvolume`, `volumebounds`

**Geographic Plots:** `geoaxes`, `geobasemap`, `geobubble`, `geodensityplot`, `geolimits`, `geoplot`, `geoscatter`, `geotickformat`

### Image

**Image Type Conversion:** `rgb2ind`

**Color Space Conversion:** `imapprox`

**Synthetic Images and Display:** `imshowpair`, `montage`, `immovie`

**Geometric Transformations:** `findbounds`, `fitgeotrans`, `imtransform`, `imwarp`, `makeresampler`

**Image Registration:** `cpcorr`, `imregconfig`, `imregcorr`, `imregdemons`, `imregister`, `imregmtb`, `imregtform`

**Image Filtering:** `gabor`

**Contrast Adjustment:** `decorrstretch`, `imlocalbrighten`, `localcontrast`

**ROI-Based Processing:** `inpaintCoherent`, `inpaintExemplar`, `roifill`

**Morphological Operations:** `bwskel`, `bwulterode`, `bwunpack`, `conndef`, `offsetstrel`

**Deblurring:** `deconvblind`, `deconvlucy`

**Neighborhood and Block Processing:** `blockproc`

**Image Segmentation:** `activecontour`, `bfscore`, `grabcut`, `imseggeodesic`, `imsegfmm`, `imsegisodata`, `imsegkmeans`, `imsegkmeans3`, `lazysnapping`, `superpixels`, `superpixels3`, `watershed`

**Object Analysis:** `circles2mask`, `corner`, `edge3`, `houghpeaks`, `imfindcircles`, `iradon`, `qtdecomp`, `qtgetblk`, `qtsetblk`, `radon`, `visboundaries`, `viscircles`

**Region and Image Properties:** `bwconvhull`, `bwferet`, `bwlabeln`, `bwselect3`, `imcontour`, `impixel`, `improfile`, `poly2label`, `regionprops3`

**Image Quality:** `brisque`, `niqe`, `piqe`

**Image Transforms:** `fan2para`, `fanbeam`, `ifanbeam`, `para2fan`

### IO

**Low-Level File I/O:** `openedfiles`

**Text Files (CSV / dlm / readtable):** `importdatatask`, `importtool`, `readcell`, `readtable`, `readtimetable`, `readvars`, `writecell`, `writetable`, `writetimetable`

**Spreadsheets:** `importdata`, `importdatatask`, `importtool`, `readcell`, `readtable`, `readtimetable`, `readvars`, `sheetnames`, `writecell`, `writetable`, `writetimetable`

**Workspace Save / Load:** `loadobj`, `saveobj`

**File Name Construction:** `filemarker`, `matlabdrive`, `matlabroot`, `toolboxdir`

### Linear Algebra

*(None — 100% complete)*

### ODE

**File Name Construction:** `decic`, `deval`, `ode`, `ode113`, `ode15i`, `ode15s`, `ode23s`, `ode23t`, `ode23tb`, `ode78`, `ode89`, `odeevent`, `odejacobian`, `odemassmatrix`, `odesensitivity`, `odextend`, `solveode`

### Optimization

**Local:** `optimize`

**Constrained:** `fmincon`, `fminunc`, `fseminf`, `fgoalattain`, `fminimax`, `linprog`, `intlinprog`, `quadprog`, `coneprog`, `secondordercone`, `lsqlin`, `lsqcurvefit`, `lsqnonlin`, `fsolve`, `mpsread`, `optimoptions`, `resetoptions`, `checkGradients`, `optimwarmstart`, `integerConstraint`

**Global:** `ga`, `gamultiobj`, `paretosearch`, `particleswarm`, `patternsearch`, `simulannealbnd`, `surrogateopt`, `packfcn`, `gaoptimset`, `gaoptimget`, `psoptimset`, `psoptimget`, `saoptimset`, `saoptimget`

### Signal

**Waveform Generation:** `buffer`, `demod`, `framelbl`, `framesig`, `modulate`, `shiftdata`, `udecode`, `uencode`, `unshiftdata`, `vco`

**Filter Design:** `cfirpm`, `designfilt`, `designfilter`, `digitalfilter`, `dspfwiz`, `filt2block`, `filteranalyzer`, `fircls`, `fircls1`, `info`, `isdouble`, `maxflat`, `polyscale`, `polystab`, `scalefiltersections`, `yulewalk`

**Digital Filter Analysis:** `filteranalyzer`, `zplane`

**Digital Filtering:** `cell2sos`, `ctf2zp`, `ctffilt`, `dspfwiz`, `eqtflength`, `filt2block`, `filtic`, `latc2tf`, `latcfilt`, `scalefiltersections`, `sos2cell`, `sos2ctf`, `tf2latc`, `zp2ctf`

**Multirate Signal Processing:** `fillgaps`

**Signal Modeling:** `stmcb`

**Correlation and Convolution:** `dtw`, `edr`, `findsignal`

**Transforms:** `digitrevorder`, `dlistft`, `dlstft`, `emd`, `fsst`, `hht`, `ifsst`, `istftlayer`, `pspectrum`, `stftlayer`, `stftmag2sig`, `vmd`, `wvd`, `xspectrogram`, `xwvd`, `fftw`, `nufft`, `nufftn`

**Windows:** `dpss`, `dpssclear`, `dpssdir`, `dpssload`, `dpsssave`, `wvtool`

**Parametric Spectral Estimation:** `pcov`, `pmcov`

**Nonparametric Spectral Estimation:** `plomb`, `pmtm`, `poctave`, `pspectrum`, `refinepeaks`

**Spectral Measurements:** `toi`

**Time-Frequency Analysis:** `dlistft`, `dlstft`, `emd`, `fsst`, `hht`, `ifsst`, `istftlayer`, `kurtogram`, `pspectrum`, `stftlayer`, `stftmag2sig`, `tfridge`, `vmd`, `wvd`, `xspectrogram`, `xwvd`

**Signal Descriptive Statistics:** `binmask2sigroi`, `countlabels`, `dtw`, `edr`, `extendsigroi`, `extractsigroi`, `filenames2labels`, `findchangepts`, `findsignal`, `folders2labels`, `framelbl`, `framesig`, `mergesigroi`, `removesigroi`, `seqperiod`, `shortensigroi`, `sigrangebinmask`, `sigroi2binmask`, `splitlabels`, `zerocrossrate`

**Vibration Analysis:** `modalfit`, `modalfrf`, `modalsd`, `orderspectrum`, `ordertrack`, `orderwaveform`, `rpmfreqmap`, `rpmordermap`, `rpmtrack`

### Audio

**Audio Feature Extraction:** `pitchnn`

### Statistics

**Descriptive Statistics:** `summary`

**Distribution Fitting (MLE / likelihood):** `mlecov`

**Multivariate Distributions:** `copulafit`, `copulaparam`, `copulastat`, `copularnd`

**Pearson / Johnson Distributions:** `pearspdf`, `pearscdf`, `pearsinv`, `pearsrnd`, `johnsrnd`

**Empirical / Kernel Distributions:** `mvksdensity`

**Hypothesis Tests:** `barttest`, `friedman`, `knntest`, `meanEffectSize`, `mmdtest`, `sampsizepwr`

**Resampling Techniques:** `cvpartition`

**Quasirandom Sequences and MCMC:** `mhsample`, `qrandstream`, `slicesample`, `sobolset`, `qrand`

**ANOVA / MANOVA / Correlation:** `anovan`, `manova1`, `aoctool`, `mauchly`, `epsilon`

**Linear Regression (function-form):** `stepwisefit`, `mvregress`, `mvregresslike`, `plsregress`, `polyconf`

**Nonlinear Regression (function-form):** `statset`, `statget`

**Hierarchical Clustering:** `dendrogram`, `optimalleaforder`

**Partitional Clustering:** `spectralcluster`

**Cluster Evaluation:** `evalclusters`, `manovacluster`

**Nearest Neighbors (function-form):** `createns`

**Hidden Markov Models:** `hmmdecode`, `hmmestimate`, `hmmgenerate`, `hmmtrain`, `hmmviterbi`

**Dimensionality Reduction:** `ppca`, `factoran`, `rica`, `sparsefilt`, `tsne`

**Feature Selection (function-form):** `fscchi2`, `fscmrmr`, `fscnca`, `fsrftest`, `fsrmrmr`, `fsrnca`, `fsulaplacian`, `relieff`, `sequentialfs`

### Wavelet

**Continuous Wavelet Transforms:** `cwt`, `icwt`, `cwtfreqbounds`, `centfrq`, `scal2frq`, `wcoherence`, `wsst`, `iwsst`, `wsstridge`, `wtmm`, `wavefun`, `wavefun2`, `wavsupport`, `qfactor`, `wavemngr`, `waveinfo`

**Discrete Wavelet Transforms (1-D):** `dwtmode`, `dwpt`, `idwpt`

**Discrete Wavelet Transforms (2-D / 3-D):** `wavedec2`, `waverec2`, `appcoef2`, `detcoef2`, `wrcoef2`, `wpdec2`, `wprec2`, `haart2`, `ihaart2`, `wavedec3`, `waverec3`, `dwt3`, `idwt3`

**Stationary, MODWT, and Wavelet Packets:** `swt2`, `iswt2`, `modwtmra`, `modwtcorr`, `modwtvar`, `modwtxcorr`, `modwpt`, `imodwpt`, `wpdec`, `wprec`, `wpcoef`, `wprcoef`, `besttree`

**Denoising and Compression:** `wdenoise2`, `wden`, `wdencmp`, `wpdencmp`, `wvarchg`, `ddencmp`, `thselect`, `wthcoef`, `wthcoef2`, `wmulden`, `measerr`, `wnoise`, `wcompress`

**Filter Banks and Wavelet Families:** `biorfilt`, `dbaux`, `symaux`, `biorwavf`, `rbiowavf`, `fejerkorovkin`, `mbscalf`, `hanscalf`, `blscalf`, `bswfun`, `isbiorthwfb`, `isorthwfb`, `wavelets`, `waveletfamilies`, `wavenames`

**Continuous Wavelet Shapes:** `meyer`, `intwave`, `pat2cwav`

**Lifting:** `lwt`, `ilwt`, `lwt2`, `ilwt2`, `lwtcoef`, `lwtcoef2`

**Decomposition Trees and Misc:** `dualtree`, `idualtree`, `dualtree2`, `idualtree2`, `dddtree`, `idddtree`, `tqwt`, `itqwt`, `wfbm`, `wfbmesti`, `wfusimg`, `wfusmat`, `wentropy`
