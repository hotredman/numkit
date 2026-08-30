# numkit Namespace Design

Дизайн-документ для namespace-механизма в numkit движке. Цель —
управляемое расширение API: по мере добавления тулбоксов (signal,
stats, graphics, linalg, sparse, ode, table, …) и numkit-специфичных
расширений (gpu, experimental, …) плоский глобальный scope перестаёт
работать. Вводим namespace-ы заранее.

**Приоритеты дизайна:**
1. Чистое решение (без policy-флагов и hidden state)
2. Совместимость с MATLAB (через `import compat.*`)
3. Возможность сделать лучше MATLAB (иерархия, `as`, разделение
   numkit-only от MATLAB-mirror)

Связанные документы:
- [PROGRESS.md](../../tools/parity/PROGRESS.md) — живая карта парити со
  всеми функциями MATLAB-doc, разбитая по секциям/namespace. Колонки:
  status (✅/❌/⚠️), numkit_ms, vs_MATLAB, vs_Octave, correctness.

---

## 1. Принцип одной строки

> **Функция, физически живущая в `toolboxes/<name>/`, регистрируется как
> `<name>.<funcname>` (или с поднамespace-ом: `<name>.<sub>.<funcname>`,
> отражая каталог-иерархию). Функция в `toolboxes/builtin/` — в core
> (плоское, без namespace).**

Из этого правила одно (закрытое) исключение — **9 промоций** (Section
7): `fft`, `ifft`, `fftshift`, `ifftshift`, `conv`, `xcorr` (signal —
general-purpose инструменты), плюс `close`, `figure`, `hold` (graphics
— session/workspace команды наравне с `clear`). Эти дополнительно
регистрируются в core.

## 2. Что никогда не в namespace

Эти всегда глобальны:

- **Keywords:** `if`, `else`, `elseif`, `for`, `while`, `do`, `switch`,
  `case`, `otherwise`, `break`, `continue`, `return`, `function`, `end`,
  `try`, `catch`, `global`, `persistent`
  (`import` — НЕ keyword: обычный command-style builtin)
- **Operators:** `+`, `-`, `*`, `/`, `\`, `^`, `.+ .- .* ./ .\ .^`,
  `:` (range), `'`, `.'`, `<`, `>`, `<=`, `>=`, `==`, `~=`,
  `&&`, `||`, `&`, `|`, `~`
- **Constants:** `pi`, `eps`, `inf`, `Inf`, `nan`, `NaN`, `i`, `j`,
  `true`, `false` (живут в `constantsEnv_`, parent для всех scope-ов)

## 3. Lookup-алгоритм

Когда резолвер встречает имя в выражении — короткое (`fft`) или dotted
(`signal.transforms.fft`) — он идёт по шагам:

```
input: name = либо "fft" (short), либо "a.b.c" (dotted path)

1. Local scope (function/block locals, parameters) — проверяем по
   первому segment-у name.
2. Workspace / parent scopes — то же.
3. User functions in current script (scriptLocalUserFuncs_,
   workspaceUserFuncs_).
4. Builtin registry:
   4a. Если name содержит '.':
       lookup registry_[full_name]; если найдено — return.
   4b. Если name короткое:
       lookup registry_["", name] (core); если найдено — return.
   4c. Если name короткое: проверяем активные импорты текущего scope
       (в LIFO-порядке):
       - import x.y.z         → match если name == "z" → use "x.y.z"
       - import x.y.*         → match если registry_[x.y, name] есть
                                ИЛИ shortNameIndex_ под prefix "x.y."
                                содержит name (рекурсивно)
       - import x.y as alias  → match если name начинается с alias,
                                substitute alias → "x.y" и rerun
5. User m-files via VFS (Section 12):
   5a. Полный путь "a.b.c": искать `+a/+b/c.m` в addpath
   5b. Короткое имя: искать `name.m` в script-origin dir, addpath,
                      activeImports prefixes (+pkg/ packages)
6. Не найдено → error
```

Шаг 5 (m-file resolver) применяет тот же стек активных импортов —
`import myutil.*` распространяется и на user-пакеты `+myutil/`.

## 4. Import грамматика

`import` — обычный builtin, не специальный синтаксис. Парсер видит
любую из форм ниже как command-style либо function-style вызов
`import(...)` со строковыми аргументами:

```
import a.b.c             ≡ import('a.b.c')
import a.b.*             ≡ import('a.b.*')
import a.b as alias      ≡ import('a.b', 'as', 'alias')
import a.* b.*           ≡ import('a.*', 'b.*')
```

Builtin сам парсит каждый строковый arg (path-сегменты, wildcard,
3-arg alias-форма). Реализация: [`toolboxes/builtin/src/library.cpp`](toolboxes/builtin/src/library.cpp).
Command-style glue склеивает `.*` как терминатор-суффикс
([`core/src/parser.cpp`](core/src/parser.cpp) parseCommandStyleCall).

Четыре практических формы:

```matlab
import signal.transforms.fft     % один символ → fft в текущем scope
import signal.transforms.*       % все из transforms
import signal.*                  % все из signal (рекурсивно)
import signal.transforms as tr   % alias: tr.fft, tr.dct, ...
import compat.*                  % MATLAB-режим
```

`import` действует от точки декларации до конца текущего scope
(function body / script). Локально к функции — как в MATLAB. В VM
функ-локальные импорты живут в `CallFrame::env` (lazy) и уничтожаются
при возврате из функции.

## 5. `compat` namespace

`compat` — обычный namespace, **никакого special-case в Engine**. Он
наполняется явными вызовами `registerFunction("compat", name, func)`
каждой MATLAB-mirror либы — параллельно с регистрацией функции в её
собственном namespace.

**Convention:** функция в MATLAB-mirror либе регистрируется **дважды**:
в своём namespace (по иерархии каталогов) и в `compat` (под коротким
именем). Функция в numkit-only либе — **один раз** в своём namespace.

**Эргономика:** каждая `library.cpp` имеет **локальную lambda**, которая
скрывает повторную регистрацию.

**Регистрация в MATLAB-mirror либе:**

```cpp
// toolboxes/signal/src/library.cpp
void SignalLibrary::install(Engine &engine) {
    // Local helper — signal является MATLAB-mirror, поэтому каждая
    // функция регистрируется И в своём sub-namespace, И в compat.
    auto reg = [&](const std::string &sub,
                   const std::string &name,
                   ExternalFunc f) {
        engine.registerFunction("signal." + sub, name, f);
        engine.registerFunction("compat", name, f);
    };

    reg("transforms", "fft", &fft_reg);
    reg("transforms", "ifft", &ifft_reg);
    reg("windows", "hamming", &hamming_reg);
    reg("digital_filtering", "filter", &filter_reg);
    // ... ~150 функций

    // 6 промоций в core — третий путь к тем же функциям
    // (рядом с signal.transforms.fft и compat.fft).
    engine.registerFunction("", "fft", &fft_reg);
    engine.registerFunction("", "ifft", &ifft_reg);
    engine.registerFunction("", "fftshift", &fftshift_reg);
    engine.registerFunction("", "ifftshift", &ifftshift_reg);
    engine.registerFunction("", "conv", &conv_reg);
    engine.registerFunction("", "xcorr", &xcorr_reg);
}
```

**Регистрация в numkit-only либе:**

```cpp
// Будущая toolboxes/numkit_gpu/src/library.cpp:
void GPULibrary::install(Engine &engine) {
    // numkit_gpu НЕ MATLAB-mirror — регистрируется только в собственном namespace.
    auto reg = [&](const std::string &name, ExternalFunc f) {
        engine.registerFunction("numkit_gpu", name, f);
    };

    reg("matmul_gpu", &matmul_gpu_reg);
    reg("fft_gpu", &fft_gpu_reg);
}
```

**Использование пользователем:**

```matlab
import compat.*               % MATLAB-режим: все mirror-функции flat
fft(x)                        % ✅
hamming(N)                    % ✅
std(x)                        % ✅ (после переезда в stats.*)
plot(x, y)                    % ✅

