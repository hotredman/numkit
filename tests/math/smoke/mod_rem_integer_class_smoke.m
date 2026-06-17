clear

import compat.*

% mod / rem on integer types keep the integer class (a double operand is
% promoted to the integer class). Added 2026-05-30 (DEEP-PROBE). vs MATLAB R2025b.

fprintf('=== mod integer: class preserved ===\n');
a = mod(int8(7), int8(3));
fprintf('mod(int8(7),int8(3))   = %g class=%s (expect 1 int8)\n', double(a), class(a));
c = mod(int8(-7), int8(3));
fprintf('mod(int8(-7),int8(3))  = %g class=%s (expect 2 int8, floored)\n', double(c), class(c));
d = mod(int8([7 8 9]), int8(3));
fprintf('mod(int8([7 8 9]),int8(3)) = %s class=%s (expect [1 2 0] int8)\n', mat2str(d), class(d));
e = mod(int8(7), 3);
fprintf('mod(int8(7),3) mixed   = %g class=%s (expect 1 int8)\n', double(e), class(e));

fprintf('\n=== rem integer: class preserved (truncation toward zero) ===\n');
b = rem(int8(-7), int8(3));
fprintf('rem(int8(-7),int8(3))  = %g class=%s (expect -1 int8)\n', double(b), class(b));
f = rem(uint8(200), uint8(7));
fprintf('rem(uint8(200),uint8(7)) = %g class=%s (expect 4 uint8)\n', double(f), class(f));

fprintf('\n=== double regress ===\n');
fprintf('mod(7,3)=%g mod(-10,3)=%g rem(-7,3)=%g mod(5.5,2)=%g class=%s\n', ...
        mod(7,3), mod(-10,3), rem(-7,3), mod(5.5,2), class(mod(7,3)));
