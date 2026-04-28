# numkit MATLAB Parity TODO

Cross-reference of MATLAB built-ins ([core](https://www.mathworks.com/help/matlab/referencelist.html)
+ [Signal Processing Toolbox](https://www.mathworks.com/help/signal/referencelist.html))
against numkit-m's current implementation, derived from **scraped section pages**
on web.archive.org (Sep 2024 snapshot, R2024b).

**Source data:** 1226 doc entries → 1083 unique function names
after stripping doc-page class prefixes (e.g. `double.plus` is shown as `plus`)
and filtering out 195 OOP class-path entries.
**numkit detection scope:** 336 registered builtins + 19 language keywords +
10 pre-bound constants + 24 operator-as-function names.

**Skipped sections** (out of scope per project): cdflib / matlab.io / netcdf, multimedia,
hardware/network communication, web access, large files / big data, tall arrays, mapreduce,
parquet, graph + network algorithms, computational geometry, quantum, background + parallel,
security, app building, external language interfaces, settings API. The MATLAB-specific
"Functions" and "Classes" sections (about defining function/class scopes) and Simulink-specific
functions are also skipped. Per-function OOP class methods (anything matching
`<class>.<method>` such as `matlab.io.*` or `mexception.addcause`, plus the R2020b
string-Pattern OOP family `*pattern` / `*boundary`, signal-toolbox OOP classes
like `signalMask` / `sigwin` / `labeledSignalSet` and `*-app` GUI launchers) are filtered too.

**Legend:**
- ✅ implemented (registered builtin, language keyword, or pre-bound constant)
- ⚠️ works as operator/partial form, but not callable under MATLAB-doc name
- ❌ not implemented

---

## 1. Language Fundamentals

### Entering Commands — 2 ✅ + 0 ⚠️ / 9 = 22%

| Function | Status | Notes |
|---|:---:|---|
| `ans` | ✅ | implicit-assigned unsuppressed result |
| `clc` | ✅ |  |
| `commandhistory` | ❌ | IDE-only |
| `commandwindow` | ❌ | IDE-only |
| `diary` | ❌ | session log |
| `format` | ❌ | output format |
| `home` | ❌ | terminal home |
| `iskeyword` | ❌ | introspection |
| `more` | ❌ | pager |

### Matrices and Arrays — 33 ✅ + 3 ⚠️ / 55 = 65%

| Function | Status | Notes |
|---|:---:|---|
| `blkdiag` | ✅ |  |
| `cat` | ✅ |  |
| `circshift` | ✅ |  |
| `colon` | ⚠️ | works as `:` (range) operator; not callable as named fn |
| `combinations` | ❌ | all combinations |
| `ctranspose` | ⚠️ | works as postfix `'` operator; not callable as named fn |
| `diag` | ✅ |  |
| `end` | ✅ | keyword + `A(end)` indexing form |
| `eye` | ✅ |  |
| `false` | ✅ | literal/constant |
| `flip` | ❌ | general N-D flip |
| `fliplr` | ✅ |  |
| `flipud` | ✅ |  |
| `freqspace` | ❌ |  |
| `head` | ❌ |  |
| `horzcat` | ✅ |  |
| `ind2sub` | ❌ | linear-index conv |
| `ipermute` | ✅ |  |
| `iscolumn` | ❌ | predicate |
| `isempty` | ✅ |  |
| `ismatrix` | ❌ | predicate |
| `isrow` | ❌ | predicate |
| `isscalar` | ✅ |  |
| `issorted` | ❌ | check sorted |
| `issortedrows` | ❌ |  |
| `isuniform` | ❌ | uniform-spacing test |
| `isvector` | ❌ | predicate |
| `length` | ✅ |  |
| `linspace` | ✅ |  |
| `logspace` | ✅ |  |
| `meshgrid` | ✅ |  |
| `ndgrid` | ✅ |  |
| `ndims` | ✅ |  |
| `numel` | ✅ |  |
| `ones` | ✅ |  |
| `paddata` | ❌ | pad N-D |
| `permute` | ✅ |  |
| `rand` | ✅ |  |
| `repelem` | ❌ |  |
| `repmat` | ✅ |  |
| `reshape` | ✅ |  |
| `resize` | ❌ | general resize |
| `rot90` | ✅ |  |
| `shiftdim` | ❌ |  |
| `size` | ✅ |  |
| `sort` | ✅ |  |
| `sortrows` | ✅ |  |
| `squeeze` | ✅ |  |
| `sub2ind` | ❌ | linear-index conv |
| `tail` | ❌ |  |
| `transpose` | ⚠️ | works as postfix `.'` operator; not callable as named fn |
| `trimdata` | ❌ |  |
| `true` | ✅ | literal/constant |
| `vertcat` | ✅ |  |
| `zeros` | ✅ |  |

### Control Flow — 9 ✅ + 0 ⚠️ / 11 = 81%

| Function | Status | Notes |
|---|:---:|---|
| `break` | ✅ | keyword |
| `continue` | ✅ | keyword |
| `end` | ✅ | keyword + `A(end)` indexing form |
| `for` | ✅ | keyword |
| `if` | ✅ | keyword |
| `parfor` | ❌ | parallel — out of scope |
| `pause` | ❌ | no time.sleep |
| `return` | ✅ | keyword |
| `switch` | ✅ | keyword (`switch/case/otherwise`) |
| `try` | ✅ | keyword (`try/catch`) |
| `while` | ✅ | keyword |

### Numeric Types — 20 ✅ + 0 ⚠️ / 29 = 68%

| Function | Status | Notes |
|---|:---:|---|
| `allfinite` | ❌ | whole-array `all(isfinite)` |
| `anynan` | ❌ | whole-array `any(isnan)` |
| `cast` | ❌ | type conversion |
| `double` | ✅ |  |
| `eps` | ✅ | constant (machine eps) |
| `flintmax` | ❌ | largest exact float-int |
| `inf` | ✅ | constant |
| `int16` | ✅ |  |
| `int32` | ✅ |  |
| `int64` | ✅ |  |
| `int8` | ✅ |  |
| `intmax` | ❌ | max int per type |
| `intmin` | ❌ | min int per type |
| `isfinite` | ✅ |  |
| `isfloat` | ✅ |  |
| `isinf` | ✅ |  |
| `isinteger` | ✅ |  |
| `isnan` | ✅ |  |
| `isnumeric` | ✅ |  |
| `isreal` | ✅ |  |
| `nan` | ✅ | constant |
| `realmax` | ❌ | largest finite double |
| `realmin` | ❌ | smallest normal double |
| `single` | ✅ |  |
| `typecast` | ❌ | reinterpret bytes |
| `uint16` | ✅ |  |
| `uint32` | ✅ |  |
| `uint64` | ✅ |  |
| `uint8` | ✅ |  |

### Characters and Strings — 22 ✅ + 1 ⚠️ / 65 = 35%

| Function | Status | Notes |
|---|:---:|---|
| `append` | ❌ |  |
| `blanks` | ❌ |  |
| `cellstr` | ❌ | cell of char rows |
| `char` | ✅ |  |
| `compose` | ❌ |  |
| `contains` | ✅ |  |
| `convertcharstostrings` | ❌ |  |
| `convertcontainedstringstochars` | ❌ |  |
| `convertstringstochars` | ❌ |  |
| `count` | ❌ |  |
| `deblank` | ❌ |  |
| `double` | ✅ |  |
| `endswith` | ❌ |  |
| `erase` | ❌ |  |
| `erasebetween` | ❌ |  |
| `extract` | ❌ |  |
| `extractafter` | ❌ |  |
| `extractbefore` | ❌ |  |
| `extractbetween` | ❌ |  |
| `insertafter` | ❌ |  |
| `insertbefore` | ❌ |  |
| `iscellstr` | ❌ | predicate |
| `ischar` | ✅ |  |
| `isletter` | ❌ |  |
| `isspace` | ❌ |  |
| `isstring` | ✅ |  |
| `isstringscalar` | ❌ |  |
| `isstrprop` | ❌ |  |
| `join` | ❌ |  |
| `lower` | ✅ |  |
| `matches` | ❌ |  |
| `newline` | ❌ |  |
| `num2str` | ✅ |  |
| `pad` | ❌ |  |
| `plus` | ⚠️ | works as binary `+` operator; not callable as named fn |
| `regexp` | ✅ |  |
| `regexpi` | ✅ |  |
| `regexprep` | ✅ |  |
| `regexptranslate` | ❌ |  |
| `replace` | ❌ |  |
| `replacebetween` | ❌ |  |
| `reverse` | ❌ |  |
| `split` | ❌ |  |
| `splitlines` | ❌ |  |
| `sprintf` | ✅ |  |
| `sscanf` | ✅ |  |
| `startswith` | ❌ |  |
| `str2double` | ✅ |  |
| `strcat` | ✅ |  |
| `strcmp` | ✅ |  |
| `strcmpi` | ✅ |  |
| `strfind` | ❌ |  |
| `string` | ✅ |  |
| `strings` | ❌ |  |
| `strip` | ❌ |  |
| `strjoin` | ❌ |  |
| `strjust` | ❌ |  |
| `strlength` | ✅ |  |
| `strncmp` | ❌ |  |
| `strncmpi` | ❌ |  |
| `strrep` | ✅ |  |
| `strsplit` | ✅ |  |
| `strtok` | ❌ |  |
| `strtrim` | ✅ |  |
| `upper` | ✅ |  |

### Structures — 7 ✅ + 0 ⚠️ / 14 = 50%

| Function | Status | Notes |
|---|:---:|---|
| `arrayfun` | ✅ |  |
| `cell2struct` | ❌ |  |
| `fieldnames` | ✅ |  |
| `getfield` | ❌ | dynamic field |
| `isfield` | ✅ |  |
| `isstruct` | ✅ |  |
| `orderfields` | ❌ | reorder |
| `rmfield` | ✅ |  |
| `setfield` | ❌ | dynamic field |
| `struct` | ✅ |  |
| `struct2cell` | ❌ |  |
| `struct2table` | ❌ |  |
| `structfun` | ✅ |  |
| `table2struct` | ❌ |  |

### Cell Arrays — 4 ✅ + 0 ⚠️ / 17 = 23%

| Function | Status | Notes |
|---|:---:|---|
| `cell` | ✅ |  |
| `cell2mat` | ❌ | concat cells |
| `cell2struct` | ❌ |  |
| `cell2table` | ❌ |  |
| `celldisp` | ❌ |  |
| `cellfun` | ✅ |  |
| `cellplot` | ❌ |  |
| `cellstr` | ❌ | cell of char rows |
| `iscell` | ✅ |  |
| `iscellstr` | ❌ | predicate |
| `mat2cell` | ❌ | split into cell |
| `num2cell` | ❌ | wrap each elem |
| `string` | ✅ |  |
| `struct2cell` | ❌ |  |
| `table` | ❌ |  |
| `table2cell` | ❌ |  |
| `timetable` | ❌ |  |

### Function Handles — 0 ✅ + 0 ⚠️ / 6 = 0%

| Function | Status | Notes |
|---|:---:|---|
| `feval` | ❌ | call handle by name |
| `func2str` | ❌ | inspect |
| `function_handle` | ❌ | OOP class |
| `functions` | ❌ | introspection |
| `localfunctions` | ❌ |  |
| `str2func` | ❌ | create handle |

### Categorical Arrays — 1 ✅ + 0 ⚠️ / 17 = 5%

| Function | Status | Notes |
|---|:---:|---|
| `addcats` | ❌ |  |
| `categorical` | ❌ |  |
| `categories` | ❌ |  |
| `combinations` | ❌ | all combinations |
| `countcats` | ❌ |  |
| `discretize` | ✅ |  |
| `iscategorical` | ❌ |  |
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

### Tables / Timetables — 6 ✅ + 0 ⚠️ / 66 = 9%

| Function | Status | Notes |
|---|:---:|---|
| `addprop` | ❌ |  |
| `addvars` | ❌ |  |
| `anymissing` | ❌ |  |
| `array2table` | ❌ |  |
| `cell2table` | ❌ |  |
| `computebygroup` | ❌ |  |
| `convertvars` | ❌ |  |
| `fillmissing` | ❌ |  |
| `findgroups` | ❌ |  |
| `groupcounts` | ❌ |  |
| `groupfilter` | ❌ |  |
| `groupsummary` | ❌ |  |
| `grouptransform` | ❌ |  |
| `head` | ❌ |  |
| `height` | ❌ |  |
| `inner2outer` | ❌ |  |
| `innerjoin` | ❌ |  |
| `intersect` | ✅ |  |
| `ismember` | ✅ |  |
| `ismissing` | ❌ |  |
| `issortedrows` | ❌ |  |
| `istable` | ❌ |  |
| `istabular` | ❌ |  |
| `join` | ❌ |  |
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
| `rmmissing` | ❌ |  |
| `rmprop` | ❌ |  |
| `rowfun` | ❌ |  |
| `rows2vars` | ❌ |  |
| `setdiff` | ✅ |  |
| `setxor` | ❌ | symmetric set diff |
| `sortrows` | ✅ |  |
| `splitapply` | ❌ |  |
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
| `tail` | ❌ |  |
| `timetable2table` | ❌ |  |
| `topkrows` | ❌ |  |
| `union` | ✅ |  |
| `unique` | ✅ |  |
| `unstack` | ❌ |  |
| `unstacktablevariables` | ❌ |  |
| `varfun` | ❌ |  |
| `vartype` | ❌ |  |
| `width` | ❌ |  |
| `writetable` | ❌ | needs table type |

### Bit-wise Operations — 5 ✅ + 0 ⚠️ / 8 = 62%

| Function | Status | Notes |
|---|:---:|---|
| `bitand` | ✅ |  |
| `bitcmp` | ✅ |  |
| `bitget` | ❌ | get bit |
| `bitor` | ✅ |  |
| `bitset` | ❌ | set bit |
| `bitshift` | ✅ |  |
| `bitxor` | ✅ |  |
| `swapbytes` | ❌ |  |

### Set Operations — 5 ✅ + 0 ⚠️ / 13 = 38%

| Function | Status | Notes |
|---|:---:|---|
| `allunique` | ❌ | distinct check |
| `innerjoin` | ❌ |  |
| `intersect` | ✅ |  |
| `ismember` | ✅ |  |
| `ismembertol` | ❌ | tol variant |
| `join` | ❌ |  |
| `numunique` | ❌ | count distinct |
| `outerjoin` | ❌ |  |
| `setdiff` | ✅ |  |
| `setxor` | ❌ | symmetric set diff |
| `union` | ✅ |  |
| `unique` | ✅ |  |
| `uniquetol` | ❌ | tol variant |

## 2. Mathematics

### Arithmetic — 12 ✅ + 14 ⚠️ / 34 = 76%

| Function | Status | Notes |
|---|:---:|---|
| `bsxfun` | ❌ | legacy broadcast |
| `ceil` | ✅ |  |
| `ctranspose` | ⚠️ | works as postfix `'` operator; not callable as named fn |
| `cumprod` | ✅ |  |
| `cumsum` | ✅ |  |
| `diff` | ✅ |  |
| `fix` | ✅ |  |
| `floor` | ✅ |  |
| `idivide` | ❌ | integer division |
| `ldivide` | ⚠️ | works as elementwise `.\` operator; not callable as named fn |
| `minus` | ⚠️ | works as binary `-` operator; not callable as named fn |
| `mldivide` | ⚠️ | works as matrix `\` operator; not callable as named fn |
| `mod` | ✅ |  |
| `movsum` | ❌ | moving sum |
| `mpower` | ⚠️ | works as matrix `^` operator; not callable as named fn |
| `mrdivide` | ⚠️ | works as matrix `/` operator; not callable as named fn |
| `mtimes` | ⚠️ | works as matrix `*` operator; not callable as named fn |
| `pagectranspose` | ❌ |  |
| `pagemldivide` | ❌ |  |
| `pagemrdivide` | ❌ |  |
| `pagemtimes` | ✅ |  |
| `pagetranspose` | ❌ |  |
| `plus` | ⚠️ | works as binary `+` operator; not callable as named fn |
| `power` | ⚠️ | works as elementwise `.^` operator; not callable as named fn |
| `prod` | ✅ |  |
| `rdivide` | ⚠️ | works as elementwise `./` operator; not callable as named fn |
| `rem` | ✅ |  |
| `round` | ✅ |  |
| `sum` | ✅ |  |
| `tensorprod` | ❌ | tensor contraction |
| `times` | ⚠️ | works as elementwise `.*` operator; not callable as named fn |
| `transpose` | ⚠️ | works as postfix `.'` operator; not callable as named fn |
| `uminus` | ⚠️ | works as unary `-` operator; not callable as named fn |
| `uplus` | ⚠️ | works as unary `+` operator; not callable as named fn |

### Trigonometry — 10 ✅ + 0 ⚠️ / 47 = 21%

| Function | Status | Notes |
|---|:---:|---|
| `acos` | ✅ |  |
| `acosd` | ❌ | degree |
| `acosh` | ❌ | hyperbolic |
| `acot` | ❌ |  |
| `acotd` | ❌ |  |
| `acoth` | ❌ |  |
| `acsc` | ❌ |  |
| `acscd` | ❌ |  |
| `acsch` | ❌ |  |
| `asec` | ❌ |  |
| `asecd` | ❌ |  |
| `asech` | ❌ |  |
| `asin` | ✅ |  |
| `asind` | ❌ | degree |
| `asinh` | ❌ | hyperbolic |
| `atan` | ✅ |  |
| `atan2` | ✅ |  |
| `atan2d` | ❌ | degree |
| `atand` | ❌ | degree |
| `atanh` | ❌ | hyperbolic |
| `cart2pol` | ❌ | coord xform |
| `cart2sph` | ❌ | coord xform |
| `cos` | ✅ |  |
| `cosd` | ❌ | degree |
| `cosh` | ❌ | hyperbolic |
| `cospi` | ❌ | use `cos(pi*x)` |
| `cot` | ❌ | reciprocal |
| `cotd` | ❌ |  |
| `coth` | ❌ |  |
| `csc` | ❌ | reciprocal |
| `cscd` | ❌ |  |
| `csch` | ❌ |  |
| `deg2rad` | ✅ |  |
| `hypot` | ✅ |  |
| `pol2cart` | ❌ | coord xform |
| `rad2deg` | ✅ |  |
| `sec` | ❌ | reciprocal |
| `secd` | ❌ |  |
| `sech` | ❌ |  |
| `sin` | ✅ |  |
| `sind` | ❌ | degree |
| `sinh` | ❌ | hyperbolic |
| `sinpi` | ❌ | use `sin(pi*x)` |
| `sph2cart` | ❌ | coord xform |
| `tan` | ✅ |  |
| `tand` | ❌ | degree |
| `tanh` | ❌ | hyperbolic |

### Exponents and Logarithms — 9 ✅ + 0 ⚠️ / 13 = 69%

| Function | Status | Notes |
|---|:---:|---|
| `exp` | ✅ |  |
| `expm1` | ✅ |  |
| `log` | ✅ |  |
| `log10` | ✅ |  |
| `log1p` | ✅ |  |
| `log2` | ✅ |  |
| `nextpow2` | ✅ |  |
| `nthroot` | ✅ |  |
| `pow2` | ❌ |  |
| `reallog` | ❌ |  |
| `realpow` | ❌ |  |
| `realsqrt` | ❌ |  |
| `sqrt` | ✅ |  |

### Special Functions — 5 ✅ + 0 ⚠️ / 24 = 20%

| Function | Status | Notes |
|---|:---:|---|
| `airy` | ❌ |  |
| `besselh` | ❌ |  |
| `besseli` | ❌ |  |
| `besselj` | ❌ |  |
| `besselk` | ❌ |  |
| `bessely` | ❌ |  |
| `beta` | ❌ |  |
| `betainc` | ❌ |  |
| `betaincinv` | ❌ |  |
| `betaln` | ❌ |  |
| `ellipj` | ❌ |  |
| `ellipke` | ❌ |  |
| `erf` | ✅ |  |
| `erfc` | ✅ |  |
| `erfcinv` | ❌ |  |
| `erfcx` | ❌ |  |
| `erfinv` | ✅ |  |
| `expint` | ❌ |  |
| `gamma` | ✅ |  |
| `gammainc` | ❌ |  |
| `gammaincinv` | ❌ |  |
| `gammaln` | ✅ |  |
| `legendre` | ❌ |  |
| `psi` | ❌ |  |

### Discrete Math — 8 ✅ + 0 ⚠️ / 11 = 72%

| Function | Status | Notes |
|---|:---:|---|
| `factor` | ✅ |  |
| `factorial` | ✅ |  |
| `gcd` | ✅ |  |
| `isprime` | ✅ |  |
| `lcm` | ✅ |  |
| `matchpairs` | ❌ |  |
| `nchoosek` | ✅ |  |
| `perms` | ✅ |  |
| `primes` | ✅ |  |
| `rat` | ❌ |  |
| `rats` | ❌ |  |

### Polynomials — 7 ✅ + 0 ⚠️ / 12 = 58%

| Function | Status | Notes |
|---|:---:|---|
| `conv` | ✅ |  |
| `deconv` | ✅ |  |
| `poly` | ❌ | roots → coeffs |
| `polyder` | ✅ |  |
| `polydiv` | ❌ |  |
| `polyeig` | ❌ | poly eig |
| `polyfit` | ✅ |  |
| `polyint` | ✅ |  |
| `polyval` | ✅ |  |
| `polyvalm` | ❌ | matrix poly eval |
| `residue` | ❌ | partial-fraction |
| `roots` | ✅ |  |

### Linear Algebra — 6 ✅ + 6 ⚠️ / 82 = 14%

| Function | Status | Notes |
|---|:---:|---|
| `balance` | ❌ | **deferred — libs/linalg** |
| `bandwidth` | ❌ | **deferred — libs/linalg** |
| `cdf2rdf` | ❌ | **deferred — libs/linalg** |
| `chol` | ❌ | **deferred — libs/linalg** |
| `cholupdate` | ❌ |  |
| `cond` | ❌ | **deferred — libs/linalg** |
| `condeig` | ❌ | **deferred — libs/linalg** |
| `condest` | ❌ | **deferred — libs/linalg** |
| `cross` | ✅ |  |
| `ctranspose` | ⚠️ | works as postfix `'` operator; not callable as named fn |
| `decomposition` | ❌ | **deferred — libs/linalg** |
| `det` | ❌ | **deferred — libs/linalg** |
| `dot` | ✅ |  |
| `eig` | ❌ | **deferred — libs/linalg** |
| `eigs` | ❌ | **deferred — libs/linalg** |
| `expm` | ❌ | **deferred — libs/linalg** |
| `expmv` | ❌ |  |
| `funm` | ❌ | **deferred — libs/linalg** |
| `gsvd` | ❌ | **deferred — libs/linalg** |
| `hess` | ❌ | **deferred — libs/linalg** |
| `inv` | ❌ | **deferred — libs/linalg** |
| `isbanded` | ❌ | **deferred — libs/linalg** |
| `isdiag` | ❌ |  |
| `ishermitian` | ❌ | **deferred — libs/linalg** |
| `issymmetric` | ❌ | **deferred — libs/linalg** |
| `istril` | ❌ | **deferred — libs/linalg** |
| `istriu` | ❌ | **deferred — libs/linalg** |
| `kron` | ✅ |  |
| `ldl` | ❌ | **deferred — libs/linalg** |
| `linsolve` | ❌ | **deferred — libs/linalg** |
| `logm` | ❌ | **deferred — libs/linalg** |
| `lscov` | ❌ |  |
| `lsqminnorm` | ❌ | **deferred — libs/linalg** |
| `lsqnonneg` | ❌ |  |
| `lu` | ❌ | **deferred — libs/linalg** |
| `mldivide` | ⚠️ | works as matrix `\` operator; not callable as named fn |
| `mpower` | ⚠️ | works as matrix `^` operator; not callable as named fn |
| `mrdivide` | ⚠️ | works as matrix `/` operator; not callable as named fn |
| `mtimes` | ⚠️ | works as matrix `*` operator; not callable as named fn |
| `norm` | ❌ | **deferred — libs/linalg** |
| `normest` | ❌ | **deferred — libs/linalg** |
| `null` | ❌ | **deferred — libs/linalg** |
| `ordeig` | ❌ | **deferred — libs/linalg** |
| `ordqz` | ❌ | **deferred — libs/linalg** |
| `ordschur` | ❌ | **deferred — libs/linalg** |
| `orth` | ❌ | **deferred — libs/linalg** |
| `pagectranspose` | ❌ |  |
| `pageeig` | ❌ |  |
| `pageinv` | ❌ |  |
| `pagelsqminnorm` | ❌ |  |
| `pagemldivide` | ❌ |  |
| `pagemrdivide` | ❌ |  |
| `pagemtimes` | ✅ |  |
| `pagenorm` | ❌ |  |
| `pagepinv` | ❌ |  |
| `pagesvd` | ❌ |  |
| `pagetranspose` | ❌ |  |
| `pinv` | ❌ | **deferred — libs/linalg** |
| `planerot` | ❌ | **deferred — libs/linalg** |
| `polyeig` | ❌ | poly eig |
| `qr` | ❌ | **deferred — libs/linalg** |
| `qrdelete` | ❌ |  |
| `qrinsert` | ❌ |  |
| `qrupdate` | ❌ |  |
| `qz` | ❌ | **deferred — libs/linalg** |
| `rank` | ❌ | **deferred — libs/linalg** |
| `rcond` | ❌ | **deferred — libs/linalg** |
| `rref` | ❌ |  |
| `rsf2csf` | ❌ | **deferred — libs/linalg** |
| `schur` | ❌ | **deferred — libs/linalg** |
| `sqrtm` | ❌ | **deferred — libs/linalg** |
| `subspace` | ❌ | **deferred — libs/linalg** |
| `svd` | ❌ | **deferred — libs/linalg** |
| `svdappend` | ❌ |  |
| `svds` | ❌ | **deferred — libs/sparse** |
| `svdsketch` | ❌ |  |
| `sylvester` | ❌ | **deferred — libs/linalg** |
| `trace` | ❌ | **deferred — libs/linalg** |
| `transpose` | ⚠️ | works as postfix `.'` operator; not callable as named fn |
| `tril` | ✅ |  |
| `triu` | ✅ |  |
| `vecnorm` | ❌ | **deferred — libs/linalg** |

### Random Number Generation — 5 ✅ + 0 ⚠️ / 6 = 83%

| Function | Status | Notes |
|---|:---:|---|
| `rand` | ✅ |  |
| `randi` | ✅ |  |
| `randn` | ✅ |  |
| `randperm` | ✅ |  |
| `randstream` | ❌ |  |
| `rng` | ✅ |  |

### Interpolation — 8 ✅ + 0 ⚠️ / 18 = 44%

| Function | Status | Notes |
|---|:---:|---|
| `griddata` | ❌ |  |
| `griddatan` | ❌ |  |
| `griddedinterpolant` | ❌ |  |
| `interp1` | ✅ |  |
| `interp2` | ✅ |  |
| `interp3` | ✅ |  |
| `interpft` | ❌ |  |
| `interpn` | ✅ |  |
| `makima` | ❌ |  |
| `meshgrid` | ✅ |  |
| `mkpp` | ❌ |  |
| `ndgrid` | ✅ |  |
| `padecoef` | ❌ |  |
| `pchip` | ✅ |  |
| `ppval` | ❌ |  |
| `scatteredinterpolant` | ❌ |  |
| `spline` | ✅ |  |
| `unmkpp` | ❌ |  |

### Optimization — 1 ✅ + 0 ⚠️ / 7 = 14%

| Function | Status | Notes |
|---|:---:|---|
| `fminbnd` | ❌ | 1-D bounded |
| `fminsearch` | ❌ | Nelder-Mead |
| `fzero` | ✅ |  |
| `lsqnonneg` | ❌ |  |
| `optimget` | ❌ |  |
| `optimize` | ❌ |  |
| `optimset` | ❌ |  |

### Ordinary Differential Equations — 0 ✅ + 0 ⚠️ / 21 = 0%

| Function | Status | Notes |
|---|:---:|---|
| `decic` | ❌ | **deferred — libs/ode** |
| `deval` | ❌ | **deferred — libs/ode** |
| `ode` | ❌ |  |
| `ode113` | ❌ | **deferred — libs/ode** |
| `ode15i` | ❌ | **deferred — libs/ode** |
| `ode15s` | ❌ | **deferred — libs/ode** |
| `ode23` | ❌ | **deferred — libs/ode** |
| `ode23s` | ❌ | **deferred — libs/ode** |
| `ode23t` | ❌ | **deferred — libs/ode** |
| `ode23tb` | ❌ | **deferred — libs/ode** |
| `ode45` | ❌ | **deferred — libs/ode** |
| `ode78` | ❌ | **deferred — libs/ode** |
| `ode89` | ❌ | **deferred — libs/ode** |
| `odeevent` | ❌ |  |
| `odeget` | ❌ | **deferred — libs/ode** |
| `odejacobian` | ❌ |  |
| `odemassmatrix` | ❌ |  |
| `odesensitivity` | ❌ |  |
| `odeset` | ❌ | **deferred — libs/ode** |
| `odextend` | ❌ | **deferred — libs/ode** |
| `solveode` | ❌ |  |

### Sparse Matrices — 4 ✅ + 0 ⚠️ / 53 = 7%

| Function | Status | Notes |
|---|:---:|---|
| `amd` | ❌ | **deferred — libs/sparse** |
| `bicg` | ❌ | **deferred — libs/sparse** |
| `bicgstab` | ❌ | **deferred — libs/sparse** |
| `bicgstabl` | ❌ | **deferred — libs/sparse** |
| `cgs` | ❌ | **deferred — libs/sparse** |
| `colamd` | ❌ | **deferred — libs/sparse** |
| `colperm` | ❌ |  |
| `condest` | ❌ | **deferred — libs/linalg** |
| `dissect` | ❌ | **deferred — libs/sparse** |
| `dmperm` | ❌ |  |
| `eigs` | ❌ | **deferred — libs/linalg** |
| `equilibrate` | ❌ |  |
| `etree` | ❌ | **deferred — libs/sparse** |
| `etreeplot` | ❌ | **deferred — libs/sparse** |
| `find` | ✅ |  |
| `full` | ❌ | **deferred — libs/sparse** |
| `gmres` | ❌ | **deferred — libs/sparse** |
| `gplot` | ❌ | **deferred — libs/sparse** |
| `ichol` | ❌ |  |
| `ilu` | ❌ |  |
| `issparse` | ❌ | **deferred — libs/sparse** |
| `lsqr` | ❌ | **deferred — libs/sparse** |
| `minres` | ❌ | **deferred — libs/sparse** |
| `nnz` | ✅ |  |
| `nonzeros` | ✅ |  |
| `normest` | ❌ | **deferred — libs/linalg** |
| `nzmax` | ❌ | **deferred — libs/sparse** |
| `pcg` | ❌ | **deferred — libs/sparse** |
| `qmr` | ❌ | **deferred — libs/sparse** |
| `randperm` | ✅ |  |
| `spalloc` | ❌ | **deferred — libs/sparse** |
| `sparse` | ❌ | **deferred — libs/sparse** |
| `spaugment` | ❌ |  |
| `spconvert` | ❌ |  |
| `spdiags` | ❌ | **deferred — libs/sparse** |
| `speye` | ❌ | **deferred — libs/sparse** |
| `spfun` | ❌ | **deferred — libs/sparse** |
| `spones` | ❌ |  |
| `spparms` | ❌ |  |
| `sprand` | ❌ | **deferred — libs/sparse** |
| `sprandn` | ❌ | **deferred — libs/sparse** |
| `sprandsym` | ❌ | **deferred — libs/sparse** |
| `sprank` | ❌ |  |
| `spy` | ❌ | **deferred — libs/sparse** |
| `svds` | ❌ | **deferred — libs/sparse** |
| `symamd` | ❌ | **deferred — libs/sparse** |
| `symbfact` | ❌ | **deferred — libs/sparse** |
| `symmlq` | ❌ | **deferred — libs/sparse** |
| `symrcm` | ❌ | **deferred — libs/sparse** |
| `tfqmr` | ❌ | **deferred — libs/sparse** |
| `treelayout` | ❌ | **deferred — libs/sparse** |
| `treeplot` | ❌ | **deferred — libs/sparse** |
| `unmesh` | ❌ | **deferred — libs/sparse** |

### Fourier Analysis and Filtering — 8 ✅ + 0 ⚠️ / 21 = 38%

| Function | Status | Notes |
|---|:---:|---|
| `conv` | ✅ |  |
| `conv2` | ❌ |  |
| `convn` | ❌ |  |
| `deconv` | ✅ |  |
| `fft` | ✅ |  |
| `fft2` | ❌ | N-D FFT |
| `fftn` | ❌ | N-D FFT |
| `fftshift` | ✅ |  |
| `fftw` | ❌ | wisdom file |
| `filter` | ✅ |  |
| `filter2` | ❌ |  |
| `ifft` | ✅ |  |
| `ifft2` | ❌ | N-D FFT |
| `ifftn` | ❌ | N-D FFT |
| `ifftshift` | ✅ |  |
| `interpft` | ❌ |  |
| `nextpow2` | ✅ |  |
| `nufft` | ❌ | non-uniform |
| `nufftn` | ❌ | non-uniform |
| `padecoef` | ❌ |  |
| `ss2tf` | ❌ | inverse |

## 3. Data Analysis

### Descriptive Statistics — 14 ✅ + 0 ⚠️ / 33 = 42%

| Function | Status | Notes |
|---|:---:|---|
| `bounds` | ❌ | `[min,max]` |
| `corrcoef` | ✅ |  |
| `cov` | ✅ |  |
| `cummax` | ✅ |  |
| `cummin` | ✅ |  |
| `iqr` | ❌ | inter-quartile |
| `kde` | ❌ |  |
| `mape` | ❌ |  |
| `max` | ✅ |  |
| `maxk` | ❌ |  |
| `mean` | ✅ |  |
| `median` | ✅ |  |
| `min` | ✅ |  |
| `mink` | ❌ |  |
| `mode` | ✅ |  |
| `movmad` | ❌ | moving mad |
| `movmax` | ❌ | moving max |
| `movmean` | ❌ | moving avg |
| `movmedian` | ❌ | moving median |
| `movmin` | ❌ | moving min |
| `movprod` | ❌ | moving prod |
| `movstd` | ❌ | moving std |
| `movsum` | ❌ | moving sum |
| `movvar` | ❌ | moving var |
| `prctile` | ✅ |  |
| `quantile` | ✅ |  |
| `rms` | ❌ | root-mean-square |
| `rmse` | ❌ |  |
| `std` | ✅ |  |
| `summary` | ❌ |  |
| `var` | ✅ |  |
| `xcorr` | ✅ | cross-correlation |
| `xcov` | ❌ | cross-covariance |

## 4. Programming and Scripts

### Workspace — 6 ✅ + 0 ⚠️ / 10 = 60%

| Function | Status | Notes |
|---|:---:|---|
| `clear` | ✅ |  |
| `clearvars` | ❌ |  |
| `disp` | ✅ |  |
| `formatteddisplaytext` | ❌ |  |
| `load` | ✅ |  |
| `openvar` | ❌ | IDE |
| `save` | ✅ |  |
| `who` | ✅ |  |
| `whos` | ✅ |  |
| `workspacebrowser` | ❌ |  |

### Error Handling (basic) — 4 ✅ + 0 ⚠️ / 6 = 66%

| Function | Status | Notes |
|---|:---:|---|
| `assert` | ✅ |  |
| `error` | ✅ |  |
| `lastwarn` | ❌ |  |
| `oncleanup` | ❌ |  |
| `try` | ✅ | keyword (`try/catch`) |
| `warning` | ✅ |  |

### Exception Handling — 2 ✅ + 0 ⚠️ / 2 = 100%

| Function | Status | Notes |
|---|:---:|---|
| `mexception` | ✅ | MATLAB exception class — registered as `MException` |
| `try` | ✅ | keyword (`try/catch`) |

## 5. Graphics

### Line Plots — 2 ✅ + 0 ⚠️ / 12 = 16%

| Function | Status | Notes |
|---|:---:|---|
| `area` | ❌ |  |
| `errorbar` | ❌ |  |
| `fimplicit` | ❌ |  |
| `fplot` | ❌ |  |
| `fplot3` | ❌ |  |
| `loglog` | ❌ |  |
| `plot` | ✅ |  |
| `plot3` | ❌ | 3-D |
| `semilogx` | ❌ |  |
| `semilogy` | ❌ |  |
| `stackedplot` | ❌ |  |
| `stairs` | ✅ |  |

### Polar Plots — 3 ✅ + 0 ⚠️ / 19 = 15%

| Function | Status | Notes |
|---|:---:|---|
| `compassplot` | ❌ |  |
| `fpolarplot` | ❌ |  |
| `polaraxes` | ❌ |  |
| `polarbubblechart` | ❌ |  |
| `polarhistogram` | ❌ |  |
| `polarplot` | ✅ |  |
| `polarregion` | ❌ |  |
| `polarscatter` | ❌ |  |
| `radiusregion` | ❌ |  |
| `rlim` | ✅ |  |
| `rtickangle` | ❌ |  |
| `rtickformat` | ❌ |  |
| `rticklabels` | ❌ |  |
| `rticks` | ❌ |  |
| `thetalim` | ✅ |  |
| `thetaregion` | ❌ |  |
| `thetatickformat` | ❌ |  |
| `thetaticklabels` | ❌ |  |
| `thetaticks` | ❌ |  |

### Contour Plots — 2 ✅ + 0 ⚠️ / 7 = 28%

| Function | Status | Notes |
|---|:---:|---|
| `clabel` | ❌ |  |
| `contour` | ✅ |  |
| `contour3` | ❌ |  |
| `contourc` | ❌ |  |
| `contourf` | ✅ |  |
| `contourslice` | ❌ |  |
| `fcontour` | ❌ |  |

### Vector Fields — 0 ✅ + 0 ⚠️ / 6 = 0%

| Function | Status | Notes |
|---|:---:|---|
| `compassplot` | ❌ |  |
| `feather` | ❌ |  |
| `quiver` | ❌ |  |
| `quiver3` | ❌ |  |
| `streamline` | ❌ |  |
| `streamslice` | ❌ |  |

### Surface and Mesh Plots — 3 ✅ + 0 ⚠️ / 21 = 14%

| Function | Status | Notes |
|---|:---:|---|
| `contour3` | ❌ |  |
| `cylinder` | ❌ |  |
| `ellipsoid` | ❌ |  |
| `fimplicit3` | ❌ |  |
| `fmesh` | ❌ |  |
| `fsurf` | ❌ |  |
| `hidden` | ❌ |  |
| `mesh` | ✅ |  |
| `meshc` | ❌ |  |
| `meshz` | ❌ |  |
| `pcolor` | ✅ |  |
| `peaks` | ❌ |  |
| `ribbon` | ❌ |  |
| `sphere` | ❌ |  |
| `surf` | ✅ |  |
| `surf2patch` | ❌ |  |
| `surface` | ❌ |  |
| `surfc` | ❌ |  |
| `surfl` | ❌ |  |
| `surfnorm` | ❌ |  |
| `waterfall` | ❌ |  |

### Volume Visualization — 0 ✅ + 0 ⚠️ / 24 = 0%

| Function | Status | Notes |
|---|:---:|---|
| `coneplot` | ❌ |  |
| `contourslice` | ❌ |  |
| `curl` | ❌ |  |
| `divergence` | ❌ |  |
| `flow` | ❌ |  |
| `interpstreamspeed` | ❌ |  |
| `isocaps` | ❌ |  |
| `isocolors` | ❌ |  |
| `isonormals` | ❌ |  |
| `isosurface` | ❌ |  |
| `reducepatch` | ❌ |  |
| `reducevolume` | ❌ |  |
| `shrinkfaces` | ❌ |  |
| `slice` | ❌ |  |
| `smooth3` | ❌ |  |
| `stream2` | ❌ |  |
| `stream3` | ❌ |  |
| `streamline` | ❌ |  |
| `streamparticles` | ❌ |  |
| `streamribbon` | ❌ |  |
| `streamslice` | ❌ |  |
| `streamtube` | ❌ |  |
| `subvolume` | ❌ |  |
| `volumebounds` | ❌ |  |

### Geographic Plots — 0 ✅ + 0 ⚠️ / 8 = 0%

| Function | Status | Notes |
|---|:---:|---|
| `geoaxes` | ❌ |  |
| `geobasemap` | ❌ |  |
| `geobubble` | ❌ |  |
| `geodensityplot` | ❌ |  |
| `geolimits` | ❌ |  |
| `geoplot` | ❌ |  |
| `geoscatter` | ❌ |  |
| `geotickformat` | ❌ |  |

## 6. Data Import and Export (file I/O only)

### Low-Level File I/O — 13 ✅ + 0 ⚠️ / 15 = 86%

| Function | Status | Notes |
|---|:---:|---|
| `fclose` | ✅ |  |
| `feof` | ✅ |  |
| `ferror` | ✅ |  |
| `fgetl` | ✅ |  |
| `fgets` | ✅ |  |
| `fileread` | ❌ | whole-file read |
| `fopen` | ✅ |  |
| `fprintf` | ✅ |  |
| `fread` | ✅ |  |
| `frewind` | ✅ |  |
| `fscanf` | ✅ |  |
| `fseek` | ✅ |  |
| `ftell` | ✅ |  |
| `fwrite` | ✅ |  |
| `openedfiles` | ❌ |  |

### Text Files (CSV / dlm / readtable) — 1 ✅ + 0 ⚠️ / 16 = 6%

| Function | Status | Notes |
|---|:---:|---|
| `fileread` | ❌ | whole-file read |
| `importdatatask` | ❌ |  |
| `importtool` | ❌ |  |
| `readcell` | ❌ |  |
| `readlines` | ❌ |  |
| `readmatrix` | ❌ | modern CSV |
| `readtable` | ❌ | needs table type |
| `readtimetable` | ❌ |  |
| `readvars` | ❌ |  |
| `textscan` | ✅ |  |
| `type` | ❌ |  |
| `writecell` | ❌ |  |
| `writelines` | ❌ |  |
| `writematrix` | ❌ | modern CSV |
| `writetable` | ❌ | needs table type |
| `writetimetable` | ❌ |  |

### Spreadsheets — 0 ✅ + 0 ⚠️ / 13 = 0%

| Function | Status | Notes |
|---|:---:|---|
| `importdata` | ❌ | auto-detect |
| `importdatatask` | ❌ |  |
| `importtool` | ❌ |  |
| `readcell` | ❌ |  |
| `readmatrix` | ❌ | modern CSV |
| `readtable` | ❌ | needs table type |
| `readtimetable` | ❌ |  |
| `readvars` | ❌ |  |
| `sheetnames` | ❌ |  |
| `writecell` | ❌ |  |
| `writematrix` | ❌ | modern CSV |
| `writetable` | ❌ | needs table type |
| `writetimetable` | ❌ |  |

### Workspace Save / Load — 0 ✅ + 0 ⚠️ / 2 = 0%

| Function | Status | Notes |
|---|:---:|---|
| `loadobj` | ❌ |  |
| `saveobj` | ❌ |  |

### File Name Construction — 0 ✅ + 0 ⚠️ / 9 = 0%

| Function | Status | Notes |
|---|:---:|---|
| `filemarker` | ❌ |  |
| `fileparts` | ❌ | split path |
| `filesep` | ❌ | path sep |
| `fullfile` | ❌ | OS path join |
| `matlabdrive` | ❌ |  |
| `matlabroot` | ❌ |  |
| `tempdir` | ❌ |  |
| `tempname` | ❌ |  |
| `toolboxdir` | ❌ |  |

## 7. Signal Processing Toolbox

### Waveform Generation — 5 ✅ + 0 ⚠️ / 21 = 23%

| Function | Status | Notes |
|---|:---:|---|
| `buffer` | ❌ | reshape with overlap |
| `chirp` | ✅ |  |
| `demod` | ❌ |  |
| `diric` | ❌ | Dirichlet |
| `framelbl` | ❌ |  |
| `framesig` | ❌ |  |
| `gauspuls` | ✅ | Gaussian pulse |
| `gmonopuls` | ❌ | Gaussian monopulse |
| `marcumq` | ❌ |  |
| `modulate` | ❌ |  |
| `pulstran` | ✅ | pulse train |
| `rectpuls` | ✅ | rectangular pulse |
| `sawtooth` | ❌ |  |
| `shiftdata` | ❌ |  |
| `sinc` | ❌ | sin(πx)/(πx) |
| `square` | ❌ |  |
| `tripuls` | ✅ | triangular |
| `udecode` | ❌ |  |
| `uencode` | ❌ |  |
| `unshiftdata` | ❌ |  |
| `vco` | ❌ | VCO |

### Filter Design (FIR / IIR coefficient generators) — 6 ✅ + 0 ⚠️ / 37 = 16%

| Function | Status | Notes |
|---|:---:|---|
| `butter` | ✅ | IIR Butterworth |
| `buttord` | ❌ | order estimator |
| `cfirpm` | ❌ | complex Parks-McClellan |
| `cheb1ord` | ❌ | order estimator |
| `cheb2ord` | ❌ | order estimator |
| `cheby1` | ❌ | IIR Chebyshev I |
| `cheby2` | ❌ | IIR Chebyshev II |
| `designfilt` | ❌ |  |
| `designfilter` | ❌ |  |
| `digitalfilter` | ❌ |  |
| `double` | ✅ |  |
| `dspfwiz` | ❌ |  |
| `ellip` | ❌ | IIR elliptic |
| `ellipord` | ❌ | order estimator |
| `filt2block` | ❌ |  |
| `filteranalyzer` | ❌ |  |
| `fir1` | ✅ | FIR window-design |
| `fir2` | ❌ | arbitrary-response FIR |
| `fircls` | ❌ | constrained-LS FIR |
| `fircls1` | ❌ |  |
| `firls` | ❌ | least-squares FIR |
| `firpm` | ❌ | Parks-McClellan FIR |
| `firpmord` | ❌ | order estimator |
| `gaussdesign` | ❌ |  |
| `info` | ❌ |  |
| `intfilt` | ❌ | interpolating FIR |
| `isdouble` | ❌ |  |
| `issingle` | ✅ |  |
| `kaiserord` | ❌ | Kaiser window order |
| `maxflat` | ❌ |  |
| `polyscale` | ❌ |  |
| `polystab` | ❌ |  |
| `rcosdesign` | ❌ |  |
| `scalefiltersections` | ❌ |  |
| `sgolay` | ✅ | Savitzky-Golay |
| `single` | ✅ |  |
| `yulewalk` | ❌ | recursive YW |

### Analog Filters (prototype + analog response) — 1 ✅ + 0 ⚠️ / 17 = 5%

| Function | Status | Notes |
|---|:---:|---|
| `besselap` | ❌ | analog prototype |
| `besself` | ❌ | IIR Bessel |
| `bilinear` | ❌ |  |
| `buttap` | ❌ | analog prototype |
| `butter` | ✅ | IIR Butterworth |
| `cheb1ap` | ❌ | analog prototype |
| `cheb2ap` | ❌ | analog prototype |
| `cheby1` | ❌ | IIR Chebyshev I |
| `cheby2` | ❌ | IIR Chebyshev II |
| `ellip` | ❌ | IIR elliptic |
| `ellipap` | ❌ | analog prototype |
| `freqs` | ❌ | analog freq response |
| `impinvar` | ❌ |  |
| `lp2bp` | ❌ |  |
| `lp2bs` | ❌ |  |
| `lp2hp` | ❌ |  |
| `lp2lp` | ❌ |  |

### Digital Filter Analysis (freqz / phasez / grpdelay / impz / ...) — 3 ✅ + 0 ⚠️ / 19 = 15%

| Function | Status | Notes |
|---|:---:|---|
| `filteranalyzer` | ❌ |  |
| `filternorm` | ❌ |  |
| `filtord` | ❌ |  |
| `firtype` | ❌ |  |
| `freqz` | ✅ | discrete freq response |
| `grpdelay` | ✅ | group delay |
| `impz` | ❌ | impulse response |
| `impzlength` | ❌ | impulse length |
| `isallpass` | ❌ | predicate |
| `isfir` | ❌ | predicate |
| `islinphase` | ❌ | predicate |
| `ismaxphase` | ❌ | predicate |
| `isminphase` | ❌ | predicate |
| `isstable` | ❌ | predicate |
| `phasedelay` | ❌ | phase delay |
| `phasez` | ✅ | phase response |
| `stepz` | ❌ | step response |
| `zerophase` | ❌ |  |
| `zplane` | ❌ |  |

### Digital Filtering (filter / filtfilt / sosfilt / lowpass / ...) — 8 ✅ + 0 ⚠️ / 41 = 19%

| Function | Status | Notes |
|---|:---:|---|
| `bandpass` | ❌ | spec-driven BP |
| `bandstop` | ❌ | spec-driven BS |
| `cell2sos` | ❌ |  |
| `convmtx` | ❌ | convolution matrix |
| `ctf2zp` | ❌ | control TF → ZPK |
| `ctffilt` | ❌ | control TF filter |
| `dspfwiz` | ❌ |  |
| `eqtflength` | ❌ |  |
| `fftfilt` | ❌ | FFT-based overlap-add |
| `filt2block` | ❌ |  |
| `filtfilt` | ✅ | zero-phase forward+back |
| `filtic` | ❌ | init state |
| `hampel` | ❌ | outlier-resilient |
| `highpass` | ❌ | spec-driven HP |
| `latc2tf` | ❌ | inverse |
| `latcfilt` | ❌ |  |
| `lowpass` | ❌ | spec-driven LP |
| `medfilt1` | ✅ | median |
| `residuez` | ❌ |  |
| `scalefiltersections` | ❌ |  |
| `sgolayfilt` | ✅ | Savitzky-Golay |
| `sos2cell` | ❌ |  |
| `sos2ctf` | ❌ |  |
| `sos2ss` | ❌ | SOS → SS |
| `sos2tf` | ❌ | inverse |
| `sos2zp` | ❌ | SOS → ZPK |
| `sosfilt` | ✅ | SOS-cascade filter |
| `ss` | ❌ |  |
| `ss2sos` | ❌ | inverse |
| `ss2zp` | ❌ | SS → ZPK |
| `tf` | ❌ |  |
| `tf2latc` | ❌ | lattice |
| `tf2sos` | ✅ | TF → SOS |
| `tf2ss` | ❌ | TF → SS |
| `tf2zp` | ✅ | TF → ZPK |
| `tf2zpk` | ❌ |  |
| `zp2ctf` | ❌ |  |
| `zp2sos` | ✅ | ZPK → SOS |
| `zp2ss` | ❌ | inverse |
| `zp2tf` | ✅ | inverse |
| `zpk` | ❌ |  |

### Multirate Signal Processing (decimate / interp / resample / ...) — 4 ✅ + 0 ⚠️ / 8 = 50%

| Function | Status | Notes |
|---|:---:|---|
| `decimate` | ✅ |  |
| `downsample` | ✅ |  |
| `fillgaps` | ❌ |  |
| `interp` | ❌ |  |
| `intfilt` | ❌ | interpolating FIR |
| `resample` | ✅ |  |
| `upfirdn` | ❌ |  |
| `upsample` | ✅ |  |

### Signal Modeling (AR / Burg / Yule-Walker / Levinson / Prony) — 0 ✅ + 0 ⚠️ / 25 = 0%

| Function | Status | Notes |
|---|:---:|---|
| `ac2poly` | ❌ |  |
| `ac2rc` | ❌ |  |
| `arburg` | ❌ | Burg AR |
| `arcov` | ❌ | covariance AR |
| `armcov` | ❌ | modified cov AR |
| `aryule` | ❌ | Yule-Walker AR |
| `corrmtx` | ❌ | autocorr matrix |
| `invfreqs` | ❌ |  |
| `invfreqz` | ❌ | IIR sys-id |
| `is2rc` | ❌ |  |
| `lar2rc` | ❌ |  |
| `levinson` | ❌ | Levinson-Durbin |
| `lpc` | ❌ | linear prediction |
| `lsf2poly` | ❌ |  |
| `poly2ac` | ❌ |  |
| `poly2lsf` | ❌ |  |
| `poly2rc` | ❌ |  |
| `prony` | ❌ | Prony method |
| `rc2ac` | ❌ |  |
| `rc2is` | ❌ |  |
| `rc2lar` | ❌ |  |
| `rc2poly` | ❌ |  |
| `rlevinson` | ❌ | reverse Levinson |
| `schurrc` | ❌ | Schur recursion |
| `stmcb` | ❌ | Steiglitz-McBride |

### Correlation and Convolution (extras: alignsignals / finddelay / xcorr2 / cconv / convmtx) — 0 ✅ + 0 ⚠️ / 9 = 0%

| Function | Status | Notes |
|---|:---:|---|
| `alignsignals` | ❌ | align via xcorr |
| `cconv` | ❌ | circular convolution |
| `convmtx` | ❌ | convolution matrix |
| `corrmtx` | ❌ | autocorr matrix |
| `dtw` | ❌ | dynamic time warp |
| `edr` | ❌ | edit distance on real |
| `finddelay` | ❌ | estimate delay |
| `findsignal` | ❌ | pattern search |
| `xcorr2` | ❌ | 2-D xcorr |

### Transforms (FFT / DCT / DWT / Hilbert / CZT / Cepstrum) — 6 ✅ + 0 ⚠️ / 32 = 18%

| Function | Status | Notes |
|---|:---:|---|
| `bitrevorder` | ❌ | bit-reverse permutation |
| `cceps` | ❌ | complex cepstrum |
| `czt` | ❌ | chirp Z-transform |
| `dct` | ✅ |  |
| `dftmtx` | ❌ | DFT matrix |
| `digitrevorder` | ❌ |  |
| `dlistft` | ❌ |  |
| `dlstft` | ❌ |  |
| `emd` | ❌ | empirical mode decomp |
| `envelope` | ✅ |  |
| `fsst` | ❌ | Fourier synchrosqueezed |
| `fwht` | ❌ | fast Walsh-Hadamard |
| `goertzel` | ✅ |  |
| `hht` | ❌ | Hilbert-Huang |
| `hilbert` | ✅ |  |
| `icceps` | ❌ | inverse complex cepstrum |
| `idct` | ✅ |  |
| `ifsst` | ❌ |  |
| `ifwht` | ❌ | inverse |
| `instfreq` | ❌ | instantaneous frequency |
| `istft` | ❌ | inverse |
| `istftlayer` | ❌ |  |
| `pspectrum` | ❌ | easy spectral analysis |
| `rceps` | ❌ | real cepstrum |
| `spectrogram` | ✅ |  |
| `stft` | ❌ | short-time FFT |
| `stftlayer` | ❌ |  |
| `stftmag2sig` | ❌ |  |
| `vmd` | ❌ | variational MD |
| `wvd` | ❌ | Wigner-Ville |
| `xspectrogram` | ❌ | cross-spectrogram |
| `xwvd` | ❌ | cross WVD |

### Windows (Hamming / Hann / Kaiser / Chebyshev / DPSS / ...) — 6 ✅ + 0 ⚠️ / 24 = 25%

| Function | Status | Notes |
|---|:---:|---|
| `barthannwin` | ❌ | Bartlett-Hann |
| `bartlett` | ✅ |  |
| `blackman` | ✅ |  |
| `blackmanharris` | ❌ |  |
| `bohmanwin` | ❌ | Bohman |
| `chebwin` | ❌ | Dolph-Chebyshev |
| `dpss` | ❌ | discrete prolate spheroidal |
| `dpssclear` | ❌ | cache |
| `dpssdir` | ❌ | cache |
| `dpssload` | ❌ | cache |
| `dpsssave` | ❌ | cache |
| `enbw` | ❌ | equivalent noise BW |
| `flattopwin` | ❌ |  |
| `gausswin` | ❌ | Gaussian |
| `hamming` | ✅ |  |
| `hann` | ✅ |  |
| `kaiser` | ✅ |  |
| `nuttallwin` | ❌ |  |
| `parzenwin` | ❌ | Parzen |
| `rectwin` | ✅ |  |
| `taylorwin` | ❌ | Taylor |
| `triang` | ❌ | triangular |
| `tukeywin` | ❌ | tapered cosine |
| `wvtool` | ❌ | GUI |

### Parametric Spectral Estimation (pburg / pmtm / pmusic / ...) — 1 ✅ + 0 ⚠️ / 10 = 10%

| Function | Status | Notes |
|---|:---:|---|
| `db` | ❌ | magnitude → dB |
| `db2mag` | ❌ |  |
| `db2pow` | ❌ |  |
| `findpeaks` | ✅ |  |
| `mag2db` | ❌ |  |
| `pburg` | ❌ | Burg AR |
| `pcov` | ❌ |  |
| `pmcov` | ❌ |  |
| `pow2db` | ❌ |  |
| `pyulear` | ❌ | Yule-Walker AR |

### Nonparametric Spectral Estimation (pwelch / periodogram / cpsd / ...) — 3 ✅ + 0 ⚠️ / 17 = 17%

| Function | Status | Notes |
|---|:---:|---|
| `cpsd` | ❌ | cross-PSD |
| `db` | ❌ | magnitude → dB |
| `db2mag` | ❌ |  |
| `db2pow` | ❌ |  |
| `findpeaks` | ✅ |  |
| `mag2db` | ❌ |  |
| `mscohere` | ❌ | magnitude-squared coherence |
| `periodogram` | ✅ |  |
| `plomb` | ❌ | Lomb-Scargle |
| `pmtm` | ❌ | multi-taper |
| `poctave` | ❌ |  |
| `pow2db` | ❌ |  |
| `pspectrum` | ❌ | easy spectral analysis |
| `pwelch` | ✅ | Welch PSD |
| `refinepeaks` | ❌ |  |
| `spectralentropy` | ❌ |  |
| `tfestimate` | ❌ | TF estimate |

### Spectral Measurements (bandpower / snr / sinad / thd / ...) — 0 ✅ + 0 ⚠️ / 18 = 0%

| Function | Status | Notes |
|---|:---:|---|
| `bandpower` | ❌ |  |
| `enbw` | ❌ | equivalent noise BW |
| `instbw` | ❌ |  |
| `instfreq` | ❌ | instantaneous frequency |
| `meanfreq` | ❌ | mean frequency |
| `medfreq` | ❌ | median frequency |
| `obw` | ❌ |  |
| `powerbw` | ❌ |  |
| `sfdr` | ❌ | spurious-free dynamic range |
| `sinad` | ❌ | signal-noise-distortion |
| `snr` | ❌ | signal-to-noise |
| `spectralcrest` | ❌ |  |
| `spectralentropy` | ❌ |  |
| `spectralflatness` | ❌ |  |
| `spectralkurtosis` | ❌ |  |
| `spectralskewness` | ❌ |  |
| `thd` | ❌ | total harmonic distortion |
| `toi` | ❌ | third-order intercept |

### Time-Frequency Analysis (spectrogram / stft / cwt / wvd / ...) — 1 ✅ + 0 ⚠️ / 27 = 3%

| Function | Status | Notes |
|---|:---:|---|
| `dlistft` | ❌ |  |
| `dlstft` | ❌ |  |
| `emd` | ❌ | empirical mode decomp |
| `fsst` | ❌ | Fourier synchrosqueezed |
| `hht` | ❌ | Hilbert-Huang |
| `ifsst` | ❌ |  |
| `instbw` | ❌ |  |
| `instfreq` | ❌ | instantaneous frequency |
| `iscola` | ❌ |  |
| `istft` | ❌ | inverse |
| `istftlayer` | ❌ |  |
| `kurtogram` | ❌ |  |
| `pspectrum` | ❌ | easy spectral analysis |
| `spectralcrest` | ❌ |  |
| `spectralentropy` | ❌ |  |
| `spectralflatness` | ❌ |  |
| `spectralkurtosis` | ❌ |  |
| `spectralskewness` | ❌ |  |
| `spectrogram` | ✅ |  |
| `stft` | ❌ | short-time FFT |
| `stftlayer` | ❌ |  |
| `stftmag2sig` | ❌ |  |
| `tfridge` | ❌ |  |
| `vmd` | ❌ | variational MD |
| `wvd` | ❌ | Wigner-Ville |
| `xspectrogram` | ❌ | cross-spectrogram |
| `xwvd` | ❌ | cross WVD |

### Pulse and Transition Metrics (risetime / dutycycle / overshoot / ...) — 0 ✅ + 0 ⚠️ / 12 = 0%

| Function | Status | Notes |
|---|:---:|---|
| `dutycycle` | ❌ | duty cycle |
| `falltime` | ❌ |  |
| `midcross` | ❌ | mid-ref crossings |
| `overshoot` | ❌ |  |
| `pulseperiod` | ❌ |  |
| `pulsesep` | ❌ |  |
| `pulsewidth` | ❌ |  |
| `risetime` | ❌ |  |
| `settlingtime` | ❌ |  |
| `slewrate` | ❌ |  |
| `statelevels` | ❌ |  |
| `undershoot` | ❌ |  |

### Signal Descriptive Statistics (rms / peak2peak / envelope / sigROIs / ...) — 2 ✅ + 0 ⚠️ / 30 = 6%

| Function | Status | Notes |
|---|:---:|---|
| `alignsignals` | ❌ | align via xcorr |
| `binmask2sigroi` | ❌ |  |
| `countlabels` | ❌ |  |
| `cusum` | ❌ | CUSUM change detection |
| `dtw` | ❌ | dynamic time warp |
| `edr` | ❌ | edit distance on real |
| `envelope` | ✅ |  |
| `extendsigroi` | ❌ |  |
| `extractsigroi` | ❌ |  |
| `filenames2labels` | ❌ |  |
| `findchangepts` | ❌ | change-point detection |
| `finddelay` | ❌ | estimate delay |
| `findpeaks` | ✅ |  |
| `findsignal` | ❌ | pattern search |
| `folders2labels` | ❌ |  |
| `framelbl` | ❌ |  |
| `framesig` | ❌ |  |
| `meanfreq` | ❌ | mean frequency |
| `medfreq` | ❌ | median frequency |
| `mergesigroi` | ❌ |  |
| `peak2peak` | ❌ | p-p amplitude |
| `peak2rms` | ❌ |  |
| `removesigroi` | ❌ |  |
| `rssq` | ❌ | root-sum-squared |
| `seqperiod` | ❌ |  |
| `shortensigroi` | ❌ |  |
| `sigrangebinmask` | ❌ |  |
| `sigroi2binmask` | ❌ |  |
| `splitlabels` | ❌ |  |
| `zerocrossrate` | ❌ |  |

### Smoothing and Denoising (smoothdata / hampel / sgolayfilt / ...) — 3 ✅ + 0 ⚠️ / 4 = 75%

| Function | Status | Notes |
|---|:---:|---|
| `hampel` | ❌ | outlier-resilient |
| `medfilt1` | ✅ | median |
| `sgolay` | ✅ | Savitzky-Golay |
| `sgolayfilt` | ✅ | Savitzky-Golay |

### Vibration Analysis (envspectrum / order tracking / modal) — 0 ✅ + 0 ⚠️ / 13 = 0%

| Function | Status | Notes |
|---|:---:|---|
| `envspectrum` | ❌ | envelope spectrum |
| `modalfit` | ❌ | modal-fit |
| `modalfrf` | ❌ |  |
| `modalsd` | ❌ |  |
| `orderspectrum` | ❌ |  |
| `ordertrack` | ❌ |  |
| `orderwaveform` | ❌ |  |
| `rainflow` | ❌ |  |
| `rpmfreqmap` | ❌ |  |
| `rpmordermap` | ❌ |  |
| `rpmtrack` | ❌ | order tracking |
| `tachorpm` | ❌ | tachometer→RPM |
| `tsa` | ❌ |  |


## Summary — 296 ✅ + 24 ⚠️ / 1226 = 26%

Status of the major missing groups (high-effort, deferred to dedicated libs):

- **Linear Algebra** — separate `libs/linalg/` likely (LAPACK-class operations)
- **Sparse Matrices** — separate `libs/sparse/` + dispatch logic
- **ODE / DAE / DDE / BVP / PDE** — `libs/ode/` likely
- **Tables / Timetables / Categorical** — fundamental data-type addition
- **Datetime / Duration** — fundamental data-type addition
- **`containers.Map` / `dictionary`** — hash-map type
- **`fft2` / `ifft2` / `fftn` / `ifftn`** — N-D FFT entry points
- **Wavelet family** (`cwt` / `wsst` / `vmd` / `hht` / `emd` / `dwt`) — separate effort
- **Signal Modeling family** (Burg / Yule-Walker / Levinson / Prony / `lpc` / `invfreqz`) — `libs/signal/parametric/` add

## Quick Wins

Functions cheap to implement (often wrappers over existing kernels) but common in MATLAB scripts:

1. **Operator-named functions:** `plus`, `minus`, `times`, `mtimes`, `mldivide`, `mrdivide`, `rdivide`, `ldivide`, `power`, `mpower`, `uminus`, `uplus`, `ctranspose`, `eq`, `ne`, `lt`, `le`, `gt`, `ge`, `and`, `or`, `not`, `colon` — register thin wrappers over the operators (lifts ~25 ⚠️ to ✅)
2. **Hyperbolic + degree-trig variants:** `sinh`, `cosh`, `tanh`, `asinh`, `acosh`, `atanh`, `sind`, `cosd`, `tand`, `asind`, `acosd`, `atand`, `atan2d`, `sinpi`, `cospi`
3. **Reciprocal trig:** `sec`, `csc`, `cot` and their `*h` / `*d` / `a*` variants
4. **Coordinate transforms:** `cart2pol`, `cart2sph`, `pol2cart`, `sph2cart`
5. **Moving statistics:** `movmean`, `movmedian`, `movmin`, `movmax`, `movsum`, `movstd`, `movvar`, `movmad`, `movprod`, `smoothdata`, `hampel`
6. **Cell idioms:** `mat2cell`, `num2cell`, `cell2mat`, `iscellstr`, `cellplot`, `cellstr`, `celldisp`
7. **Struct utilities:** `getfield`, `setfield`, `orderfields`, `struct2cell`, `cell2struct`, `deal`
8. **String utilities:** `strncmp`, `strncmpi`, `strfind`, `blanks`, `deblank`, `mat2str`, `strtok`, `strjoin`, `strjust`, `cellstr`
9. **Type predicates:** `isvector`, `isrow`, `iscolumn`, `ismatrix`, `issorted`
10. **Index helpers:** `sub2ind`, `ind2sub`, `shiftdim`, `flip`, `repelem`
11. **N-D FFT:** `fft2`, `ifft2`, `fftn`, `ifftn`
12. **Optimization extras:** `fminbnd`, `fminsearch`, `lsqnonneg`, `optimset` / `optimget`
13. **Rounding/exp extras:** `pow2`, `realpow`, `reallog`, `realsqrt`
14. **Function-handle:** `feval`, `func2str`, `str2func`
15. **Special funcs:** `beta`, `betainc`, `gammainc`, `besselj` / `bessely` / `besseli` / `besselk`, `legendre`, `psi`, `airy`, `expint`
16. **Set ops:** `setxor`, `ismembertol`, `uniquetol`, `allunique`
17. **Bit ops:** `bitset`, `bitget`
18. **Constants as functions:** `flintmax`, `intmax`, `intmin`, `realmax`, `realmin`, `allfinite`, `anynan`
19. **File-path helpers:** `fullfile`, `fileparts`, `filesep`, `pathsep`, `tempdir`, `tempname`

### Signal Processing Toolbox quick wins

1. **Filter conversions** (algebraic, fast wrappers): `sos2tf`, `sos2zp`, `tf2ss`, `ss2tf`, `ss2zp`, `zp2ss`, `sos2ss`, `ss2sos`
2. **Filter analysis predicates:** `isallpass`, `isfir`, `islinphase`, `ismaxphase`, `isminphase`, `isstable`
3. **Filter analysis primary:** `impz`, `impzlength`, `stepz`, `phasedelay`, `zerophase`
4. **Spec-driven filters:** `lowpass`, `highpass`, `bandpass`, `bandstop`
5. **Magnitude/phase utils:** `db`, `db2mag`, `db2pow`, `mag2db`, `pow2db`, `wrapToPi`, `wrap2Pi`, `wrapTo180`, `wrapTo360`
6. **Window family extras:** `triang`, `tukeywin`, `flattopwin`, `gausswin`, `chebwin`, `parzenwin`, `nuttallwin`, `taylorwin`
7. **Signal stats:** `rms`, `rssq`, `peak2peak`, `peak2rms`
8. **Waveform extras:** `square`, `sawtooth`, `sinc`, `gmonopuls`, `diric`
9. **AR / linear-prediction:** `lpc`, `levinson`, `arburg`, `arcov`, `armcov`, `aryule`, `prony`, `stmcb`, `invfreqz`
10. **Cepstrum + bit-rev:** `cceps`, `rceps`, `icceps`, `dftmtx`, `bitrevorder`, `dst`/`idst`
11. **Spectrum extras:** `cpsd`, `mscohere`, `tfestimate`, `pmtm`, `pburg`, `pyulear`, `pmusic`, `peig`, `pcov`, `pmcov`, `bandpower`
12. **Pulse metrics:** `risetime`, `falltime`, `slewrate`, `settlingtime`, `overshoot`, `undershoot`, `dutycycle`, `pulseperiod`, `pulsesep`, `statelevels`, `midcross`
13. **STFT family:** `stft`, `istft`, `pspectrum`
14. **Detrending / change-point:** `detrend`, `findchangepts`, `cusum`, `findsignal`

---

*Generated 2026-04-28 via web.archive.org snapshots of R2024b MATLAB doc pages.
Detection scope: registerFunction list + lexer keywords + Engine constants + operator-as-function aliases.*
