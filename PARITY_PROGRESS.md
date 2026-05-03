# numkit-m parity progress

This is the live parity map of numkit's MATLAB compatibility surface.
Every documented MATLAB function gets a row here, organized by
MATLAB-doc section + namespace. Empty cells mean "not yet benched";
filled cells reflect the most recent harness run.

Updated by `tools/parity/run_parity.py` — each spec run rewrites the
row(s) for its function in place (the same function may appear in
multiple sections; all occurrences refresh together).

**Columns:**
- `function` — MATLAB-doc name
- `status` — ✅ implemented · ❌ missing · ⚠️ partial / operator-only
- `numkit_ms` — single-iteration mean (ms) on numkit native build
- `vs_MATLAB` — MATLAB_ms / numkit_ms (>1× = numkit faster)
- `vs_Octave` — same against Octave
- `correctness` — `OK` element-wise vs MATLAB · `MISMATCH` · `N/A` if
  the comparison engine doesn't support the function
- `comment` — input size / notes / deviations

## Entering Commands

**Namespace:** core — 5 ✅ + 0 ⚠️ / 9 = 56%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `ans` | ✅ |  |  |  |  | implicit-assigned unsuppressed result |
| `clc` | ✅ |  |  |  |  |  |
| `commandhistory` | ❌ |  |  |  |  | IDE-only |
| `commandwindow` | ❌ |  |  |  |  | IDE-only |
| `diary` | ❌ |  |  |  |  | session log |
| `format` | ✅ |  |  |  |  | output format (no-op stub) |
| `home` | ✅ |  |  |  |  | terminal home |
| `iskeyword` | ✅ |  |  |  |  | introspection |
| `more` | ❌ |  |  |  |  | pager |

## Matrices and Arrays

**Namespace:** core — 53 ✅ + 1 ⚠️ / 55 = 98%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `blkdiag` | ✅ |  |  |  |  |  |
| `cat` | ✅ |  |  |  |  |  |
| `circshift` | ✅ |  |  |  |  |  |
| `colon` | ⚠️ |  |  |  |  | works as `:` (range) operator; not callable as named fn |
| `combinations` | ❌ |  |  |  |  | all combinations |
| `ctranspose` | ✅ |  |  |  |  | named-fn form added in Pack 11 |
| `diag` | ✅ |  |  |  |  |  |
| `end` | ✅ |  |  |  |  | keyword + `A(end)` indexing form |
| `eye` | ✅ |  |  |  |  |  |
| `false` | ✅ |  |  |  |  | literal/constant |
| `flip` | ✅ |  |  |  |  | general N-D flip |
| `fliplr` | ✅ |  |  |  |  |  |
| `flipud` | ✅ |  |  |  |  |  |
| `freqspace` | ✅ |  |  |  |  |  |
| `head` | ✅ |  |  |  |  |  |
| `horzcat` | ✅ |  |  |  |  |  |
| `ind2sub` | ✅ |  |  |  |  | linear-index conv |
| `ipermute` | ✅ |  |  |  |  |  |
| `iscolumn` | ✅ |  |  |  |  | predicate |
| `isempty` | ✅ |  |  |  |  |  |
| `ismatrix` | ✅ |  |  |  |  | predicate |
| `isrow` | ✅ |  |  |  |  | predicate |
| `isscalar` | ✅ |  |  |  |  |  |
| `issorted` | ✅ |  |  |  |  | check sorted |
| `issortedrows` | ✅ |  |  |  |  |  |
| `isuniform` | ✅ |  |  |  |  | uniform-spacing test |
| `isvector` | ✅ |  |  |  |  | predicate |
| `length` | ✅ |  |  |  |  |  |
| `linspace` | ✅ |  |  |  |  |  |
| `logspace` | ✅ |  |  |  |  |  |
| `meshgrid` | ✅ |  |  |  |  |  |
| `ndgrid` | ✅ |  |  |  |  |  |
| `ndims` | ✅ |  |  |  |  |  |
| `numel` | ✅ |  |  |  |  |  |
| `ones` | ✅ |  |  |  |  |  |
| `paddata` | ✅ |  |  |  |  | pad N-D |
| `permute` | ✅ |  |  |  |  |  |
| `rand` | ✅ |  |  |  |  |  |
| `repelem` | ✅ |  |  |  |  |  |
| `repmat` | ✅ |  |  |  |  |  |
| `reshape` | ✅ |  |  |  |  |  |
| `resize` | ✅ |  |  |  |  | general resize |
| `rot90` | ✅ |  |  |  |  |  |
| `shiftdim` | ✅ |  |  |  |  |  |
| `size` | ✅ |  |  |  |  |  |
| `sort` | ✅ |  |  |  |  |  |
| `sortrows` | ✅ |  |  |  |  |  |
| `squeeze` | ✅ |  |  |  |  |  |
| `sub2ind` | ✅ |  |  |  |  | linear-index conv |
| `tail` | ✅ |  |  |  |  |  |
| `transpose` | ✅ |  |  |  |  | named-fn form added in Pack 11 |
| `trimdata` | ✅ |  |  |  |  |  |
| `true` | ✅ |  |  |  |  | literal/constant |
| `vertcat` | ✅ |  |  |  |  |  |
| `zeros` | ✅ |  |  |  |  |  |

## Control Flow

**Namespace:** core (keywords) — 10 ✅ + 0 ⚠️ / 11 = 91%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `break` | ✅ |  |  |  |  | keyword |
| `continue` | ✅ |  |  |  |  | keyword |
| `end` | ✅ |  |  |  |  | keyword + `A(end)` indexing form |
| `for` | ✅ |  |  |  |  | keyword |
| `if` | ✅ |  |  |  |  | keyword |
| `parfor` | ❌ |  |  |  |  | parallel — out of scope |
| `pause` | ✅ |  |  |  |  | no time.sleep |
| `return` | ✅ |  |  |  |  | keyword |
| `switch` | ✅ |  |  |  |  | keyword (`switch/case/otherwise`) |
| `try` | ✅ |  |  |  |  | keyword (`try/catch`) |
| `while` | ✅ |  |  |  |  | keyword |

## Numeric Types

**Namespace:** core — 27 ✅ + 0 ⚠️ / 29 = 93%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `allfinite` | ✅ |  |  |  |  | whole-array `all(isfinite)` |
| `anynan` | ✅ |  |  |  |  | whole-array `any(isnan)` |
| `cast` | ✅ | 5.072 | 0.30× | 0.55× | OK | 1M doubles -> int32. 50 iters. |
| `double` | ✅ |  |  |  |  |  |
| `eps` | ✅ |  |  |  |  | constant (machine eps) |
| `flintmax` | ✅ |  |  |  |  | largest exact float-int |
| `inf` | ✅ |  |  |  |  | constant |
| `int16` | ✅ |  |  |  |  |  |
| `int32` | ✅ |  |  |  |  |  |
| `int64` | ✅ |  |  |  |  |  |
| `int8` | ✅ |  |  |  |  |  |
| `intmax` | ✅ |  |  |  |  | max int per type |
| `intmin` | ✅ |  |  |  |  | min int per type |
| `isfinite` | ✅ |  |  |  |  |  |
| `isfloat` | ✅ |  |  |  |  |  |
| `isinf` | ✅ |  |  |  |  |  |
| `isinteger` | ✅ |  |  |  |  |  |
| `isnan` | ✅ |  |  |  |  |  |
| `isnumeric` | ✅ |  |  |  |  |  |
| `isreal` | ✅ |  |  |  |  |  |
| `nan` | ✅ |  |  |  |  | constant |
| `realmax` | ✅ |  |  |  |  | largest finite double |
| `realmin` | ✅ |  |  |  |  | smallest normal double |
| `single` | ✅ |  |  |  |  |  |
| `typecast` | ✅ | 1.059 | 0.01× | 0.97× | OK | 1M uint32 reinterpreted as 2M uint16 (LE byte order). 50 iters. |
| `uint16` | ✅ |  |  |  |  |  |
| `uint32` | ✅ |  |  |  |  |  |
| `uint64` | ✅ |  |  |  |  |  |
| `uint8` | ✅ |  |  |  |  |  |

## Characters and Strings

**Namespace:** core — 54 ✅ + 0 ⚠️ / 65 = 83%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `append` | ✅ |  |  |  |  |  |
| `blanks` | ✅ |  |  |  |  |  |
| `cellstr` | ✅ |  |  |  |  | cell of char rows |
| `char` | ✅ |  |  |  |  |  |
| `compose` | ✅ | 0.391 | 0.68× | — | OK | Format 1000 ints with single-spec template. 100 iters. |
| `contains` | ✅ |  |  |  |  |  |
| `convertcharstostrings` | ✅ |  |  |  |  |  |
| `convertcontainedstringstochars` | ✅ |  |  |  |  |  |
| `convertstringstochars` | ✅ |  |  |  |  |  |
| `count` | ✅ |  |  |  |  |  |
| `deblank` | ✅ |  |  |  |  |  |
| `double` | ✅ |  |  |  |  |  |
| `endswith` | ❌ |  |  |  |  |  |
| `erase` | ✅ |  |  |  |  |  |
| `erasebetween` | ✅ |  |  |  |  |  |
| `extract` | ✅ | 0.106 | 1.00× | — | OK | Extract 'xyz' from 8000-char string with 1000 hits. 1000 iters. |
| `extractafter` | ✅ |  |  |  |  |  |
| `extractbefore` | ✅ |  |  |  |  |  |
| `extractbetween` | ✅ |  |  |  |  |  |
| `insertafter` | ✅ |  |  |  |  |  |
| `insertbefore` | ✅ |  |  |  |  |  |
| `iscellstr` | ✅ |  |  |  |  | predicate |
| `ischar` | ✅ |  |  |  |  |  |
| `isletter` | ✅ |  |  |  |  |  |
| `isspace` | ✅ |  |  |  |  |  |
| `isstring` | ✅ |  |  |  |  |  |
| `isstringscalar` | ✅ |  |  |  |  |  |
| `isstrprop` | ✅ |  |  |  |  |  |
| `join` | ✅ | 0.001 | 0.27× | — | OK | Join 24-element Greek-letter string array. 10k iters. |
| `lower` | ✅ |  |  |  |  |  |
| `matches` | ✅ |  |  |  |  |  |
| `newline` | ✅ | 0.000 | 0.10× | 7.25× | OK | ASCII LF char. Bench is 100k iters of the call itself. |
| `num2str` | ✅ |  |  |  |  |  |
| `pad` | ✅ |  |  |  |  |  |
| `plus` | ✅ |  |  |  |  | named-fn form added in Pack 11 |
| `regexp` | ✅ |  |  |  |  |  |
| `regexpi` | ✅ |  |  |  |  |  |
| `regexprep` | ✅ |  |  |  |  |  |
| `regexptranslate` | ✅ | 0.000 | 13.86× | 63.52× | OK | Escape 21-char string with many metachars. 10k iters. |
| `replace` | ✅ |  |  |  |  |  |
| `replacebetween` | ✅ |  |  |  |  |  |
| `reverse` | ✅ |  |  |  |  |  |
| `split` | ✅ | 0.102 | 0.99× | — | OK | Split CSV-like 4000-char string into 1000 tokens. 1000 iters. |
| `splitlines` | ✅ |  |  |  |  |  |
| `sprintf` | ✅ |  |  |  |  |  |
| `sscanf` | ✅ |  |  |  |  |  |
| `startswith` | ❌ |  |  |  |  |  |
| `str2double` | ✅ |  |  |  |  |  |
| `strcat` | ✅ |  |  |  |  |  |
| `strcmp` | ✅ |  |  |  |  |  |
| `strcmpi` | ✅ |  |  |  |  |  |
| `strfind` | ✅ |  |  |  |  |  |
| `string` | ✅ |  |  |  |  |  |
| `strings` | ✅ | 0.710 | 0.22× | — | OK | 100x100 empty-string array. 1000 iters. |
| `strip` | ✅ |  |  |  |  |  |
| `strjoin` | ✅ |  |  |  |  |  |
| `strjust` | ✅ | 0.084 | 2.07× | 2.42× | OK | 1000x50 char matrix, right-justify. 200 iters. |
| `strlength` | ✅ |  |  |  |  |  |
| `strncmp` | ✅ |  |  |  |  |  |
| `strncmpi` | ✅ |  |  |  |  |  |
| `strrep` | ✅ |  |  |  |  |  |
| `strsplit` | ✅ |  |  |  |  |  |
| `strtok` | ✅ |  |  |  |  |  |
| `strtrim` | ✅ |  |  |  |  |  |
| `upper` | ✅ |  |  |  |  |  |

