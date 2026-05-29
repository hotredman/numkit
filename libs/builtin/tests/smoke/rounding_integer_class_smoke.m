clear

import compat.*

% floor / ceil / round / fix are identities on integer values but MUST keep
% the integer class (MATLAB R2025b). Added 2026-05-30 (DEEP-PROBE).

fprintf('=== integer input: identity + class preserved ===\n');
a = floor(int8([-3 5 -128]));
fprintf('floor(int8([-3 5 -128])) = %s class=%s (expect [-3 5 -128] int8)\n', mat2str(a), class(a));
b = ceil(int16([-3 5]));
fprintf('ceil(int16([-3 5]))      = %s class=%s (expect [-3 5] int16)\n', mat2str(b), class(b));
c = round(uint8([3 200]));
fprintf('round(uint8([3 200]))    = %s class=%s (expect [3 200] uint8)\n', mat2str(c), class(c));
d = fix(int32([-7 7]));
fprintf('fix(int32([-7 7]))       = %s class=%s (expect [-7 7] int32)\n', mat2str(d), class(d));
e = floor(int8(-5));
fprintf('floor(int8(-5)) scalar   = %g class=%s (expect -5 int8)\n', double(e), class(e));

fprintf('\n=== double regress ===\n');
fprintf('floor(-2.7)=%g ceil(-2.7)=%g round(2.5)=%g fix(-2.7)=%g (expect -3 -2 3 -2)\n', ...
        floor(-2.7), ceil(-2.7), round(2.5), fix(-2.7));
fprintf('floor([1.2 2.8]) = %s (expect [1 2])\n', mat2str(floor([1.2 2.8])));