import numkit_gpu.*           % явный отдельный импорт
matmul_gpu(A, B)              % ✅
```

**Конфликты в compat** (`signal.foo` и `stats.foo` обе пытаются alias):
ловятся generic-механизмом duplicate-full-name detection. Когда `stats`
вызывает `registerFunction("compat", "foo", ...)`, а `compat.foo` уже
зарегистрирован из `signal` → Engine throws:

```
"duplicate registration: compat.foo (already registered)"
```

Это работает для **любого** namespace одинаково — нет особой логики для
compat. Программист либо разрешает коллизию (исключает одну из либ из
compat — не вызывает second registerFunction), либо переименовывает
функцию.

## 6. Test fixture / REPL setup

Engine не имеет policy-флагов. Совместимость с MATLAB достигается одной
строкой `import compat.*` в начале скрипта.

**Тестовый раннер:**
```cpp
// тестовый wrapper:
auto fullCode = "import compat.*;\n" + testCode;
engine.eval(fullCode);
```

**REPL:**
```cpp
// инициализация REPL session:
replEngine.eval("import compat.*;");
// затем persistent imports — флаг что imports не сбрасываются между evals
replEngine.setPersistentImports(true);
```

**End-user MATLAB script:**
```matlab
import compat.*
y = filter(b, a, x);
Y = fft(y);
plot(Y);
```

Одна строка в шапке. Концептуально match-ит MATLAB-стиль (там тоже
есть `import`, у нас — обычный command-style builtin).

## 7. Promotions — закрытый whitelist

Каждая promotion — функция зарегистрированная **трижды**: под своим
toolbox-namespace, в `compat`, и в core (namespace `""`). Доступна по
короткому имени без `import`.

### Signal (6 функций) — DSP general-purpose

```
fft, ifft, fftshift, ifftshift, conv, xcorr
```

**Почему:** general-purpose инструменты, используются за пределами DSP
(анализ, корреляции, полиномиальное умножение, image, ML, time-series).
**Dual-citizenship** (signal toolbox + core math) отражает их реальную
природу.

**Регистрация:** в `toolboxes/signal/src/library.cpp`:
```cpp
engine.registerFunction("", "fft", &fft_reg);
// ... 5 других
```

### Graphics (3 функции) — workspace/session commands

```
close, figure, hold
```

**Почему:** session-style команды на одном уровне с `clear` / `who` —
не data-plotting (как `plot`/`bar`/`imagesc`), а workspace state
manipulation. Пользователь ожидает их доступными без явного импорта,
аналогично `clear`. Только эти три из toolboxes/graphics удовлетворяют
критерию "session command, not plotting".

**Регистрация:** в `toolboxes/graphics/src/library.cpp` через `regCore`
helper (triple-register):
```cpp
auto regCore = [&](sub, name, fn) {
    engine.registerFunction(string("graphics.") + sub, name, fn);
    engine.registerFunction("compat", name, fn);
    engine.registerFunction("", name, fn);  // core promotion
};
regCore("layout", "close", ...);
regCore("layout", "figure", ...);
regCore("layout", "hold", ...);
```

**Не promoted (требуют import):** `plot`, `bar`, `scatter`, `subplot`,
`stem`, `xline`, `yline`, `polarplot`, `surf`, `mesh`, `contour`,
`imagesc` — все data-plotting функции остаются в `graphics.<sub>.*` +
`compat.*`.

### Whitelist closed

Все 9 promotions перечислены выше (6 signal + 3 graphics).
Расширение требует обновления данного документа с обоснованием
(критерий: general-purpose / session-command, не toolbox-specific
data operation).

## 8. Что в core (toolboxes/builtin)

Core — плоский namespace (`""`). Сюда попадает всё что **физически
живёт** в `toolboxes/builtin/`. По принципу одной строки (Section 1).

**Важные перемещения относительно текущего состояния:**

| Сейчас в `toolboxes/builtin/` | Переезжает в | Почему |
|---|---|---|
| `var, std, median, mode, quantile, prctile, cov, corrcoef, iqr, maxk, mink, bounds, movmean, movstd, ...` (~30 функций) | `toolboxes/stats/` → `stats.*` | Декомпозиция: skewness/kurtosis/nan* уже в stats |
| `fopen, fclose, fread, fwrite, fprintf, fscanf, sscanf, textscan, csvread, csvwrite, dlmread, save, load, setenv, getenv` (~25 функций) | `toolboxes/io/` → `io.*` | Чистый MATLAB-doc раздел "Data Import and Export" |

После переезда **все** эти функции доступны через `import compat.*` или
явно по namespace-у (`stats.std(x)`, `io.fopen(...)`).

## 9. Полная карта namespace-ов

### 9.1 core (toolboxes/builtin) — плоский

Содержит:

- **Массивы и формы**: `zeros`, `ones`, `eye`, `size`, `length`, `numel`,
  `ndims`, `reshape`, `transpose`, `permute`, `ipermute`, `squeeze`,
  `cat`, `blkdiag`, `horzcat`, `vertcat`, `repmat`, `repelem`, `flip`,
  `fliplr`, `flipud`, `rot90`, `circshift`, `tril`, `triu`, `diag`,
  `kron`, `linspace`, `logspace`, `meshgrid`, `ndgrid`, `sort`,
  `sortrows`, `find`, `nnz`, `nonzeros`, `pagemtimes`
- **Index/shape утилиты**: `sub2ind`, `ind2sub`, `shiftdim`, `head`,
  `tail`, `isvector`, `isrow`, `iscolumn`, `ismatrix`, `issorted`,
  `issortedrows`, `isuniform`, `paddata`, `resize`, `trimdata`
- **Скалярная математика**: `abs`, `sign`, `sqrt`, `nthroot`, `hypot`,
  `exp`, `log`, `log2`, `log10`, `expm1`, `log1p`, `pow2`, `reallog`,
  `realpow`, `realsqrt`, `floor`, `ceil`, `round`, `fix`, `mod`, `rem`,
  `idivide`, `max`, `min`, `maxk`, `mink`, `bounds`, `sum`, `prod`,
  `mean`, `cumsum`, `cumprod`, `cummax`, `cummin`, `diff`, `gradient`,
  `trapz`, `cumtrapz`, `gcd`, `lcm`, `deg2rad`, `rad2deg`,
  `sin/cos/tan/asin/acos/atan/atan2`,
  `sinh/cosh/tanh/asinh/acosh/atanh`,
  `sind/cosd/tand/asind/acosd/atand/atan2d`,
  `sinpi/cospi`, `sec/csc/cot` + `*h/*d/a*` варианты,
  `cart2pol`, `pol2cart`, `cart2sph`, `sph2cart`
- **Set/discrete/bit**: `unique`, `ismember`, `ismembertol`, `uniquetol`,
  `union`, `intersect`, `setdiff`, `setxor`, `allunique`, `numunique`,
  `histcounts`, `discretize`, `accumarray`, `primes`, `isprime`,
  `factor`, `factorial`, `nchoosek`, `perms`, `bitand`, `bitor`,
  `bitxor`, `bitshift`, `bitcmp`, `bitget`, `bitset`, `swapbytes`
- **Численные методы**: `interp1`, `interp2`, `interp3`, `interpn`,
  `spline`, `pchip`, `makima`, `griddata`, `griddedinterpolant`, `mkpp`,
  `ppval`, `unmkpp`, `polyfit`, `polyval`, `polyder`, `polyint`,
  `polyvalm`, `roots`, `poly`, `residue`, `tf2zp`, `zp2tf`, `fzero`,
  `fminbnd`, `fminsearch`, `lsqnonneg`, `optimset`, `optimget`,
  `integral`
- **Случайные**: `rand`, `randn`, `randi`, `randperm`, `rng`
- **Комплексные**: `real`, `imag`, `conj`, `complex`, `angle`
- **Спец. функции**: `gamma`, `gammaln`, `gammainc`, `gammaincinv`,
  `erf`, `erfc`, `erfinv`, `erfcinv`, `erfcx`, `beta`, `betainc`,
  `betaincinv`, `betaln`, `expint`, `psi`, `legendre`, `airy`,
  `besselj`, `bessely`, `besseli`, `besselk`, `besselh`, `ellipj`,
  `ellipke`
- **Численные предикаты и константы**: `intmax`, `intmin`, `realmax`,
  `realmin`, `flintmax`, `eps`, `pi`, `inf`, `nan`, `true`, `false`,
  `isnan`, `isinf`, `isfinite`, `allfinite`, `anynan`, `isnumeric`,
  `islogical`, `ischar`, `isstring`, `iscell`, `isstruct`, `isempty`,
  `isscalar`, `isreal`, `isinteger`, `isfloat`, `issingle`, `isequal`,
  `isequaln`, `class`, `isa`
- **Преобразование типов**: `double`, `single`, `int8/16/32/64`,
  `uint8/16/32/64`, `logical`, `char`, `string`, `cast`, `typecast`
- **Cell/struct**: `cell`, `struct`, `fieldnames`, `isfield`, `rmfield`,
  `getfield`, `setfield`, `orderfields`, `cellfun`, `arrayfun`,
  `structfun`, `cell2mat`, `mat2cell`, `num2cell`, `cell2struct`,
  `struct2cell`, `iscellstr`, `cellstr`, `celldisp`, `cellplot`, `deal`
- **Строки**: `num2str`, `str2num`, `str2double`, `mat2str`, `string`,
  `char`, `strcmp`, `strcmpi`, `strncmp`, `strncmpi`, `strfind`,
  `strsplit`, `strjoin`, `strjust`, `strtok`, `strtrim`, `strip`, `pad`,
  `upper`, `lower`, `strcat`, `strlength`, `strrep`, `replace`,
  `replacebetween`, `contains`, `startsWith`, `endsWith`, `count`,
  `erase`, `erasebetween`, `extract`, `extractafter`, `extractbefore`,
  `extractbetween`, `insertafter`, `insertbefore`, `split`,
  `splitlines`, `join`, `compose`, `append`, `reverse`, `blanks`,
  `deblank`, `newline`, `isspace`, `isletter`, `isstrprop`, `matches`,
  `sprintf`, `sscanf`, `regexp`, `regexpi`, `regexprep`,
  `regexptranslate`, `convertstringstochars`,
  `convertcharstostrings`, `convertcontainedstringstochars`
- **Function handles**: `feval`, `func2str`, `str2func`, `functions`,
  `localfunctions`
- **Workspace**: `clear`, `clearvars`, `clc`, `who`, `whos`, `which`,
  `exist`, `tic`, `toc`, `pause`, `format`, `diary`, `home`, `iskeyword`
- **Errors**: `error`, `warning`, `assert`, `MException`, `rethrow`,
  `throw`, `lastwarn`, `oncleanup`
- **Magnitude/phase утилиты**: `db`, `db2mag`, `db2pow`, `mag2db`,
  `pow2db`, `wrapToPi`, `wrap2Pi`, `wrapTo180`, `wrapTo360`
- **6 промоций**: `fft`, `ifft`, `fftshift`, `ifftshift`, `conv`,
  `xcorr` (физически в `toolboxes/signal/`, но также прописаны в core)

Итого **~310 имён** в core.

### 9.2 `signal.*` (toolboxes/signal) — sub-namespace по каталогам

Иерархия = sub-каталоги внутри `toolboxes/signal/src/`:

| Sub-namespace | Каталог | Примеры |
|---|---|---|
| `signal.transforms` | `transforms/` | `fft`, `ifft`, `dct`, `idct`, `hilbert`, `stft`, `cceps`, `dftmtx`, `czt`, `bitrevorder`, `goertzel`, `envelope`, ... |
| `signal.windows` | `windows/` | `hamming`, `hann`, `blackman`, `kaiser`, `bartlett`, `chebwin`, `taylorwin`, `dpss`, `gausswin`, `rectwin`, `tukeywin`, `flattopwin`, `parzenwin`, `nuttallwin`, `triang`, `barthannwin`, `bohmanwin`, `blackmanharris` |
| `signal.filter_design` | `filter_design/` | `butter`, `cheby1`, `cheby2`, `ellip`, `fir1`, `fir2`, `firpm`, `firls`, `besself`, `kaiserord`, `buttord`, `cheb1ord`, `cheb2ord`, `ellipord`, `firpmord`, `sgolay`, `bilinear`, `lp2lp`, `lp2bp`, `lp2bs`, `lp2hp`, `besselap`, `buttap`, `cheb1ap`, `cheb2ap`, `ellipap`, `freqs`, `impinvar`, ... |
| `signal.filter_analysis` | `filter_analysis/` | `freqz`, `phasez`, `grpdelay`, `impz`, `impzlength`, `stepz`, `phasedelay`, `zerophase`, `zplane`, `isfir`, `islinphase`, `isstable`, `isminphase`, `ismaxphase`, `isallpass`, `filtord`, `firtype`, `filternorm` |
| `signal.filter_implementation` | `filter_implementation/` | `tf2zp`, `zp2tf`, `tf2sos`, `sos2tf`, `tf2ss`, `ss2tf`, `tf2zpk`, ... (преобразования форм) |
| `signal.digital_filtering` | `digital_filtering/` | `filter`, `filtfilt`, `sosfilt`, `medfilt1`, `sgolayfilt`, `lowpass`, `highpass`, `bandpass`, `bandstop`, `fftfilt`, `convmtx`, `hampel`, `latcfilt`, ... |
| `signal.multirate` | `multirate/` | `decimate`, `downsample`, `upsample`, `resample`, `interp`, `upfirdn`, `intfilt`, `fillgaps` |
| `signal.convolution` | `convolution/` | `conv2`, `convn`, `xcorr2`, `xcov`, `cconv`, `dtw`, `edr`, `alignsignals`, `finddelay`, `findsignal`, `corrmtx` |
| `signal.smoothing` | `smoothing/` | `smoothdata` (если выделяется) |
| `signal.measurements` | `measurements/` | `findpeaks`, `peak2peak`, `peak2rms`, `rms`, `rssq`, `risetime`, `falltime`, `slewrate`, `overshoot`, `undershoot`, `dutycycle`, `pulseperiod`, `pulsesep`, `pulsewidth`, `settlingtime`, `statelevels`, `midcross`, `cusum`, `findchangepts`, `zerocrossrate`, ... |
| `signal.spectral_analysis` | `spectral_analysis/` | `pwelch`, `periodogram`, `cpsd`, `mscohere`, `tfestimate`, `pmtm`, `pburg`, `pyulear`, `pcov`, `pmcov`, `pmusic`, `peig`, `pspectrum`, `bandpower`, `meanfreq`, `medfreq`, `obw`, `powerbw`, `snr`, `sinad`, `thd`, `sfdr`, `toi`, `enbw`, `plomb`, `poctave`, `spectralentropy`, `refinepeaks` |
| `signal.time_frequency` | `time_frequency/` | `spectrogram`, `stft`, `istft`, `cwt`, `wsst`, `vmd`, `hht`, `emd`, `fsst`, `ifsst`, `wvd`, `xwvd`, `kurtogram`, `tfridge`, `instfreq`, `instbw`, `iscola`, `xspectrogram`, `spectralcrest`, `spectralflatness`, `spectralkurtosis`, `spectralskewness`, `dlistft`, `dlstft`, `stftlayer`, `istftlayer`, `stftmag2sig` |
| `signal.waveform_generation` | `waveform_generation/` | `chirp`, `gauspuls`, `pulstran`, `rectpuls`, `tripuls`, `square`, `sawtooth`, `sinc`, `gmonopuls`, `diric`, `vco`, `modulate`, `demod`, `marcumq`, `udecode`, `uencode`, `buffer`, `framelbl`, `framesig`, `shiftdata`, `unshiftdata` |
| `signal.parametric` | `parametric/` (новый) | `lpc`, `levinson`, `arburg`, `arcov`, `armcov`, `aryule`, `prony`, `stmcb`, `invfreqz`, `invfreqs`, `rlevinson`, `ac2poly`, `ac2rc`, `is2rc`, `lar2rc`, `lsf2poly`, `poly2ac`, `poly2lsf`, `poly2rc`, `rc2ac`, `rc2is`, `rc2lar`, `rc2poly`, `schurrc` |
| `signal.vibration` | `vibration/` (новый) | `envspectrum`, `modalfit`, `modalfrf`, `modalsd`, `orderspectrum`, `ordertrack`, `orderwaveform`, `rainflow`, `rpmfreqmap`, `rpmordermap`, `rpmtrack`, `tachorpm`, `tsa` |

Все эти функции дополнительно регистрируются в `compat.*` (двойная
регистрация через locally-определённый helper в `library.cpp`,
Section 5). 6 промоций (`fft, ifft, fftshift, ifftshift, conv, xcorr`)
**тройная регистрация** — в `signal.transforms.*`, в `compat.*`, и в
core.

### 9.3 `stats.*` (toolboxes/stats)

Содержит то что переезжает из toolboxes/builtin + текущее содержимое:

| Sub-namespace | Каталог | Примеры |
|---|---|---|
| `stats.descriptive` | `descriptive/` | `var`, `std`, `median`, `mode`, `quantile`, `prctile`, `cov`, `corrcoef`, `iqr`, `maxk`, `mink`, `bounds`, `mape`, `rmse`, `skewness`, `kurtosis`, `moment`, `summary`, `kde` |
| `stats.moving` | `moving/` | `movmean`, `movmedian`, `movmin`, `movmax`, `movsum`, `movstd`, `movvar`, `movmad`, `movprod`, `smoothdata` |
| `stats.nan` | `nan/` | `nansum`, `nanmean`, `nanmin`, `nanmax`, `nanvar`, `nanstd`, `nanmedian` |

Все функции также регистрируются в `compat.*` (Section 5).

### 9.4 `io.*` (toolboxes/io — новый)

Содержит то что переезжает из toolboxes/builtin:

| Sub-namespace | Каталог | Примеры |
|---|---|---|
| `io.file_io` | `file_io/` | `fopen`, `fclose`, `fread`, `fwrite`, `fprintf`, `fscanf`, `fgetl`, `fgets`, `feof`, `ferror`, `ftell`, `fseek`, `frewind`, `sscanf`, `textscan`, `fileread` |
| `io.text` | `text/` | `csvread`, `csvwrite`, `dlmread`, `dlmwrite`, `readmatrix`, `writematrix`, `readlines`, `writelines`, `importdata`, `type` |
| `io.folders` | `folders/` | `dir`, `ls`, `cd`, `pwd`, `mkdir`, `rmdir`, `delete`, `copyfile`, `movefile`, `recycle`, `what` |
| `io.paths` | `paths/` | `fullfile`, `fileparts`, `filesep`, `pathsep`, `tempdir`, `tempname`, `matlabroot`, `toolboxdir`, `matlabdrive`, `filemarker` |
| `io.workspace` | `workspace/` | `save`, `load`, `setenv`, `getenv` |

Все функции также регистрируются в `compat.*` (Section 5).

### 9.5 `graphics.*` (toolboxes/graphics)

| Sub-namespace | Каталог | Примеры |
|---|---|---|
| `graphics.line` | `line/` | `plot`, `plot3`, `semilogx`, `semilogy`, `loglog`, `errorbar`, `area`, `stairs`, `fplot`, `fplot3`, `fimplicit`, `stackedplot` |
| `graphics.polar` | `polar/` | `polarplot`, `polarscatter`, `polarhistogram`, `polaraxes`, ... |
| `graphics.contour` | `contour/` | `contour`, `contourf`, `contour3`, `contourc`, `contourslice`, `clabel`, `fcontour` |
| `graphics.surface` | `surface/` | `surf`, `mesh`, `surfc`, `surfl`, `meshc`, `meshz`, `surface`, `pcolor`, `peaks`, `sphere`, `cylinder`, `ellipsoid`, `ribbon`, ... |
| `graphics.volume` | `volume/` | `slice`, `isosurface`, `isocaps`, `isonormals`, `stream2`, `stream3`, `streamline`, `streamslice`, ... |
| `graphics.geographic` | `geographic/` | `geoplot`, `geoaxes`, `geobasemap`, ... |
| `graphics.layout` | `layout/` | `figure`, `subplot`, `tiledlayout`, `nexttile`, `axes`, `hold`, `axis`, `title`, `xlabel`, `ylabel`, `zlabel`, `legend`, `grid`, `colorbar` |
| `graphics.bar` | `bar/` | `bar`, `bar3`, `histogram`, `histogram2`, `scatter`, `scatter3`, `pie`, `pie3`, ... |

Все функции также регистрируются в `compat.*` (Section 5).

### 9.6 Будущие MATLAB-mirror либы

| Library | Когда | Namespace |
|---|---|---|
| `toolboxes/linalg/` | LAPACK-class работы (det, inv, eig, svd, qr, chol, lu, norm) | `linalg.*` (с sub-разделами) |
| `toolboxes/sparse/` | Sparse matrix type + iterative solvers | `sparse.*` |
| `toolboxes/ode/` | ODE integrators (ode23, ode45, ode15s, …) | `ode.*` |
| `toolboxes/table/` | Table/timetable type + операции | `table.*` |
| `toolboxes/categorical/` | Categorical type | `categorical.*` |
| `toolboxes/datetime/` | datetime/duration types | `datetime.*` |
| `toolboxes/wavelet/` | cwt, dwt, modwt, vmd, hht, emd, fsst | `wavelet.*` |

Каждая регистрирует свои функции и в собственном namespace, и в
`compat.*` (через локальный helper в своей `library.cpp`).

### 9.7 Будущие numkit-only либы

| Library | Namespace | Регистрируется в compat? |
|---|---|---|
| `toolboxes/numkit_gpu/` | `numkit_gpu.*` | **НЕТ** (нет MATLAB-аналога) |
| `toolboxes/experimental/` | `experimental.*` | **НЕТ** (beta features) |
| `toolboxes/numkit_extras/` | `numkit_extras.*` | **НЕТ** |

## 10. Directory layout — Variant B (сокращённые MATLAB-mirror имена)

### 10.1 toolboxes/builtin/src/

Полный refactor:

| Сейчас | Становится | MATLAB-doc раздел |
|---|---|---|
| `lang/arrays/` | `language/arrays/` | Matrices and Arrays |
| `lang/commands/` | `language/commands/` | Entering Commands |
| `lang/operators/` (часть с +,-,*,/) | `math/arithmetic/` | Arithmetic |
| `lang/operators/` (часть с ==,<,>) | `language/relational/` | Relational Operators |
| `math/elementary/` (sum/prod/diff/...) | `math/arithmetic/` | Arithmetic |
| `math/elementary/` (sin/cos/...) | `math/trig/` | Trigonometry |
| `math/elementary/` (exp/log/sqrt/...) | `math/exp_log/` | Exponents and Logarithms |
| `math/elementary/` (gamma/erf/...) | `math/special/` | Special Functions |
| `math/elementary/discrete.cpp` | `math/discrete/` | Discrete Math |
| `math/elementary/complex.cpp` | `math/complex/` | Complex Numbers |
| `math/elementary/polynomials.cpp` | `math/poly/` | Polynomials |
| `math/elementary/int_math.cpp` | `language/bitwise/` | Bit-wise Operations |
| `math/elementary/discrete.cpp` (set ops) | `language/sets/` | Set Operations |
| `math/integration/` | `math/integration/` | Numerical Integration |
| `math/interpolation/` | `math/interp/` | Interpolation |
| `math/optim/` | `math/optim/` | Optimization |
| `math/random/` | `math/random/` | Random Number Generation |
| `data_io/csv.cpp` | (переезжает в toolboxes/io/) | — |
| `data_io/fileio.cpp` | (переезжает в toolboxes/io/) | — |
| `data_io/saveload.cpp` | (переезжает в toolboxes/io/) | — |
| `datatypes/numeric/` | `language/types/` | Numeric Types |
| `datatypes/strings/` | `language/strings/` | Characters and Strings |
| `datatypes/strings/regex.cpp` | `language/regex/` | (sub of strings) |
| `datatypes/struct/` | `language/structures/` | Structures |
| `datatypes/cell/` | `language/cells/` | Cell Arrays |
| `datatypes/_handlefn_helpers.hpp` | `language/handles/` | Function Handles |
| `data_analysis/descriptive_statistics/` | (переезжает в toolboxes/stats/) | — |
| `programming/errors/` | `programming/errors/` | Error Handling |
| `helpers.hpp` | `helpers.hpp` | (cross-cutting) |
| `library.cpp` | `library.cpp` | (registration) |

Финальная структура `toolboxes/builtin/src/`:

```
toolboxes/builtin/src/
  language/                # MATLAB: Language Fundamentals
    commands/              # Entering Commands
    arrays/                # Matrices and Arrays
    types/                 # Numeric Types
    strings/               # Characters and Strings
    regex/                 # (sub of strings)
    structures/            # Structures
    cells/                 # Cell Arrays
    handles/               # Function Handles
    bitwise/               # Bit-wise Operations
    sets/                  # Set Operations
    relational/            # Relational Operators
  math/                    # MATLAB: Mathematics
    arithmetic/
    trig/                  # Trigonometry
    exp_log/               # Exponents and Logarithms
    special/               # Special Functions
    discrete/              # Discrete Math
    complex/               # Complex Numbers
    poly/                  # Polynomials
    interp/                # Interpolation
    integration/           # Numerical Integration
    optim/                 # Optimization
    random/                # Random Number Generation
  programming/             # MATLAB: Programming and Scripts
    workspace/             # Workspace (clear/who/whos/...)
    errors/                # Error Handling
  helpers.hpp              # cross-cutting helpers
  library.cpp              # registration entry point
```

**Заметки:**
- Workspace builtins (`clear`, `who`, `whos`, ...) переезжают в
  `programming/workspace/`. Раньше они в `lang/commands/` — но
  семантически workspace ближе к programming model.
- `programming/errors/` остаётся на месте (имя совпадает с MATLAB-doc
  "Error Handling" сокращённо).
- `language/regex/` — выделен в собственный sub-каталог (был `regex.cpp`
  в `strings/`), потому что regex это значительный сабсет.
- `language/relational/` — для `eq, ne, lt, le, gt, ge, and, or, not`
  если они wrap-аются как функции (Tier 1.1 в parity plan).

### 10.2 toolboxes/io/src/ (новый)

```
toolboxes/io/src/
  file_io/                 # Low-Level File I/O
  text/                    # Text Files
  folders/                 # File Operations
  paths/                   # File Name Construction
  workspace/               # save/load/setenv/getenv
  library.cpp
```

### 10.3 toolboxes/stats/src/ (расширяется)

```
toolboxes/stats/src/
  descriptive/             # var/std/median/cov/corrcoef/skewness/kurtosis/...
  moving/                  # movmean/movstd/...
  nan/                     # nansum/nanmean/...
  library.cpp
```

### 10.4 toolboxes/signal/src/ (минорные правки)

Уже в основном MATLAB-mirror. Возможны локальные переименования:
- `parametric/` — новый sub-каталог (lpc/levinson/arburg/...)
- `vibration/` — новый (envspectrum/modalfit/...)

### 10.5 toolboxes/graphics/src/

Структура зависит от текущего состояния (не аудировано в этом
документе). После Phase 7 (миграция) приведу к каталогам по разделам
MATLAB-doc (line/polar/contour/surface/volume/geographic/layout/bar).

## 11. VFS-инвариант

> **Любой доступ к файловой системе из `core/` и `toolboxes/*` идёт через
> `numkit::VirtualFS`. Прямые `std::filesystem`, `fopen`, `ifstream`,
> `ofstream`, native `_open`, `CreateFile*` — запрещены.** Единственное
> исключение — реализация `FilesystemFS` сама.

**Зачем:** в WASM нет native FS. Любой доступ должен идти через
VFS-абстракцию, которая в native сборке делегирует `std::filesystem`,
а в WASM-сборке делегирует IDE-callback-у.

**Под правило попадают:**
- `core/` — Engine, runtime, всё что в WASM-сборке
- `toolboxes/builtin/`, `toolboxes/signal/`, `toolboxes/stats/`, `toolboxes/graphics/`, `toolboxes/io/`
- Future toolboxes/*

**Исключения (whitelist):**
- `core/src/vfs.cpp`, `core/include/numkit/core/vfs.hpp` — VFS impl
- `*/tests/*` — тестовая инфраструктура
- `bench*/` — benchmark harnesses
- Host/IDE code — регистрирует VFS backends, не входит в WASM bundle

**Enforcement:** [tools/maintenance/check_vfs_invariant.sh](../tools/maintenance/check_vfs_invariant.sh)
— grep-based линтер. Прогоняется в CI / pre-commit. При нарушении —
exit 1 с указанием файлов и строк.

Текущее состояние (Phase 0 audit): **0 нарушений**. Всё уже чисто.

## 12. M-file resolver via VFS

Вторая часть резолвера (помимо builtin namespace lookup) — **загрузка
user m-файлов** из path-ов через VFS.

### 12.1 Path registry в Engine

```cpp
class Engine {
public:
    // Добавить директорию в path. Принимает VFS-путь
    // ("native:/usr/local/numkit", "local:/scripts", "temporary:/work").
    void addPath(const std::string &dir);
    void rmPath(const std::string &dir);
    std::vector<std::string> path() const;