## Structures

**Namespace:** core — 12 ✅ + 0 ⚠️ / 14 = 86%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `arrayfun` | ✅ |  |  |  |  |  |
| `cell2struct` | ✅ |  |  |  |  |  |
| `fieldnames` | ✅ |  |  |  |  |  |
| `getfield` | ✅ |  |  |  |  | dynamic field |
| `isfield` | ✅ |  |  |  |  |  |
| `isstruct` | ✅ |  |  |  |  |  |
| `orderfields` | ✅ |  |  |  |  | reorder |
| `rmfield` | ✅ |  |  |  |  |  |
| `setfield` | ✅ |  |  |  |  | dynamic field |
| `struct` | ✅ |  |  |  |  |  |
| `struct2cell` | ✅ |  |  |  |  |  |
| `struct2table` | ❌ |  |  |  |  |  |
| `structfun` | ✅ |  |  |  |  |  |
| `table2struct` | ❌ |  |  |  |  |  |

## Cell Arrays

**Namespace:** core — 12 ✅ + 0 ⚠️ / 17 = 71%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `cell` | ✅ |  |  |  |  |  |
| `cell2mat` | ✅ |  |  |  |  | concat cells |
| `cell2struct` | ✅ |  |  |  |  |  |
| `cell2table` | ❌ |  |  |  |  |  |
| `celldisp` | ✅ |  |  |  |  |  |
| `cellfun` | ✅ |  |  |  |  |  |
| `cellplot` | ❌ |  |  |  |  |  |
| `cellstr` | ✅ |  |  |  |  | cell of char rows |
| `iscell` | ✅ |  |  |  |  |  |
| `iscellstr` | ✅ |  |  |  |  | predicate |
| `mat2cell` | ✅ |  |  |  |  | split into cell |
| `num2cell` | ✅ |  |  |  |  | wrap each elem |
| `string` | ✅ |  |  |  |  |  |
| `struct2cell` | ✅ |  |  |  |  |  |
| `table` | ❌ |  |  |  |  |  |
| `table2cell` | ❌ |  |  |  |  |  |
| `timetable` | ❌ |  |  |  |  |  |

## Function Handles

**Namespace:** core — 5 ✅ + 0 ⚠️ / 6 = 83%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `feval` | ✅ |  |  |  |  | call handle by name |
| `func2str` | ✅ |  |  |  |  | inspect |
| `function_handle` | ❌ |  |  |  |  | OOP class |
| `functions` | ✅ |  |  |  |  | introspection |
| `localfunctions` | ✅ |  |  |  |  | (stub: empty cell) |
| `str2func` | ✅ |  |  |  |  | create handle |

## Categorical Arrays

**Namespace:** `categorical.*` (future) — 1 ✅ + 0 ⚠️ / 17 = 5%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `addcats` | ❌ |  |  |  |  |  |
| `categorical` | ❌ |  |  |  |  |  |
| `categories` | ❌ |  |  |  |  |  |
| `combinations` | ❌ |  |  |  |  | all combinations |
| `countcats` | ❌ |  |  |  |  |  |
| `discretize` | ✅ |  |  |  |  |  |
| `iscategory` | ❌ |  |  |  |  |  |
| `isordinal` | ❌ |  |  |  |  |  |
| `isprotected` | ❌ |  |  |  |  |  |
| `isundefined` | ❌ |  |  |  |  |  |
| `mergecats` | ❌ |  |  |  |  |  |
| `removecats` | ❌ |  |  |  |  |  |
| `renamecats` | ❌ |  |  |  |  |  |
| `reordercats` | ❌ |  |  |  |  |  |
| `setcats` | ❌ |  |  |  |  |  |
| `summary` | ❌ |  |  |  |  |  |

## Tables / Timetables

**Namespace:** `table.*` (future) — 6 ✅ + 0 ⚠️ / 66 = 9%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `addprop` | ❌ |  |  |  |  |  |
| `addvars` | ❌ |  |  |  |  |  |
| `anymissing` | ❌ |  |  |  |  |  |
| `array2table` | ❌ |  |  |  |  |  |
| `cell2table` | ❌ |  |  |  |  |  |
| `computebygroup` | ❌ |  |  |  |  |  |
| `convertvars` | ❌ |  |  |  |  |  |
| `fillmissing` | ❌ |  |  |  |  |  |
| `findgroups` | ❌ |  |  |  |  |  |
| `groupcounts` | ❌ |  |  |  |  |  |
| `groupfilter` | ❌ |  |  |  |  |  |
| `groupsummary` | ❌ |  |  |  |  |  |
| `grouptransform` | ❌ |  |  |  |  |  |
| `head` | ✅ |  |  |  |  |  |
| `height` | ❌ |  |  |  |  |  |
| `inner2outer` | ❌ |  |  |  |  |  |
| `innerjoin` | ❌ |  |  |  |  |  |
| `intersect` | ✅ |  |  |  |  |  |
| `ismember` | ✅ |  |  |  |  |  |
| `ismissing` | ❌ |  |  |  |  |  |
| `issortedrows` | ❌ |  |  |  |  |  |
| `join` | ✅ | 0.001 | 0.27× | — | OK | Join 24-element Greek-letter string array. 10k iters. |
| `jointables` | ❌ |  |  |  |  |  |
| `mergevars` | ❌ |  |  |  |  |  |
| `movevars` | ❌ |  |  |  |  |  |
| `outerjoin` | ❌ |  |  |  |  |  |
| `parquetread` | ❌ |  |  |  |  |  |
| `parquetwrite` | ❌ |  |  |  |  |  |
| `pivot` | ❌ |  |  |  |  |  |
| `pivottable` | ❌ |  |  |  |  |  |
| `readtable` | ❌ |  |  |  |  | needs table type |
| `removevars` | ❌ |  |  |  |  |  |
| `renamevars` | ❌ |  |  |  |  |  |
| `rmmissing` | ❌ |  |  |  |  |  |
| `rmprop` | ❌ |  |  |  |  |  |
| `rowfun` | ❌ |  |  |  |  |  |
| `rows2vars` | ❌ |  |  |  |  |  |
| `setdiff` | ✅ |  |  |  |  |  |
| `setxor` | ✅ |  |  |  |  | symmetric set diff |
| `sortrows` | ✅ |  |  |  |  |  |
| `splitapply` | ❌ |  |  |  |  |  |
| `splitvars` | ❌ |  |  |  |  |  |
| `stack` | ❌ |  |  |  |  |  |
| `stackedplot` | ❌ |  |  |  |  |  |
| `stacktablevariables` | ❌ |  |  |  |  |  |
| `standardizemissing` | ❌ |  |  |  |  |  |
| `struct2table` | ❌ |  |  |  |  |  |
| `summary` | ❌ |  |  |  |  |  |
| `table` | ❌ |  |  |  |  |  |
| `table2array` | ❌ |  |  |  |  |  |
| `table2cell` | ❌ |  |  |  |  |  |
| `table2struct` | ❌ |  |  |  |  |  |
| `table2timetable` | ❌ |  |  |  |  |  |
| `tail` | ✅ |  |  |  |  |  |
| `timetable2table` | ❌ |  |  |  |  |  |
| `topkrows` | ❌ |  |  |  |  |  |
| `union` | ✅ |  |  |  |  |  |
| `unique` | ✅ |  |  |  |  |  |
| `unstack` | ❌ |  |  |  |  |  |
| `unstacktablevariables` | ❌ |  |  |  |  |  |
| `varfun` | ❌ |  |  |  |  |  |
| `vartype` | ❌ |  |  |  |  |  |
| `width` | ❌ |  |  |  |  |  |
| `writetable` | ❌ |  |  |  |  | needs table type |

## Bit-wise Operations

**Namespace:** core — 7 ✅ + 0 ⚠️ / 8 = 88%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `bitand` | ✅ |  |  |  |  |  |
| `bitcmp` | ✅ |  |  |  |  |  |
| `bitget` | ✅ |  |  |  |  | get bit |
| `bitor` | ✅ |  |  |  |  |  |
| `bitset` | ✅ |  |  |  |  | set bit |
| `bitshift` | ✅ |  |  |  |  |  |
| `bitxor` | ✅ |  |  |  |  |  |
| `swapbytes` | ✅ | 1.049 | 1.04× | 7.60× | OK | 1M uint32 byte-swap. 50 iters. |

## Set Operations

