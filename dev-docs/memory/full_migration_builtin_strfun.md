# Full Migration to Pure C++ Builtin: String Manipulation (`strfun`)

## Context
As part of the layer separation refactoring (Layer L2 Compute vs Layer L3 Registration), string and character array manipulation, formatting, pattern matching, regex, and predicates (`strfun`) were migrated to pure, engine-free C++ in `src/builtin/` under `namespace numkit::builtin`.

## Architectural Decision & Changes
1. **Public Engine-Free C++ Header**:
   - `include/numkit/builtin/strfun.hpp` defines the public C++ API with comprehensive Doxygen documentation, parameter descriptions, formulas, exceptions, and `@see` links.
   - Decoupled from `Engine`, `CallContext`, and the VM.

2. **Compute Implementation (`src/builtin/src/strfun/`)**:
   - `conversion.cpp`: `num2str`, `int2str`, `mat2str`, `str2num`, `str2double`, `char_array`, `string_array`, `blanks`, `newline`, `dec2bin`, `dec2hex`, `dec2base`, `base2dec`, `bin2dec`, `hex2dec`.
   - `manipulation.cpp`: `strcmp`, `strncmp`, `strcmpi`, `strncmpi`, `matches`, `upper`, `lower`, `strtrim`, `deblank`, `strip`, `strcat`, `strjoin`, `strsplit`, `splitlines`, `strfind`, `strrep`, `contains`, `startsWith`, `endsWith`, `count`, `reverse`.
   - `predicates.cpp`: `isletter`, `isspace`, `isstrprop`, `isstringscalar`, `validatestring`, `convertContainedStringsToChars`.
   - `regex.cpp`: `regexp`, `regexpi`, `regexprep`, `regexptranslate`.

3. **Engine Registration (`src/bundle/src/register/builtin/`)**:
   - `strfun_reg.cpp` registers all string builtins into `Engine` under `namespace numkit::bundle::builtin`.
   - Old monolithic `src/builtin/src/strfun.cpp` was deleted.

## Verification & Results
- Built with `desktop-fast` preset (MSVC).
- Layer validation: `python tools/check_layering.py` passed with 0 violations.
- Test suite: `numkit_gtest.exe --gtest_filter=*Strfun*:*Builtin*` passed 694/694 tests (100% green).