    // Доступ к скомпилированному m-файлу — с кэшем по mtime.
    const UserFunction *resolveMFile(const std::string &qualifiedName,
                                     const Environment &scope);
};
```

### 12.2 Lookup при коротком имени `name` (продолжение Section 3 step 5)

```
1. script-origin dir (директория где живёт текущий скрипт):
   resolvePath(scriptOrigin + "/" + name + ".m") → если файл есть, грузим.
2. addpath_ list (в порядке регистрации):
   for each dir in addpath_:
     resolvePath(dir + "/" + name + ".m") → если файл есть, грузим.
3. Активные импорты (для +pkg/ packages):
   for each import in activeImports_:
     try +pkg path → resolveMFile("import.prefix.name")
```

### 12.3 Lookup при dotted-name `a.b.c`

```
Полный путь "a.b.c" → ищем `+a/+b/c.m` в:
  - script-origin dir
  - каждом addpath_ dir
  - возвращаем первый найденный
```

Если не найдено — fallback на builtin namespace lookup (т.е. может это
`a.b.c` как зарегистрированная функция, не m-file).

### 12.4 Caching

```cpp
struct CompiledScript {
    UserFunction func;
    std::optional<int64_t> mtime;     // время модификации, если VFS поддерживает
    std::optional<std::string> hash;   // content hash, fallback
};