**Namespace:** core — 10 ✅ + 0 ⚠️ / 13 = 77%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `allunique` | ✅ |  |  |  |  | distinct check |
| `innerjoin` | ❌ |  |  |  |  |  |
| `intersect` | ✅ |  |  |  |  |  |
| `ismember` | ✅ |  |  |  |  |  |
| `ismembertol` | ✅ |  |  |  |  | tol variant |
| `join` | ✅ | 0.001 | 0.27× | — | OK | Join 24-element Greek-letter string array. 10k iters. |
| `numunique` | ✅ |  |  |  |  | count distinct |
| `outerjoin` | ❌ |  |  |  |  |  |
| `setdiff` | ✅ |  |  |  |  |  |
| `setxor` | ✅ |  |  |  |  | symmetric set diff |
| `union` | ✅ |  |  |  |  |  |
| `unique` | ✅ |  |  |  |  |  |
| `uniquetol` | ✅ |  |  |  |  | tol variant |

## Arithmetic

**Namespace:** core — 28 ✅ + 0 ⚠️ / 34 = 82%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `bsxfun` | ✅ |  |  |  |  | legacy broadcast |
| `ceil` | ✅ |  |  |  |  |  |
| `ctranspose` | ✅ |  |  |  |  | named-fn form added in Pack 11 |
| `cumprod` | ✅ |  |  |  |  |  |
| `cumsum` | ✅ |  |  |  |  |  |
| `diff` | ✅ |  |  |  |  |  |
| `fix` | ✅ |  |  |  |  |  |
| `floor` | ✅ |  |  |  |  |  |
| `idivide` | ✅ |  |  |  |  | integer division |
| `ldivide` | ✅ |  |  |  |  | named-fn form added in Pack 11 |
| `minus` | ✅ |  |  |  |  | named-fn form added in Pack 11 |
| `mldivide` | ✅ |  |  |  |  | named-fn form added in Pack 11 |
| `mod` | ✅ |  |  |  |  |  |
| `movsum` | ✅ |  |  |  |  | moving sum |
| `mpower` | ✅ |  |  |  |  | named-fn form added in Pack 11 |
| `mrdivide` | ✅ |  |  |  |  | named-fn form added in Pack 11 |
| `mtimes` | ✅ |  |  |  |  | named-fn form added in Pack 11 |
| `pagectranspose` | ✅ | 0.207 | 0.24× | 0.23× | OK | 128x64x8 real-valued — pagectranspose equals pagetranspose. 100 iters. |
| `pagemldivide` | ❌ |  |  |  |  |  |
| `pagemrdivide` | ❌ |  |  |  |  |  |
| `pagemtimes` | ✅ |  |  |  |  |  |
| `pagetranspose` | ✅ | 0.083 | 1.11× | 0.63× | OK | 128x64x8 array, page-wise transpose. 100 iters. |
| `plus` | ✅ |  |  |  |  | named-fn form added in Pack 11 |
| `power` | ✅ |  |  |  |  | named-fn form added in Pack 11 |
| `prod` | ✅ |  |  |  |  |  |
| `rdivide` | ✅ |  |  |  |  | named-fn form added in Pack 11 |
| `rem` | ✅ |  |  |  |  |  |
| `round` | ✅ |  |  |  |  |  |
| `sum` | ✅ |  |  |  |  |  |
| `tensorprod` | ❌ |  |  |  |  | tensor contraction |
| `times` | ✅ |  |  |  |  | named-fn form added in Pack 11 |
| `transpose` | ✅ |  |  |  |  | named-fn form added in Pack 11 |
| `uminus` | ✅ |  |  |  |  | named-fn form added in Pack 11 |
| `uplus` | ✅ |  |  |  |  | named-fn form added in Pack 11 |

## Trigonometry

**Namespace:** core — 47 ✅ + 0 ⚠️ / 47 = 100%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `acos` | ✅ |  |  |  |  |  |
| `acosd` | ✅ |  |  |  |  | degree |
| `acosh` | ✅ |  |  |  |  | hyperbolic |
| `acot` | ✅ |  |  |  |  |  |
| `acotd` | ✅ |  |  |  |  |  |
| `acoth` | ✅ |  |  |  |  |  |
| `acsc` | ✅ |  |  |  |  |  |
| `acscd` | ✅ |  |  |  |  |  |
| `acsch` | ✅ |  |  |  |  |  |
| `asec` | ✅ |  |  |  |  |  |
| `asecd` | ✅ |  |  |  |  |  |
| `asech` | ✅ |  |  |  |  |  |
| `asin` | ✅ |  |  |  |  |  |
| `asind` | ✅ |  |  |  |  | degree |
| `asinh` | ✅ |  |  |  |  | hyperbolic |
| `atan` | ✅ |  |  |  |  |  |
| `atan2` | ✅ |  |  |  |  |  |
| `atan2d` | ✅ |  |  |  |  | degree |
| `atand` | ✅ |  |  |  |  | degree |
| `atanh` | ✅ |  |  |  |  | hyperbolic |
| `cart2pol` | ✅ |  |  |  |  | coord xform |
| `cart2sph` | ✅ |  |  |  |  | coord xform |
| `cos` | ✅ |  |  |  |  |  |
| `cosd` | ✅ |  |  |  |  | degree |
| `cosh` | ✅ |  |  |  |  | hyperbolic |
| `cospi` | ✅ |  |  |  |  | use `cos(pi*x)` |
| `cot` | ✅ |  |  |  |  | reciprocal |
| `cotd` | ✅ |  |  |  |  |  |
| `coth` | ✅ |  |  |  |  |  |
| `csc` | ✅ |  |  |  |  | reciprocal |
| `cscd` | ✅ |  |  |  |  |  |
| `csch` | ✅ |  |  |  |  |  |
| `deg2rad` | ✅ |  |  |  |  |  |
| `hypot` | ✅ |  |  |  |  |  |
| `pol2cart` | ✅ |  |  |  |  | coord xform |
| `rad2deg` | ✅ |  |  |  |  |  |
| `sec` | ✅ |  |  |  |  | reciprocal |
| `secd` | ✅ |  |  |  |  |  |
| `sech` | ✅ |  |  |  |  |  |
| `sin` | ✅ |  |  |  |  |  |
| `sind` | ✅ |  |  |  |  | degree |
| `sinh` | ✅ |  |  |  |  | hyperbolic |
| `sinpi` | ✅ |  |  |  |  | use `sin(pi*x)` |
| `sph2cart` | ✅ |  |  |  |  | coord xform |
| `tan` | ✅ |  |  |  |  |  |
| `tand` | ✅ |  |  |  |  | degree |
| `tanh` | ✅ |  |  |  |  | hyperbolic |

## Exponents and Logarithms

**Namespace:** core — 13 ✅ + 0 ⚠️ / 13 = 100%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `exp` | ✅ |  |  |  |  |  |
| `expm1` | ✅ |  |  |  |  |  |
| `log` | ✅ |  |  |  |  |  |
| `log10` | ✅ |  |  |  |  |  |
| `log1p` | ✅ |  |  |  |  |  |
| `log2` | ✅ |  |  |  |  |  |
| `nextpow2` | ✅ |  |  |  |  |  |
| `nthroot` | ✅ |  |  |  |  |  |
| `pow2` | ✅ |  |  |  |  |  |
| `reallog` | ✅ |  |  |  |  |  |
| `realpow` | ✅ |  |  |  |  |  |
| `realsqrt` | ✅ |  |  |  |  |  |
| `sqrt` | ✅ |  |  |  |  |  |

## Special Functions

**Namespace:** core — 20 ✅ + 0 ⚠️ / 24 = 83%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `airy` | ✅ | 7.487 | 0.10× | 0.38× | OK | Ai over 10k pts on [-5,5]. 10 iters. Element-wise comparison. |
| `besselh` | ✅ |  |  |  |  |  |
| `besseli` | ✅ |  |  |  |  |  |
| `besselj` | ✅ |  |  |  |  |  |
| `besselk` | ✅ |  |  |  |  |  |
| `bessely` | ✅ |  |  |  |  |  |
| `beta` | ✅ |  |  |  |  |  |
| `betainc` | ✅ |  |  |  |  |  |
| `betaincinv` | ✅ | 1.119 | 1.04× | 4.43× | OK | Inverse regularized beta over 2k probability points, a=3 b=5. 20 iters, element-wise. |
| `betaln` | ✅ |  |  |  |  |  |
| `ellipj` | ✅ | 0.614 | 2.23× | 1.41× | OK | Jacobi sn over 5k pts at m=0.7. 50 iters, element-wise on sn. |
| `ellipke` | ✅ |  |  |  |  |  |
| `erf` | ✅ | 9.174 | 0.28× | 0.78× | OK | smoke-test (already implemented). N=1e6, mean over 10 iters. |
| `erfc` | ✅ |  |  |  |  |  |
| `erfcinv` | ✅ |  |  |  |  |  |
| `erfcx` | ✅ |  |  |  |  |  |
| `erfinv` | ✅ |  |  |  |  |  |
| `expint` | ✅ |  |  |  |  |  |
| `gamma` | ✅ |  |  |  |  |  |
| `gammainc` | ✅ |  |  |  |  |  |
| `gammaincinv` | ✅ | 1.725 | 1.18× | 23.31× | OK | Inverse regularized gamma over 5k probability points, a=2.5. 20 iters, element-wise. |
| `gammaln` | ✅ |  |  |  |  |  |
| `legendre` | ✅ |  |  |  |  |  |
| `psi` | ✅ |  |  |  |  |  |

## Discrete Math

**Namespace:** core — 10 ✅ + 0 ⚠️ / 11 = 90%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `factor` | ✅ |  |  |  |  |  |
| `factorial` | ✅ |  |  |  |  |  |
| `gcd` | ✅ |  |  |  |  |  |
| `isprime` | ✅ |  |  |  |  |  |
| `lcm` | ✅ |  |  |  |  |  |
| `matchpairs` | ❌ |  |  |  |  |  |
| `nchoosek` | ✅ |  |  |  |  |  |
| `perms` | ✅ |  |  |  |  |  |
| `primes` | ✅ |  |  |  |  |  |
| `rat` | ✅ |  |  |  |  |  |
| `rats` | ✅ |  |  |  |  |  |

## Polynomials

**Namespace:** core — 10 ✅ + 0 ⚠️ / 12 = 83%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `conv` | ✅ |  |  |  |  |  |
| `deconv` | ✅ |  |  |  |  |  |
| `poly` | ✅ |  |  |  |  | roots → coeffs |
| `polyder` | ✅ |  |  |  |  |  |
| `polydiv` | ✅ |  |  |  |  |  |
| `polyeig` | ❌ |  |  |  |  | poly eig |
| `polyfit` | ✅ |  |  |  |  |  |
| `polyint` | ✅ |  |  |  |  |  |
| `polyval` | ✅ |  |  |  |  |  |
| `polyvalm` | ✅ |  |  |  |  | matrix poly eval |
| `residue` | ❌ |  |  |  |  | partial-fraction |
| `roots` | ✅ |  |  |  |  |  |

## Linear Algebra

