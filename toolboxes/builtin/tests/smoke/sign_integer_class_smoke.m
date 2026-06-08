clear

import compat.*

% sign() on integer types keeps the integer class.
% Added 2026-05-30 (DEEP-PROBE, closes the integer-class family). vs MATLAB R2025b.

fprintf('=== integer sign: class preserved ===\n');
a = sign(int8(-5));
fprintf('sign(int8(-5))        = %g class=%s (expect -1 int8)\n', double(a), class(a));
b = sign(int8([-5 0 9]));
fprintf('sign(int8([-5 0 9]))  = %s class=%s (expect [-1 0 1] int8)\n', mat2str(b), class(b));
c = sign(uint8(0));
fprintf('sign(uint8(0))        = %g class=%s (expect 0 uint8, never -1)\n', double(c), class(c));
d = sign(int32([-100 100]));
fprintf('sign(int32([-100 100])) = %s class=%s (expect [-1 1] int32)\n', mat2str(d), class(d));

fprintf('\n=== double regress ===\n');
fprintf('sign(-3.5)=%g sign(0)=%g sign(2.1)=%g class=%s (expect -1 0 1 double)\n', ...
        sign(-3.5), sign(0), sign(2.1), class(sign(-3.5)));
fprintf('sign(NaN) isnan=%d (expect 1)\n', isnan(sign(NaN)));