std::unordered_map<std::string, CompiledScript> mFileCache_;
```

При повторном lookup-е:
- Если VFS-backend поддерживает `stat()` — сравниваем mtime
- Иначе — вычисляем content hash, сравниваем
- При совпадении — берём из кэша; иначе — перекомпилируем

### 12.5 Builtins для path management

Добавляются в core (toolboxes/io/folders/ или toolboxes/builtin/programming/path/):

- `addpath(dir)` — `engine.addPath(dir)`
- `rmpath(dir)` — `engine.rmPath(dir)`
- `path()` — возвращает список как cellstr
- `which('foo')` — где найдена `foo` (возвращает full path или
  "built-in (signal.transforms.fft)" для namespaced builtin)
- `exist('foo','file')` — существует ли m-файл `foo.m` в path
- `run('script.m')` — выполнить файл как скрипт
- `rehash` — инвалидировать m-file cache

### 12.6 Shadow-rule — m-file vs builtin

**MATLAB-стиль:** m-file в текущей директории (script-origin dir)
**перебивает** одноимённый builtin. m-file в addpath не перебивает.

Реализация в Section 3 step 5a — script-origin dir lookup-ится
**раньше** builtin lookup в специальном случае. То есть пересмотренный
порядок:

```
1. Local scope
2. Workspace
3. User functions
4. *** m-file в script-origin dir (приоритет над builtins) ***
5. Builtin registry (Section 3.4 в первоначальном порядке)
6. M-file в addpath / +pkg/
7. Error
```

Это даёт пользователю возможность переопределить builtin своей
локальной версией (как в MATLAB), но не позволяет случайному файлу из
системного path-а перебить builtin.

## 13. User packages — `+pkg/` directories через VFS — **DONE (Phase 10)**

MATLAB convention: каталог `+pkgname/` содержит функции пакета.

**Пример:**
```
local:/scripts/
  +myutil/
    helper.m              → myutil.helper
    +sub/
      deep.m              → myutil.sub.deep