**Namespace:** `linalg.*` (future) — 12 ✅ + 0 ⚠️ / 82 = 15%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `balance` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `bandwidth` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `cdf2rdf` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `chol` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `cholupdate` | ❌ |  |  |  |  |  |
| `cond` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `condeig` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `condest` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `cross` | ✅ |  |  |  |  |  |
| `ctranspose` | ✅ |  |  |  |  | named-fn form added in Pack 11 |
| `decomposition` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `det` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `dot` | ✅ |  |  |  |  |  |
| `eig` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `eigs` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `expm` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `expmv` | ❌ |  |  |  |  |  |
| `funm` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `gsvd` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `hess` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `inv` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `isbanded` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `isdiag` | ❌ |  |  |  |  |  |
| `ishermitian` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `issymmetric` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `istril` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `istriu` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `kron` | ✅ |  |  |  |  |  |
| `ldl` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `linsolve` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `logm` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `lscov` | ❌ |  |  |  |  |  |
| `lsqminnorm` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `lsqnonneg` | ❌ |  |  |  |  |  |
| `lu` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `mldivide` | ✅ |  |  |  |  | named-fn form added in Pack 11 |
| `mpower` | ✅ |  |  |  |  | named-fn form added in Pack 11 |
| `mrdivide` | ✅ |  |  |  |  | named-fn form added in Pack 11 |
| `mtimes` | ✅ |  |  |  |  | named-fn form added in Pack 11 |
| `norm` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `normest` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `null` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `ordeig` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `ordqz` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `ordschur` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `orth` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `pagectranspose` | ✅ | 0.207 | 0.24× | 0.23× | OK | 128x64x8 real-valued — pagectranspose equals pagetranspose. 100 iters. |
| `pageeig` | ❌ |  |  |  |  |  |
| `pageinv` | ❌ |  |  |  |  |  |
| `pagelsqminnorm` | ❌ |  |  |  |  |  |
| `pagemldivide` | ❌ |  |  |  |  |  |
| `pagemrdivide` | ❌ |  |  |  |  |  |
| `pagemtimes` | ✅ |  |  |  |  |  |
| `pagenorm` | ❌ |  |  |  |  |  |
| `pagepinv` | ❌ |  |  |  |  |  |
| `pagesvd` | ❌ |  |  |  |  |  |
| `pagetranspose` | ✅ | 0.083 | 1.11× | 0.63× | OK | 128x64x8 array, page-wise transpose. 100 iters. |
| `pinv` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `planerot` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `polyeig` | ❌ |  |  |  |  | poly eig |
| `qr` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `qrdelete` | ❌ |  |  |  |  |  |
| `qrinsert` | ❌ |  |  |  |  |  |
| `qrupdate` | ❌ |  |  |  |  |  |
| `qz` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `rank` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `rcond` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `rref` | ❌ |  |  |  |  |  |
| `rsf2csf` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `schur` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `sqrtm` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `subspace` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `svd` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `svdappend` | ❌ |  |  |  |  |  |
| `svds` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `svdsketch` | ❌ |  |  |  |  |  |
| `sylvester` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `trace` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `transpose` | ✅ |  |  |  |  | named-fn form added in Pack 11 |
| `tril` | ✅ |  |  |  |  |  |
| `triu` | ✅ |  |  |  |  |  |
| `vecnorm` | ❌ |  |  |  |  | **deferred — libs/linalg** |

## Random Number Generation

**Namespace:** core — 5 ✅ + 0 ⚠️ / 6 = 83%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `rand` | ✅ |  |  |  |  |  |
| `randi` | ✅ |  |  |  |  |  |
| `randn` | ✅ |  |  |  |  |  |
| `randperm` | ✅ |  |  |  |  |  |
| `randstream` | ❌ |  |  |  |  |  |
| `rng` | ✅ |  |  |  |  |  |

## Interpolation

**Namespace:** core — 11 ✅ + 0 ⚠️ / 18 = 61%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `griddata` | ❌ |  |  |  |  |  |
| `griddatan` | ❌ |  |  |  |  |  |
| `griddedinterpolant` | ❌ |  |  |  |  |  |
| `interp1` | ✅ |  |  |  |  |  |
| `interp2` | ✅ |  |  |  |  |  |
| `interp3` | ✅ |  |  |  |  |  |
| `interpft` | ✅ | 0.012 | 2.33× | 16.15× | OK | 256-pt band-limited signal interpolated to 1024 points. 200 iters, element-wise. |
| `interpn` | ✅ |  |  |  |  |  |
| `makima` | ❌ |  |  |  |  |  |
| `meshgrid` | ✅ |  |  |  |  |  |
| `mkpp` | ✅ |  |  |  |  |  |
| `ndgrid` | ✅ |  |  |  |  |  |
| `padecoef` | ✅ | 0.000 | 3.03× | 158.05× | OK | Pade(10,10) of e^{-1.5s} numerator coefficients. 10k iters. Octave's padecoef (control pkg) uses a different normalization — comparison reference is MATLAB. |
| `pchip` | ✅ |  |  |  |  |  |
| `ppval` | ✅ |  |  |  |  |  |
| `scatteredinterpolant` | ❌ |  |  |  |  |  |
| `spline` | ✅ |  |  |  |  |  |
| `unmkpp` | ✅ |  |  |  |  |  |

## Optimization

**Namespace:** core — 5 ✅ + 0 ⚠️ / 7 = 71%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `fminbnd` | ✅ |  |  |  |  | 1-D bounded |
| `fminsearch` | ✅ |  |  |  |  | Nelder-Mead |
| `fzero` | ✅ |  |  |  |  |  |
| `lsqnonneg` | ❌ |  |  |  |  |  |
| `optimget` | ✅ |  |  |  |  |  |
| `optimize` | ❌ |  |  |  |  |  |
| `optimset` | ✅ |  |  |  |  |  |

## Ordinary Differential Equations

**Namespace:** `ode.*` (future) — 0 ✅ + 0 ⚠️ / 21 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `decic` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `deval` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `ode` | ❌ |  |  |  |  |  |
| `ode113` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `ode15i` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `ode15s` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `ode23` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `ode23s` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `ode23t` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `ode23tb` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `ode45` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `ode78` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `ode89` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `odeevent` | ❌ |  |  |  |  |  |
| `odeget` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `odejacobian` | ❌ |  |  |  |  |  |
| `odemassmatrix` | ❌ |  |  |  |  |  |
| `odesensitivity` | ❌ |  |  |  |  |  |
| `odeset` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `odextend` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `solveode` | ❌ |  |  |  |  |  |

## Sparse Matrices

**Namespace:** `sparse.*` (future) — 4 ✅ + 0 ⚠️ / 53 = 7%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `amd` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `bicg` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `bicgstab` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `bicgstabl` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `cgs` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `colamd` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `colperm` | ❌ |  |  |  |  |  |
| `condest` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `dissect` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `dmperm` | ❌ |  |  |  |  |  |
| `eigs` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `equilibrate` | ❌ |  |  |  |  |  |
| `etree` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `etreeplot` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `find` | ✅ |  |  |  |  |  |
| `full` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `gmres` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `gplot` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `ichol` | ❌ |  |  |  |  |  |
| `ilu` | ❌ |  |  |  |  |  |
| `issparse` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `lsqr` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `minres` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `nnz` | ✅ |  |  |  |  |  |
| `nonzeros` | ✅ |  |  |  |  |  |
| `normest` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `nzmax` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `pcg` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `qmr` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `randperm` | ✅ |  |  |  |  |  |
| `spalloc` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `sparse` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `spaugment` | ❌ |  |  |  |  |  |
| `spconvert` | ❌ |  |  |  |  |  |
| `spdiags` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `speye` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `spfun` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `spones` | ❌ |  |  |  |  |  |
| `spparms` | ❌ |  |  |  |  |  |
| `sprand` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `sprandn` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `sprandsym` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `sprank` | ❌ |  |  |  |  |  |
| `spy` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `svds` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `symamd` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `symbfact` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `symmlq` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `symrcm` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `tfqmr` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `treelayout` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `treeplot` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `unmesh` | ❌ |  |  |  |  | **deferred — libs/sparse** |

## Fourier Analysis and Filtering

**Namespace:** `signal.transforms.*` + 6 promotions in core (`fft, ifft, fftshift, ifftshift, conv, xcorr`) — 8 ✅ + 0 ⚠️ / 21 = 38%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `conv` | ✅ |  |  |  |  |  |
| `conv2` | ✅ | 0.318 | 0.25× | 0.34× | OK | 128x128 image, 7x7 averaging kernel, 'same' shape. 100 iters. |
| `convn` | ✅ | 0.028 | 2.06× | 0.85× | OK | 64x64 2-D image / convn dispatch (delegates to conv2). 100 iters. |
| `deconv` | ✅ |  |  |  |  |  |
| `fft` | ✅ |  |  |  |  |  |
| `fft2` | ✅ | 1.127 | 0.60× | 0.58× | OK | 256x256 deterministic test signal, complex 2-D FFT. 50 iters. |
| `fftn` | ❌ |  |  |  |  | N-D FFT |
| `fftshift` | ✅ |  |  |  |  |  |
| `fftw` | ❌ |  |  |  |  | wisdom file |
| `filter` | ✅ |  |  |  |  |  |
| `filter2` | ✅ | 0.141 | 0.51× | 0.34× | OK | 128x128 image with 3x3 Laplacian kernel. 100 iters. |
| `ifft` | ✅ |  |  |  |  |  |
| `ifft2` | ✅ | 1.840 | 0.38× | 0.57× | OK | 256x256 inverse 2-D FFT (after fft2 of deterministic signal). 50 iters. |
| `ifftn` | ❌ |  |  |  |  | N-D FFT |
| `ifftshift` | ✅ |  |  |  |  |  |
| `interpft` | ✅ | 0.012 | 2.33× | 16.15× | OK | 256-pt band-limited signal interpolated to 1024 points. 200 iters, element-wise. |
| `nextpow2` | ✅ |  |  |  |  |  |
| `nufft` | ❌ |  |  |  |  | non-uniform |
| `nufftn` | ❌ |  |  |  |  | non-uniform |
| `padecoef` | ✅ | 0.000 | 3.03× | 158.05× | OK | Pade(10,10) of e^{-1.5s} numerator coefficients. 10k iters. Octave's padecoef (control pkg) uses a different normalization — comparison reference is MATLAB. |
| `ss2tf` | ✅ |  |  |  |  | inverse |

## Descriptive Statistics

