clear

import compat.*

% abs() on integer types: MATLAB keeps the class and SATURATES.
% abs(intmin) -> intmax. Added 2026-05-30 (DEEP-PROBE). vs MATLAB R2025b.

fprintf('=== signed saturation at intmin ===\n');
a = abs(int8(-128));
fprintf('abs(int8(-128))    = %g class=%s (expect 127 int8, NOT 128)\n', double(a), class(a));
b = abs(int16(-32768));
fprintf('abs(int16(-32768)) = %g class=%s (expect 32767 int16)\n', double(b), class(b));
c = abs(int32(-2147483648));
fprintf('abs(int32(intmin)) = %g class=%s (expect 2147483647 int32)\n', double(c), class(c));

fprintf('\n=== ordinary values keep class ===\n');
d = abs(int8([-3 -128 5]));
fprintf('abs(int8([-3 -128 5])) = %s class=%s (expect [3 127 5] int8)\n', mat2str(d), class(d));
e = abs(int32(-7));
fprintf('abs(int32(-7)) = %g class=%s (expect 7 int32)\n', double(e), class(e));

fprintf('\n=== unsigned unchanged ===\n');
u = abs(uint8(200));
fprintf('abs(uint8(200)) = %g class=%s (expect 200 uint8)\n', double(u), class(u));

fprintf('\n=== double / complex regress ===\n');
fprintf('abs(-3.5) = %g class=%s (expect 3.5 double)\n', abs(-3.5), class(abs(-3.5)));
fprintf('abs(3-4i) = %g (expect 5)\n', abs(3-4i));
fprintf('abs([-1.5 2.5 -3.5]) = %s (expect [1.5 2.5 3.5])\n', mat2str(abs([-1.5 2.5 -3.5])));