```

**Использование (что РЕАЛИЗОВАНО):**
```matlab
addpath('local:/scripts')

% Полный путь — direct qualified call (TW + VM):
y = myutil.helper(x);
z = myutil.sub.deep(y);

% Через wildcard import:
import myutil.*
y = helper(x);

% Через single-symbol import:
import myutil.helper
y = helper(x);

% Через nested wildcard:
import myutil.sub.*
z = deep(y);
```

**Что НЕ работает (известные gap'ы):**
- `import myutil as m; m.helper(x)` — alias-форма для qualified call.
  Resolver ходит по imports, но при `m.helper` не подменяет `m → myutil`.
  Workaround: `import myutil.*; helper(x)`.
- VM CSL `[s.field]` для смешанных рядов с не-CSL элементами проходит
  через `HORZCAT_APPEND` per-element — квадратика на больших рядах.

**Резолвер:** при встрече dotted имени `pkg.fn` пробует:
1. `externalFuncs_["pkg.fn"]` (зарегистрированный namespace builtin).
2. `userFuncs_["pkg.fn"]` (уже загруженный m-file).
3. `+pkg/<fn>.m` в script-origin и addpath-ах. При имени `pkg.sub.fn`
   — `+pkg/+sub/<fn>.m`.

**Compiler invariant:** для `pkg.foo(x)` без import компилятор делает
compile-time check `engine.workspaceEnv().getLocal("pkg")`. Workspace
переменная с тем же именем затеняет namespace (MATLAB precedence).
Зависит от per-statement split в `Engine::eval`.

**Compiler chunk key:** `Compiler::registerFunctionAs(qualifiedName,
funcDef)` биндит чанк под полным qualified-именем (`+a/foo.m` → `a.foo`,
`+b/foo.m` → `b.foo`), чтобы пакеты с одинаковым leaf не конфликтовали.

**Resolver invariant:** `Engine::lookupUserFunction(name, env)` проходит
imports в обратном порядке (latest-pushed wins). MATLAB-семантика
shadowing для `import a.*; import b.*;` с пересекающимися leaves.

**Implementation:** см. реализацию в [`engine.cpp`](core/src/engine.cpp)
(`resolveMFile_`, `lookupUserFunction`, `walkImportCandidates_`),
[`compiler.cpp`](core/src/compiler.cpp) (`compileCall` qualified detect,
`registerFunctionAs`), [`tree_walker.cpp`](core/src/tree_walker.cpp)
(`execCall` qualified detect).

## 14. Engine API — финальный

```cpp
class Engine {
public:
    // ── Function registration ─────────────────────────────────