**Namespace:** `stats.descriptive.*` / `stats.moving.*` / `stats.nan.*`. Exception: `xcorr/xcov/rms/rssq/peak2peak/peak2rms` → `signal.*` (signal-side stats) — 14 ✅ + 0 ⚠️ / 33 = 42%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `bounds` | ✅ |  |  |  |  | `[min,max]` |
| `corrcoef` | ✅ |  |  |  |  |  |
| `cov` | ✅ |  |  |  |  |  |
| `cummax` | ✅ |  |  |  |  |  |
| `cummin` | ✅ |  |  |  |  |  |
| `iqr` | ✅ |  |  |  |  | inter-quartile |
| `kde` | ❌ |  |  |  |  |  |
| `mape` | ✅ | 9.431 | 0.28× | 0.98× | OK | 1M-point MAPE. 50 iters. numkit needs `import compat.*`; MATLAB+Octave have it flat. |
| `max` | ✅ |  |  |  |  |  |
| `maxk` | ✅ |  |  |  |  |  |
| `mean` | ✅ |  |  |  |  |  |
| `median` | ✅ |  |  |  |  |  |
| `min` | ✅ |  |  |  |  |  |
| `mink` | ✅ |  |  |  |  |  |
| `mode` | ✅ |  |  |  |  |  |
| `movmad` | ✅ |  |  |  |  | moving mad |
| `movmax` | ✅ |  |  |  |  | moving max |
| `movmean` | ✅ |  |  |  |  | moving avg |
| `movmedian` | ✅ |  |  |  |  | moving median |
| `movmin` | ✅ |  |  |  |  | moving min |
| `movprod` | ✅ |  |  |  |  | moving prod |
| `movstd` | ✅ |  |  |  |  | moving std |
| `movsum` | ✅ |  |  |  |  | moving sum |
| `movvar` | ✅ |  |  |  |  | moving var |
| `prctile` | ✅ |  |  |  |  |  |
| `quantile` | ✅ |  |  |  |  |  |
| `rms` | ✅ |  |  |  |  | root-mean-square |
| `rmse` | ✅ |  |  |  |  |  |
| `std` | ✅ |  |  |  |  |  |
| `summary` | ❌ |  |  |  |  |  |
| `var` | ✅ |  |  |  |  |  |
| `xcorr` | ✅ |  |  |  |  | cross-correlation |
| `xcov` | ✅ | 0.984 | 0.41× | — | OK | Cross-cov of 5k-pt sine. 50 iters. |

## Workspace

**Namespace:** core — 8 ✅ + 0 ⚠️ / 10 = 80%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `clear` | ✅ |  |  |  |  |  |
| `clearvars` | ✅ |  |  |  |  |  |
| `disp` | ✅ |  |  |  |  |  |
| `formatteddisplaytext` | ✅ |  |  |  |  |  |
| `load` | ✅ |  |  |  |  |  |
| `openvar` | ❌ |  |  |  |  | IDE |
| `save` | ✅ |  |  |  |  |  |
| `who` | ✅ |  |  |  |  |  |
| `whos` | ✅ |  |  |  |  |  |
| `workspacebrowser` | ❌ |  |  |  |  |  |

## Error Handling (basic)

**Namespace:** core — 4 ✅ + 0 ⚠️ / 6 = 66%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `assert` | ✅ |  |  |  |  |  |
| `error` | ✅ |  |  |  |  |  |
| `lastwarn` | ✅ | 0.000 | 1.20× | 8.46× | OK | Read last warning state. 100k iters, scalar timing. |
| `oncleanup` | ❌ |  |  |  |  |  |
| `try` | ✅ |  |  |  |  | keyword (`try/catch`) |
| `warning` | ✅ |  |  |  |  |  |

## Exception Handling

**Namespace:** core (keyword + class) — 2 ✅ + 0 ⚠️ / 2 = 100%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `mexception` | ✅ |  |  |  |  | MATLAB exception class — registered as `MException` |
| `try` | ✅ |  |  |  |  | keyword (`try/catch`) |

## Line Plots

**Namespace:** `graphics.line.*` — 2 ✅ + 0 ⚠️ / 12 = 16%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `area` | ❌ |  |  |  |  |  |
| `errorbar` | ❌ |  |  |  |  |  |
| `fimplicit` | ❌ |  |  |  |  |  |
| `fplot` | ❌ |  |  |  |  |  |
| `fplot3` | ❌ |  |  |  |  |  |
| `loglog` | ❌ |  |  |  |  |  |
| `plot` | ✅ |  |  |  |  |  |
| `plot3` | ❌ |  |  |  |  | 3-D |
| `semilogx` | ❌ |  |  |  |  |  |
| `semilogy` | ❌ |  |  |  |  |  |
| `stackedplot` | ❌ |  |  |  |  |  |
| `stairs` | ✅ |  |  |  |  |  |

## Polar Plots

**Namespace:** `graphics.polar.*` — 3 ✅ + 0 ⚠️ / 19 = 15%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `compassplot` | ❌ |  |  |  |  |  |
| `fpolarplot` | ❌ |  |  |  |  |  |
| `polaraxes` | ❌ |  |  |  |  |  |
| `polarbubblechart` | ❌ |  |  |  |  |  |
| `polarhistogram` | ❌ |  |  |  |  |  |
| `polarplot` | ✅ |  |  |  |  |  |
| `polarregion` | ❌ |  |  |  |  |  |
| `polarscatter` | ❌ |  |  |  |  |  |
| `radiusregion` | ❌ |  |  |  |  |  |
| `rlim` | ✅ |  |  |  |  |  |
| `rtickangle` | ❌ |  |  |  |  |  |
| `rtickformat` | ❌ |  |  |  |  |  |
| `rticklabels` | ❌ |  |  |  |  |  |
| `rticks` | ❌ |  |  |  |  |  |
| `thetalim` | ✅ |  |  |  |  |  |
| `thetaregion` | ❌ |  |  |  |  |  |
| `thetatickformat` | ❌ |  |  |  |  |  |
| `thetaticklabels` | ❌ |  |  |  |  |  |
| `thetaticks` | ❌ |  |  |  |  |  |

## Contour Plots

**Namespace:** `graphics.contour.*` — 2 ✅ + 0 ⚠️ / 7 = 28%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `clabel` | ❌ |  |  |  |  |  |
| `contour` | ✅ |  |  |  |  |  |
| `contour3` | ❌ |  |  |  |  |  |
| `contourc` | ❌ |  |  |  |  |  |
| `contourf` | ✅ |  |  |  |  |  |
| `contourslice` | ❌ |  |  |  |  |  |
| `fcontour` | ❌ |  |  |  |  |  |

## Vector Fields

**Namespace:** `graphics.vector_fields.*` — 0 ✅ + 0 ⚠️ / 6 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `compassplot` | ❌ |  |  |  |  |  |
| `feather` | ❌ |  |  |  |  |  |
| `quiver` | ❌ |  |  |  |  |  |
| `quiver3` | ❌ |  |  |  |  |  |
| `streamline` | ❌ |  |  |  |  |  |
| `streamslice` | ❌ |  |  |  |  |  |

## Surface and Mesh Plots

**Namespace:** `graphics.surface.*` — 3 ✅ + 0 ⚠️ / 21 = 14%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `contour3` | ❌ |  |  |  |  |  |
| `cylinder` | ✅ | 0.125 | 0.48× | 2.75× | OK | 201-point sin-shaped profile, 50 angular samples. 200 iters. |
| `ellipsoid` | ✅ | 0.094 | 1.00× | 4.19× | OK | 101x101 ellipsoid (1,2,3) center, (4,5,6) semi-axes. 200 iters. |
| `fimplicit3` | ❌ |  |  |  |  |  |
| `fmesh` | ❌ |  |  |  |  |  |
| `fsurf` | ❌ |  |  |  |  |  |
| `hidden` | ❌ |  |  |  |  |  |
| `mesh` | ✅ |  |  |  |  |  |
| `meshc` | ❌ |  |  |  |  |  |
| `meshz` | ❌ |  |  |  |  |  |
| `pcolor` | ✅ |  |  |  |  |  |
| `peaks` | ✅ | 0.365 | 1.71× | 5.05× | OK | 200x200 peaks() surface. 50 iters, element-wise. |
| `ribbon` | ❌ |  |  |  |  |  |
| `sphere` | ✅ | 0.090 | 0.55× | 3.55× | OK | Unit sphere on 101x101 grid. 200 iters, element-wise on Z. |
| `surf` | ✅ |  |  |  |  |  |
| `surf2patch` | ❌ |  |  |  |  |  |
| `surface` | ❌ |  |  |  |  |  |
| `surfc` | ❌ |  |  |  |  |  |
| `surfl` | ❌ |  |  |  |  |  |
| `surfnorm` | ❌ |  |  |  |  |  |
| `waterfall` | ❌ |  |  |  |  |  |

## Volume Visualization

**Namespace:** `graphics.volume.*` — 0 ✅ + 0 ⚠️ / 24 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `coneplot` | ❌ |  |  |  |  |  |
| `contourslice` | ❌ |  |  |  |  |  |
| `curl` | ❌ |  |  |  |  |  |
| `divergence` | ❌ |  |  |  |  |  |
| `flow` | ❌ |  |  |  |  |  |
| `interpstreamspeed` | ❌ |  |  |  |  |  |
| `isocaps` | ❌ |  |  |  |  |  |
| `isocolors` | ❌ |  |  |  |  |  |
| `isonormals` | ❌ |  |  |  |  |  |
| `isosurface` | ❌ |  |  |  |  |  |
| `reducepatch` | ❌ |  |  |  |  |  |
| `reducevolume` | ❌ |  |  |  |  |  |
| `shrinkfaces` | ❌ |  |  |  |  |  |
| `slice` | ❌ |  |  |  |  |  |
| `smooth3` | ❌ |  |  |  |  |  |
| `stream2` | ❌ |  |  |  |  |  |
| `stream3` | ❌ |  |  |  |  |  |
| `streamline` | ❌ |  |  |  |  |  |
| `streamparticles` | ❌ |  |  |  |  |  |
| `streamribbon` | ❌ |  |  |  |  |  |
| `streamslice` | ❌ |  |  |  |  |  |
| `streamtube` | ❌ |  |  |  |  |  |
| `subvolume` | ❌ |  |  |  |  |  |
| `volumebounds` | ❌ |  |  |  |  |  |

## Geographic Plots

**Namespace:** `graphics.geographic.*` — 0 ✅ + 0 ⚠️ / 8 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `geoaxes` | ❌ |  |  |  |  |  |
| `geobasemap` | ❌ |  |  |  |  |  |
| `geobubble` | ❌ |  |  |  |  |  |
| `geodensityplot` | ❌ |  |  |  |  |  |
| `geolimits` | ❌ |  |  |  |  |  |
| `geoplot` | ❌ |  |  |  |  |  |
| `geoscatter` | ❌ |  |  |  |  |  |
| `geotickformat` | ❌ |  |  |  |  |  |

## Low-Level File I/O

