clear

import compat.*

% Type-conversion batch — audit ТЗ closure 2026-05-09. 12 functions.

fprintf('int8(127)        = %d\n',  int8(127));
fprintf('int16(32767)     = %d\n',  int16(32767));
fprintf('int32(2147483647)= %d\n',  int32(2147483647));
fprintf('uint8(255)       = %d\n',  uint8(255));
fprintf('uint8(300)       = %d  (saturate to 255)\n', uint8(300));
fprintf('int8(-200)       = %d  (saturate to -128)\n', int8(-200));
fprintf('double(int8(50)) = %g\n', double(int8(50)));
fprintf('single(3.14)     = %g\n', single(3.14));
fprintf('logical(5)       = %d  (any non-zero = true)\n', logical(5));
fprintf('char(65)         = "%s"\n', char(65));