    // Регистрация функции в namespace. Это ЕДИНСТВЕННЫЙ метод
    // регистрации — нет специальных вариантов для core, compat,
    // promotion и т.д. Каждый случай — это просто разный аргумент `ns`:
    //   ns = ""                    → core (плоский, без префикса)
    //   ns = "signal.transforms"   → registry["signal.transforms.<name>"]
    //   ns = "compat"              → registry["compat.<name>"]
    // Throws при дубликате полного имени (ns.name уже зарегистрировано).
    void registerFunction(const std::string &ns,
                          const std::string &name,
                          ExternalFunc func);

    // ── Path management (VFS-based) ───────────────────────────

    void addPath(const std::string &dir);
    void rmPath(const std::string &dir);
    std::vector<std::string> path() const;

    // ── M-file caching (Section 12.4) ─────────────────────────

    void rehash();   // инвалидировать m-file cache

    // ── REPL persistent imports ───────────────────────────────

    // По умолчанию false. REPL ставит true, чтобы import-ы
    // не сбрасывались между eval()-ами.
    void setPersistentImports(bool on);

private:
    // Реестр (full_name → ExternalFunc). full_name: "ns.sub.fn" или
    // просто "fn" для core.
    std::unordered_map<std::string, ExternalFunc> registry_;