**Namespace:** `io.file_io.*` — 13 ✅ + 0 ⚠️ / 15 = 86%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `fclose` | ✅ |  |  |  |  |  |
| `feof` | ✅ |  |  |  |  |  |
| `ferror` | ✅ |  |  |  |  |  |
| `fgetl` | ✅ |  |  |  |  |  |
| `fgets` | ✅ |  |  |  |  |  |
| `fileread` | ✅ |  |  |  |  | whole-file read |
| `fopen` | ✅ |  |  |  |  |  |
| `fprintf` | ✅ |  |  |  |  |  |
| `fread` | ✅ |  |  |  |  |  |
| `frewind` | ✅ |  |  |  |  |  |
| `fscanf` | ✅ |  |  |  |  |  |
| `fseek` | ✅ |  |  |  |  |  |
| `ftell` | ✅ |  |  |  |  |  |
| `fwrite` | ✅ |  |  |  |  |  |
| `openedfiles` | ❌ |  |  |  |  |  |

## Text Files (CSV / dlm / readtable)

**Namespace:** `io.text.*`. Exception: `readtable/writetable/readtimetable/writetimetable` → `table.*` (future) — 1 ✅ + 0 ⚠️ / 16 = 6%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `fileread` | ✅ |  |  |  |  | whole-file read |
| `importdatatask` | ❌ |  |  |  |  |  |
| `importtool` | ❌ |  |  |  |  |  |
| `readcell` | ❌ |  |  |  |  |  |
| `readlines` | ✅ |  |  |  |  |  |
| `readmatrix` | ✅ |  |  |  |  | modern CSV |
| `readtable` | ❌ |  |  |  |  | needs table type |
| `readtimetable` | ❌ |  |  |  |  |  |
| `readvars` | ❌ |  |  |  |  |  |
| `textscan` | ✅ |  |  |  |  |  |
| `type` | ✅ |  |  |  |  |  |
| `writecell` | ❌ |  |  |  |  |  |
| `writelines` | ✅ |  |  |  |  |  |
| `writematrix` | ✅ |  |  |  |  | modern CSV |
| `writetable` | ❌ |  |  |  |  | needs table type |
| `writetimetable` | ❌ |  |  |  |  |  |

## Spreadsheets

**Namespace:** `io.text.*`. Table-shaped readers (`readtable`/`writetable`) → `table.*` (future) — 0 ✅ + 0 ⚠️ / 13 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `importdata` | ❌ |  |  |  |  | auto-detect |
| `importdatatask` | ❌ |  |  |  |  |  |
| `importtool` | ❌ |  |  |  |  |  |
| `readcell` | ❌ |  |  |  |  |  |
| `readmatrix` | ✅ |  |  |  |  | modern CSV |
| `readtable` | ❌ |  |  |  |  | needs table type |
| `readtimetable` | ❌ |  |  |  |  |  |
| `readvars` | ❌ |  |  |  |  |  |
| `sheetnames` | ❌ |  |  |  |  |  |
| `writecell` | ❌ |  |  |  |  |  |
| `writematrix` | ✅ |  |  |  |  | modern CSV |
| `writetable` | ❌ |  |  |  |  | needs table type |
| `writetimetable` | ❌ |  |  |  |  |  |

## Workspace Save / Load

**Namespace:** `io.workspace.*` — 0 ✅ + 0 ⚠️ / 2 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `loadobj` | ❌ |  |  |  |  |  |
| `saveobj` | ❌ |  |  |  |  |  |

## File Name Construction

**Namespace:** `io.paths.*` — 0 ✅ + 0 ⚠️ / 9 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `filemarker` | ❌ |  |  |  |  |  |
| `fileparts` | ✅ |  |  |  |  | split path |
| `filesep` | ✅ |  |  |  |  | path sep |
| `fullfile` | ✅ |  |  |  |  | OS path join |
| `matlabdrive` | ❌ |  |  |  |  |  |
| `matlabroot` | ❌ |  |  |  |  |  |
| `tempdir` | ✅ |  |  |  |  |  |
| `tempname` | ✅ |  |  |  |  |  |
| `toolboxdir` | ❌ |  |  |  |  |  |

## Waveform Generation

**Namespace:** `signal.waveform_generation.*` — 5 ✅ + 0 ⚠️ / 21 = 23%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `buffer` | ❌ |  |  |  |  | reshape with overlap |
| `chirp` | ✅ |  |  |  |  |  |
| `demod` | ❌ |  |  |  |  |  |
| `diric` | ✅ |  |  |  |  | Dirichlet |
| `framelbl` | ❌ |  |  |  |  |  |
| `framesig` | ❌ |  |  |  |  |  |
| `gauspuls` | ✅ |  |  |  |  | Gaussian pulse |
| `gmonopuls` | ✅ |  |  |  |  | Gaussian monopulse |
| `marcumq` | ❌ |  |  |  |  |  |
| `modulate` | ❌ |  |  |  |  |  |
| `pulstran` | ✅ |  |  |  |  | pulse train |
| `rectpuls` | ✅ |  |  |  |  | rectangular pulse |
| `sawtooth` | ✅ |  |  |  |  |  |
| `shiftdata` | ❌ |  |  |  |  |  |
| `sinc` | ✅ |  |  |  |  | sin(πx)/(πx) |
| `square` | ✅ |  |  |  |  |  |
| `tripuls` | ✅ |  |  |  |  | triangular |
| `udecode` | ❌ |  |  |  |  |  |
| `uencode` | ❌ |  |  |  |  |  |
| `unshiftdata` | ❌ |  |  |  |  |  |
| `vco` | ❌ |  |  |  |  | VCO |

## Filter Design (FIR / IIR coefficient generators)

**Namespace:** `signal.filter_design.*` — 6 ✅ + 0 ⚠️ / 37 = 16%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `butter` | ✅ |  |  |  |  | IIR Butterworth |
| `buttord` | ❌ |  |  |  |  | order estimator |
| `cfirpm` | ❌ |  |  |  |  | complex Parks-McClellan |
| `cheb1ord` | ❌ |  |  |  |  | order estimator |
| `cheb2ord` | ❌ |  |  |  |  | order estimator |
| `cheby1` | ❌ |  |  |  |  | IIR Chebyshev I |
| `cheby2` | ❌ |  |  |  |  | IIR Chebyshev II |
| `designfilt` | ❌ |  |  |  |  |  |
| `designfilter` | ❌ |  |  |  |  |  |
| `digitalfilter` | ❌ |  |  |  |  |  |
| `double` | ✅ |  |  |  |  |  |
| `dspfwiz` | ❌ |  |  |  |  |  |
| `ellip` | ❌ |  |  |  |  | IIR elliptic |
| `ellipord` | ❌ |  |  |  |  | order estimator |
| `filt2block` | ❌ |  |  |  |  |  |
| `filteranalyzer` | ❌ |  |  |  |  |  |
| `fir1` | ✅ |  |  |  |  | FIR window-design |
| `fir2` | ❌ |  |  |  |  | arbitrary-response FIR |
| `fircls` | ❌ |  |  |  |  | constrained-LS FIR |
| `fircls1` | ❌ |  |  |  |  |  |
| `firls` | ❌ |  |  |  |  | least-squares FIR |
| `firpm` | ❌ |  |  |  |  | Parks-McClellan FIR |
| `firpmord` | ❌ |  |  |  |  | order estimator |
| `gaussdesign` | ❌ |  |  |  |  |  |
| `info` | ❌ |  |  |  |  |  |
| `intfilt` | ✅ |  |  |  |  | interpolating FIR |
| `isdouble` | ❌ |  |  |  |  |  |
| `issingle` | ✅ |  |  |  |  |  |
| `kaiserord` | ❌ |  |  |  |  | Kaiser window order |
| `maxflat` | ❌ |  |  |  |  |  |
| `polyscale` | ❌ |  |  |  |  |  |
| `polystab` | ❌ |  |  |  |  |  |
| `rcosdesign` | ❌ |  |  |  |  |  |
| `scalefiltersections` | ❌ |  |  |  |  |  |
| `sgolay` | ✅ |  |  |  |  | Savitzky-Golay |
| `single` | ✅ |  |  |  |  |  |
| `yulewalk` | ❌ |  |  |  |  | recursive YW |

## Analog Filters (prototype + analog response)

**Namespace:** `signal.filter_design.*` — 1 ✅ + 0 ⚠️ / 17 = 5%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `besselap` | ❌ |  |  |  |  | analog prototype |
| `besself` | ❌ |  |  |  |  | IIR Bessel |
| `bilinear` | ❌ |  |  |  |  |  |
| `buttap` | ❌ |  |  |  |  | analog prototype |
| `butter` | ✅ |  |  |  |  | IIR Butterworth |
| `cheb1ap` | ❌ |  |  |  |  | analog prototype |
| `cheb2ap` | ❌ |  |  |  |  | analog prototype |
| `cheby1` | ❌ |  |  |  |  | IIR Chebyshev I |
| `cheby2` | ❌ |  |  |  |  | IIR Chebyshev II |
| `ellip` | ❌ |  |  |  |  | IIR elliptic |
| `ellipap` | ❌ |  |  |  |  | analog prototype |
| `freqs` | ❌ |  |  |  |  | analog freq response |
| `impinvar` | ❌ |  |  |  |  |  |
| `lp2bp` | ❌ |  |  |  |  |  |
| `lp2bs` | ❌ |  |  |  |  |  |
| `lp2hp` | ❌ |  |  |  |  |  |
| `lp2lp` | ❌ |  |  |  |  |  |

## Digital Filter Analysis (freqz / phasez / grpdelay / impz / ...)

**Namespace:** `signal.filter_analysis.*` — 3 ✅ + 0 ⚠️ / 19 = 15%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `filteranalyzer` | ❌ |  |  |  |  |  |
| `filternorm` | ❌ |  |  |  |  |  |
| `filtord` | ❌ |  |  |  |  |  |
| `firtype` | ❌ |  |  |  |  |  |
| `freqz` | ✅ |  |  |  |  | discrete freq response |
| `grpdelay` | ✅ |  |  |  |  | group delay |
| `impz` | ✅ |  |  |  |  | impulse response |
| `impzlength` | ✅ |  |  |  |  | impulse length |
| `isallpass` | ✅ |  |  |  |  | predicate |
| `isfir` | ✅ |  |  |  |  | predicate |
| `islinphase` | ✅ |  |  |  |  | predicate |
| `ismaxphase` | ✅ |  |  |  |  | predicate |
| `isminphase` | ✅ |  |  |  |  | predicate |
| `isstable` | ✅ |  |  |  |  | predicate |
| `phasedelay` | ✅ |  |  |  |  | phase delay |
| `phasez` | ✅ |  |  |  |  | phase response |
| `stepz` | ✅ |  |  |  |  | step response |
| `zerophase` | ✅ |  |  |  |  |  |
| `zplane` | ❌ |  |  |  |  |  |

## Digital Filtering (filter / filtfilt / sosfilt / lowpass / ...)

**Namespace:** `signal.digital_filtering.*` + `signal.filter_implementation.*` (TF/SOS/SS/ZP conversions) — 8 ✅ + 0 ⚠️ / 41 = 19%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `bandpass` | ✅ |  |  |  |  | spec-driven BP |
| `bandstop` | ✅ |  |  |  |  | spec-driven BS |
| `cell2sos` | ❌ |  |  |  |  |  |
| `convmtx` | ✅ |  |  |  |  | convolution matrix |
| `ctf2zp` | ❌ |  |  |  |  | control TF → ZPK |
| `ctffilt` | ❌ |  |  |  |  | control TF filter |
| `dspfwiz` | ❌ |  |  |  |  |  |
| `eqtflength` | ❌ |  |  |  |  |  |
| `fftfilt` | ✅ |  |  |  |  | FFT-based overlap-add |
| `filt2block` | ❌ |  |  |  |  |  |
| `filtfilt` | ✅ |  |  |  |  | zero-phase forward+back |
| `filtic` | ❌ |  |  |  |  | init state |
| `hampel` | ✅ |  |  |  |  | outlier-resilient |
| `highpass` | ✅ |  |  |  |  | spec-driven HP |
| `latc2tf` | ❌ |  |  |  |  | inverse |
| `latcfilt` | ❌ |  |  |  |  |  |
| `lowpass` | ✅ |  |  |  |  | spec-driven LP |
| `medfilt1` | ✅ |  |  |  |  | median |
| `residuez` | ❌ |  |  |  |  |  |
| `scalefiltersections` | ❌ |  |  |  |  |  |
| `sgolayfilt` | ✅ |  |  |  |  | Savitzky-Golay |
| `sos2cell` | ❌ |  |  |  |  |  |
| `sos2ctf` | ❌ |  |  |  |  |  |
| `sos2ss` | ✅ |  |  |  |  | SOS → SS |
| `sos2tf` | ✅ |  |  |  |  | inverse |
| `sos2zp` | ✅ |  |  |  |  | SOS → ZPK |
| `sosfilt` | ✅ |  |  |  |  | SOS-cascade filter |
| `ss` | ❌ |  |  |  |  |  |
| `ss2sos` | ✅ |  |  |  |  | inverse |
| `ss2zp` | ✅ |  |  |  |  | SS → ZPK |
| `tf` | ❌ |  |  |  |  |  |
| `tf2latc` | ❌ |  |  |  |  | lattice |
| `tf2sos` | ✅ |  |  |  |  | TF → SOS |
| `tf2ss` | ✅ |  |  |  |  | TF → SS |
| `tf2zp` | ✅ |  |  |  |  | TF → ZPK |
| `tf2zpk` | ✅ |  |  |  |  |  |
| `zp2ctf` | ❌ |  |  |  |  |  |
| `zp2sos` | ✅ |  |  |  |  | ZPK → SOS |
| `zp2ss` | ✅ |  |  |  |  | inverse |
| `zp2tf` | ✅ |  |  |  |  | inverse |
| `zpk` | ❌ |  |  |  |  |  |

## Multirate Signal Processing (decimate / interp / resample / ...)

**Namespace:** `signal.multirate.*` — 4 ✅ + 0 ⚠️ / 8 = 50%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `decimate` | ✅ |  |  |  |  |  |
| `downsample` | ✅ |  |  |  |  |  |
| `fillgaps` | ❌ |  |  |  |  |  |
| `interp` | ✅ |  |  |  |  |  |
| `intfilt` | ✅ |  |  |  |  | interpolating FIR |
| `resample` | ✅ |  |  |  |  |  |
| `upfirdn` | ✅ |  |  |  |  |  |
| `upsample` | ✅ |  |  |  |  |  |

## Signal Modeling (AR / Burg / Yule-Walker / Levinson / Prony)

**Namespace:** `signal.parametric.*` — 0 ✅ + 0 ⚠️ / 25 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `ac2poly` | ❌ |  |  |  |  |  |
| `ac2rc` | ❌ |  |  |  |  |  |
| `arburg` | ❌ |  |  |  |  | Burg AR |
| `arcov` | ❌ |  |  |  |  | covariance AR |
| `armcov` | ❌ |  |  |  |  | modified cov AR |
| `aryule` | ❌ |  |  |  |  | Yule-Walker AR |
| `corrmtx` | ❌ |  |  |  |  | autocorr matrix |
| `invfreqs` | ❌ |  |  |  |  |  |
| `invfreqz` | ❌ |  |  |  |  | IIR sys-id |
| `is2rc` | ❌ |  |  |  |  |  |
| `lar2rc` | ❌ |  |  |  |  |  |
| `levinson` | ❌ |  |  |  |  | Levinson-Durbin |
| `lpc` | ❌ |  |  |  |  | linear prediction |
| `lsf2poly` | ❌ |  |  |  |  |  |
| `poly2ac` | ❌ |  |  |  |  |  |
| `poly2lsf` | ❌ |  |  |  |  |  |
| `poly2rc` | ❌ |  |  |  |  |  |
| `prony` | ❌ |  |  |  |  | Prony method |
| `rc2ac` | ❌ |  |  |  |  |  |
| `rc2is` | ❌ |  |  |  |  |  |
| `rc2lar` | ❌ |  |  |  |  |  |
| `rc2poly` | ❌ |  |  |  |  |  |
| `rlevinson` | ❌ |  |  |  |  | reverse Levinson |
| `schurrc` | ❌ |  |  |  |  | Schur recursion |
| `stmcb` | ❌ |  |  |  |  | Steiglitz-McBride |

## Correlation and Convolution (extras: alignsignals / finddelay / xcorr2 / cconv / convmtx)

**Namespace:** `signal.convolution.*` — 0 ✅ + 0 ⚠️ / 9 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `alignsignals` | ✅ |  |  |  |  | align via xcorr |
| `cconv` | ✅ |  |  |  |  | circular convolution |
| `convmtx` | ✅ |  |  |  |  | convolution matrix |
| `corrmtx` | ❌ |  |  |  |  | autocorr matrix |
| `dtw` | ❌ |  |  |  |  | dynamic time warp |
| `edr` | ❌ |  |  |  |  | edit distance on real |
| `finddelay` | ✅ |  |  |  |  | estimate delay |
| `findsignal` | ❌ |  |  |  |  | pattern search |
| `xcorr2` | ✅ |  |  |  |  | 2-D xcorr |

## Transforms (FFT / DCT / DWT / Hilbert / CZT / Cepstrum)

**Namespace:** `signal.transforms.*`. Promotions in core: `fft, ifft, fftshift, ifftshift`. Future wavelet split: `cwt/dwt/modwt/...` → `wavelet.*` — 6 ✅ + 0 ⚠️ / 32 = 18%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `bitrevorder` | ✅ |  |  |  |  | bit-reverse permutation |
| `cceps` | ✅ |  |  |  |  | complex cepstrum |
| `czt` | ❌ |  |  |  |  | chirp Z-transform |
| `dct` | ✅ |  |  |  |  |  |
| `dftmtx` | ✅ |  |  |  |  | DFT matrix |
| `digitrevorder` | ❌ |  |  |  |  |  |
| `dlistft` | ❌ |  |  |  |  |  |
| `dlstft` | ❌ |  |  |  |  |  |
| `emd` | ❌ |  |  |  |  | empirical mode decomp |
| `envelope` | ✅ |  |  |  |  |  |
| `fsst` | ❌ |  |  |  |  | Fourier synchrosqueezed |
| `fwht` | ❌ |  |  |  |  | fast Walsh-Hadamard |
| `goertzel` | ✅ |  |  |  |  |  |
| `hht` | ❌ |  |  |  |  | Hilbert-Huang |
| `hilbert` | ✅ |  |  |  |  |  |
| `icceps` | ✅ |  |  |  |  | inverse complex cepstrum |
| `idct` | ✅ |  |  |  |  |  |
| `ifsst` | ❌ |  |  |  |  |  |
| `ifwht` | ❌ |  |  |  |  | inverse |
| `instfreq` | ❌ |  |  |  |  | instantaneous frequency |
| `istft` | ❌ |  |  |  |  | inverse |
| `istftlayer` | ❌ |  |  |  |  |  |
| `pspectrum` | ❌ |  |  |  |  | easy spectral analysis |
| `rceps` | ✅ |  |  |  |  | real cepstrum |
| `spectrogram` | ✅ |  |  |  |  |  |
| `stft` | ❌ |  |  |  |  | short-time FFT |
| `stftlayer` | ❌ |  |  |  |  |  |
| `stftmag2sig` | ❌ |  |  |  |  |  |
| `vmd` | ❌ |  |  |  |  | variational MD |
| `wvd` | ❌ |  |  |  |  | Wigner-Ville |
| `xspectrogram` | ❌ |  |  |  |  | cross-spectrogram |
| `xwvd` | ❌ |  |  |  |  | cross WVD |

## Windows (Hamming / Hann / Kaiser / Chebyshev / DPSS / ...)

**Namespace:** `signal.windows.*` — 6 ✅ + 0 ⚠️ / 24 = 25%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `barthannwin` | ✅ |  |  |  |  | Bartlett-Hann |
| `bartlett` | ✅ |  |  |  |  |  |
| `blackman` | ✅ |  |  |  |  |  |
| `blackmanharris` | ✅ |  |  |  |  |  |
| `bohmanwin` | ✅ |  |  |  |  | Bohman |
| `chebwin` | ✅ |  |  |  |  | Dolph-Chebyshev |
| `dpss` | ❌ |  |  |  |  | discrete prolate spheroidal |
| `dpssclear` | ❌ |  |  |  |  | cache |
| `dpssdir` | ❌ |  |  |  |  | cache |
| `dpssload` | ❌ |  |  |  |  | cache |
| `dpsssave` | ❌ |  |  |  |  | cache |
| `enbw` | ❌ |  |  |  |  | equivalent noise BW |
| `flattopwin` | ✅ |  |  |  |  |  |
| `gausswin` | ✅ |  |  |  |  | Gaussian |
| `hamming` | ✅ |  |  |  |  |  |
| `hann` | ✅ |  |  |  |  |  |
| `kaiser` | ✅ |  |  |  |  |  |
| `nuttallwin` | ✅ |  |  |  |  |  |
| `parzenwin` | ✅ |  |  |  |  | Parzen |
| `rectwin` | ✅ |  |  |  |  |  |
| `taylorwin` | ✅ |  |  |  |  | Taylor |
| `triang` | ✅ |  |  |  |  | triangular |
| `tukeywin` | ✅ |  |  |  |  | tapered cosine |
| `wvtool` | ❌ |  |  |  |  | GUI |