    // Short-name индекс для wildcard imports (`import x.*`,
    // `import compat.*`). short_name → list of full_name.
    std::unordered_multimap<std::string, std::string> shortNameIndex_;

    // Список зарегистрированных top-level namespace-ов в порядке
    // регистрации (для детерминированного поведения).
    std::vector<std::string> namespaceOrder_;

    // Path registry для m-file resolver.
    std::vector<std::string> addPath_;

    // M-file cache.
    std::unordered_map<std::string, CompiledScript> mFileCache_;
};
```

**Никаких флагов состояния** (`setMatlabCompatMode` нет). Engine — pure
dispatcher.

## 15. Resolver — финальный

```cpp
class Environment {
public:
    struct Import {
        std::vector<std::string> prefix;   // ["signal", "transforms"]
        std::string symbol;                // "fft" или "" для wildcard/alias
        std::string alias;                 // "tr" или ""
        bool wildcard;                     // true для `import x.*`
    };

    std::vector<Import> activeImports_;
};
```

При входе в function body / script — push нового пустого фрейма
импортов. При выходе — pop. Просто.

## 16. Conflict detection

**Единственный механизм:** Engine throws при попытке зарегистрировать
функцию с full_name (включая namespace), который уже занят.

Это **generic** правило — никаких особых случаев. Применяется одинаково
к core, compat, signal.transforms, и любому другому namespace:

- **Дубликат full_name** в одном и том же namespace:
  `registerFunction("signal.transforms", "fft", ...)` дважды → throw
  "duplicate registration: signal.transforms.fft"
- **Коллизия в compat** (signal.foo + stats.foo обе пытаются):
  второй вызов `registerFunction("compat", "foo", ...)` → throw
  "duplicate registration: compat.foo"
- **Конфликт core ↔ namespace ОК:** `registerFunction("", "fft", ...)`
  и `registerFunction("signal.transforms", "fft", ...)` — это разные
  full_names (`fft` и `signal.transforms.fft`), оба валидны (это
  механизм 6 промоций).
- **Конфликт namespace ↔ namespace ОК:** `signal.foo` и `stats.foo`
  валидны параллельно (разные full_names), пока кто-то один не пытается
  alias в compat. Пользователь использует полный путь для дисамбигуации.

## 17. Migration plan — **ALL PHASES DONE**

- Phase 0: FS-audit + lint script — **DONE**
- Phase 1: Этот документ — **DONE**
- Phase 2: TODO с namespace-аннотациями — **DONE**
- Phase 3: Directory refactor (Variant B) — **DONE** (toolboxes/{builtin,signal,stats,graphics,io})
- Phase 4: Engine API (registerFunction, indices, conflict detection) — **DONE**
- Phase 5: `import` builtin (command-style + function-style) — **DONE**
- Phase 6: Resolver + activeImports — **DONE**
- Phase 7: Миграция libs (signal, stats с переездом var/std, graphics, новый io) — **DONE**
- Phase 8: Расширение VFS API (listDir, stat, mkdir, ...) — **DONE**
- Phase 9: addpath/path + m-file resolver — **DONE**
- Phase 10: +packagedir/ user namespaces — **DONE**

Plus follow-up audit cleanup (5 batches, ~10 commits):
- Cleanup: dedup auto-grow / import-walk / qualified-name AST helpers,
  drop dead leaf-fallback in VM, decouple Compiler from Engine private
  state, unify struct storage to AoS-only.
- Tests: edge cases (multi-import shadow, recursive package call,
  cross-package call, large struct array, in-function shadow).
- Build: validated 7 of 12 presets (desktop-fast, threads, portable,
  bench-simd-clang, bench-clang-asan, wasm, bench-wasm).
- Semantics: `s.f = val` broadcast on multi-element struct arrays.
- Struct array features: indexed write `d(i).field = val`, auto-grow
  on out-of-range index, `struct('a', {1,2,3})` cell-input constructor,
  CSL `[s.field]` expansion in matrix literals.

Test fixture получает `import compat.*;` в setup.

## 18. Open questions

После всех итераций ответ на этот раздел: **none**. Все вопросы
решены:

- ~~Q1 (mag2db): core~~
- ~~Q2 (xcov): только в signal.*, не promotion~~
- ~~Q3 (var/std): переезжают в stats.*~~
- ~~Q4 (cellfun): core~~
- ~~Q5 (modern I/O): io.*~~
- ~~Q6 (table I/O): table.* (когда появится)~~
- ~~Q7 (categorical): categorical.*~~
- ~~Q8 (import гранулярность): да, обе формы~~
- ~~Q9 (compat resolution): через двойную регистрацию + generic conflict-detection~~
- ~~Q10 (import scope): MATLAB-стиль, push/pop по scope~~
- ~~Q11 (operator-функции): core~~
- ~~Q12 (коллизии при регистрации): error при register/alias~~
- ~~Q13 (m-file shadow): script-origin dir wins, addpath не wins~~

---

*Дата: 2026-04-30 (final).*