## Parametric Spectral Estimation (pburg / pmtm / pmusic / ...)

**Namespace:** `signal.spectral_analysis.*`. Magnitude utils (`db/db2mag/mag2db/pow2db`) → core (cross-cutting math) — 1 ✅ + 0 ⚠️ / 10 = 10%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `db` | ✅ |  |  |  |  | magnitude → dB |
| `db2mag` | ✅ |  |  |  |  |  |
| `db2pow` | ✅ |  |  |  |  |  |
| `findpeaks` | ✅ |  |  |  |  |  |
| `mag2db` | ✅ |  |  |  |  |  |
| `pburg` | ❌ |  |  |  |  | Burg AR |
| `pcov` | ❌ |  |  |  |  |  |
| `pmcov` | ❌ |  |  |  |  |  |
| `pow2db` | ✅ |  |  |  |  |  |
| `pyulear` | ❌ |  |  |  |  | Yule-Walker AR |

## Nonparametric Spectral Estimation (pwelch / periodogram / cpsd / ...)

**Namespace:** `signal.spectral_analysis.*` — 3 ✅ + 0 ⚠️ / 17 = 17%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `cpsd` | ❌ |  |  |  |  | cross-PSD |
| `db` | ✅ |  |  |  |  | magnitude → dB |
| `db2mag` | ✅ |  |  |  |  |  |
| `db2pow` | ✅ |  |  |  |  |  |
| `findpeaks` | ✅ |  |  |  |  |  |
| `mag2db` | ✅ |  |  |  |  |  |
| `mscohere` | ❌ |  |  |  |  | magnitude-squared coherence |
| `periodogram` | ✅ |  |  |  |  |  |
| `plomb` | ❌ |  |  |  |  | Lomb-Scargle |
| `pmtm` | ❌ |  |  |  |  | multi-taper |
| `poctave` | ❌ |  |  |  |  |  |
| `pow2db` | ✅ |  |  |  |  |  |
| `pspectrum` | ❌ |  |  |  |  | easy spectral analysis |
| `pwelch` | ✅ |  |  |  |  | Welch PSD |
| `refinepeaks` | ❌ |  |  |  |  |  |
| `spectralentropy` | ❌ |  |  |  |  |  |
| `tfestimate` | ❌ |  |  |  |  | TF estimate |

## Spectral Measurements (bandpower / snr / sinad / thd / ...)

**Namespace:** `signal.spectral_analysis.*` — 0 ✅ + 0 ⚠️ / 18 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `bandpower` | ❌ |  |  |  |  |  |
| `enbw` | ❌ |  |  |  |  | equivalent noise BW |
| `instbw` | ❌ |  |  |  |  |  |
| `instfreq` | ❌ |  |  |  |  | instantaneous frequency |
| `meanfreq` | ❌ |  |  |  |  | mean frequency |
| `medfreq` | ❌ |  |  |  |  | median frequency |
| `obw` | ❌ |  |  |  |  |  |
| `powerbw` | ❌ |  |  |  |  |  |
| `sfdr` | ❌ |  |  |  |  | spurious-free dynamic range |
| `sinad` | ❌ |  |  |  |  | signal-noise-distortion |
| `snr` | ❌ |  |  |  |  | signal-to-noise |
| `spectralcrest` | ❌ |  |  |  |  |  |
| `spectralentropy` | ❌ |  |  |  |  |  |
| `spectralflatness` | ❌ |  |  |  |  |  |
| `spectralkurtosis` | ❌ |  |  |  |  |  |
| `spectralskewness` | ❌ |  |  |  |  |  |
| `thd` | ❌ |  |  |  |  | total harmonic distortion |
| `toi` | ❌ |  |  |  |  | third-order intercept |

## Time-Frequency Analysis (spectrogram / stft / cwt / wvd / ...)

**Namespace:** `signal.time_frequency.*`. Wavelet/EMD subset (`cwt/wsst/vmd/hht/emd/fsst/ifsst`) → `wavelet.*` (future) — 1 ✅ + 0 ⚠️ / 27 = 3%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `dlistft` | ❌ |  |  |  |  |  |
| `dlstft` | ❌ |  |  |  |  |  |
| `emd` | ❌ |  |  |  |  | empirical mode decomp |
| `fsst` | ❌ |  |  |  |  | Fourier synchrosqueezed |
| `hht` | ❌ |  |  |  |  | Hilbert-Huang |
| `ifsst` | ❌ |  |  |  |  |  |
| `instbw` | ❌ |  |  |  |  |  |
| `instfreq` | ❌ |  |  |  |  | instantaneous frequency |
| `iscola` | ❌ |  |  |  |  |  |
| `istft` | ❌ |  |  |  |  | inverse |
| `istftlayer` | ❌ |  |  |  |  |  |
| `kurtogram` | ❌ |  |  |  |  |  |
| `pspectrum` | ❌ |  |  |  |  | easy spectral analysis |
| `spectralcrest` | ❌ |  |  |  |  |  |
| `spectralentropy` | ❌ |  |  |  |  |  |
| `spectralflatness` | ❌ |  |  |  |  |  |
| `spectralkurtosis` | ❌ |  |  |  |  |  |
| `spectralskewness` | ❌ |  |  |  |  |  |
| `spectrogram` | ✅ |  |  |  |  |  |
| `stft` | ❌ |  |  |  |  | short-time FFT |
| `stftlayer` | ❌ |  |  |  |  |  |
| `stftmag2sig` | ❌ |  |  |  |  |  |
| `tfridge` | ❌ |  |  |  |  |  |
| `vmd` | ❌ |  |  |  |  | variational MD |
| `wvd` | ❌ |  |  |  |  | Wigner-Ville |
| `xspectrogram` | ❌ |  |  |  |  | cross-spectrogram |
| `xwvd` | ❌ |  |  |  |  | cross WVD |

## Pulse and Transition Metrics (risetime / dutycycle / overshoot / ...)

**Namespace:** `signal.measurements.*` — 0 ✅ + 0 ⚠️ / 12 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `dutycycle` | ❌ |  |  |  |  | duty cycle |
| `falltime` | ❌ |  |  |  |  |  |
| `midcross` | ❌ |  |  |  |  | mid-ref crossings |
| `overshoot` | ❌ |  |  |  |  |  |
| `pulseperiod` | ❌ |  |  |  |  |  |
| `pulsesep` | ❌ |  |  |  |  |  |
| `pulsewidth` | ❌ |  |  |  |  |  |
| `risetime` | ❌ |  |  |  |  |  |
| `settlingtime` | ❌ |  |  |  |  |  |
| `slewrate` | ❌ |  |  |  |  |  |
| `statelevels` | ❌ |  |  |  |  |  |
| `undershoot` | ❌ |  |  |  |  |  |

## Signal Descriptive Statistics (rms / peak2peak / envelope / sigROIs / ...)

**Namespace:** `signal.measurements.*` — 2 ✅ + 0 ⚠️ / 30 = 6%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `alignsignals` | ✅ |  |  |  |  | align via xcorr |
| `binmask2sigroi` | ❌ |  |  |  |  |  |
| `countlabels` | ❌ |  |  |  |  |  |
| `cusum` | ❌ |  |  |  |  | CUSUM change detection |
| `dtw` | ❌ |  |  |  |  | dynamic time warp |
| `edr` | ❌ |  |  |  |  | edit distance on real |
| `envelope` | ✅ |  |  |  |  |  |
| `extendsigroi` | ❌ |  |  |  |  |  |
| `extractsigroi` | ❌ |  |  |  |  |  |
| `filenames2labels` | ❌ |  |  |  |  |  |
| `findchangepts` | ❌ |  |  |  |  | change-point detection |
| `finddelay` | ✅ |  |  |  |  | estimate delay |
| `findpeaks` | ✅ |  |  |  |  |  |
| `findsignal` | ❌ |  |  |  |  | pattern search |
| `folders2labels` | ❌ |  |  |  |  |  |
| `framelbl` | ❌ |  |  |  |  |  |
| `framesig` | ❌ |  |  |  |  |  |
| `meanfreq` | ❌ |  |  |  |  | mean frequency |
| `medfreq` | ❌ |  |  |  |  | median frequency |
| `mergesigroi` | ❌ |  |  |  |  |  |
| `peak2peak` | ✅ |  |  |  |  | p-p amplitude |
| `peak2rms` | ✅ |  |  |  |  |  |
| `removesigroi` | ❌ |  |  |  |  |  |
| `rssq` | ✅ |  |  |  |  | root-sum-squared |
| `seqperiod` | ❌ |  |  |  |  |  |
| `shortensigroi` | ❌ |  |  |  |  |  |
| `sigrangebinmask` | ❌ |  |  |  |  |  |
| `sigroi2binmask` | ❌ |  |  |  |  |  |
| `splitlabels` | ❌ |  |  |  |  |  |
| `zerocrossrate` | ❌ |  |  |  |  |  |

## Smoothing and Denoising (smoothdata / hampel / sgolayfilt / ...)

**Namespace:** `signal.smoothing.*` + `signal.digital_filtering.*` (medfilt1, sgolayfilt). `smoothdata` itself → `stats.moving.*` — 3 ✅ + 0 ⚠️ / 4 = 75%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `hampel` | ✅ |  |  |  |  | outlier-resilient |
| `medfilt1` | ✅ |  |  |  |  | median |
| `sgolay` | ✅ |  |  |  |  | Savitzky-Golay |
| `sgolayfilt` | ✅ |  |  |  |  | Savitzky-Golay |

## Vibration Analysis (envspectrum / order tracking / modal)

**Namespace:** `signal.vibration.*` — 0 ✅ + 0 ⚠️ / 13 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `envspectrum` | ❌ |  |  |  |  | envelope spectrum |
| `modalfit` | ❌ |  |  |  |  | modal-fit |
| `modalfrf` | ❌ |  |  |  |  |  |
| `modalsd` | ❌ |  |  |  |  |  |
| `orderspectrum` | ❌ |  |  |  |  |  |
| `ordertrack` | ❌ |  |  |  |  |  |
| `orderwaveform` | ❌ |  |  |  |  |  |
| `rainflow` | ❌ |  |  |  |  |  |
| `rpmfreqmap` | ❌ |  |  |  |  |  |
| `rpmordermap` | ❌ |  |  |  |  |  |
| `rpmtrack` | ❌ |  |  |  |  | order tracking |
| `tachorpm` | ❌ |  |  |  |  | tachometer→RPM |
| `tsa` | ❌ |  |  |  |  |  |
