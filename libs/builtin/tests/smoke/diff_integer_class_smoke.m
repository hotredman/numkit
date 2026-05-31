clear

import compat.*

% diff() on integer types keeps the class and SATURATES each pass.
% Added 2026-05-30 (DEEP-PROBE). vs MATLAB R2025b.

fprintf('=== integer diff: class + saturation ===\n');
a = diff(int8([10 5 20]));
fprintf('diff(int8([10 5 20]))  = %s class=%s (expect [-5 15] int8)\n', mat2str(a), class(a));
b = diff(int8([-100 100]));
fprintf('diff(int8([-100 100])) = %s class=%s (expect 127 int8, 200 overflow)\n', mat2str(b), class(b));
c = diff(uint8([5 3]));
fprintf('diff(uint8([5 3]))     = %s class=%s (expect 0 uint8, 3-5 underflow)\n', mat2str(c), class(c));
d = diff(int16([10 20 45]));
fprintf('diff(int16([10 20 45])) = %s class=%s (expect [10 25] int16)\n', mat2str(d), class(d));

fprintf('\n=== order n + per-dim keep class ===\n');
e = diff(int8([1 2 4 8]), 2);
fprintf('diff(int8([1 2 4 8]),2) = %s class=%s (expect [1 2] int8)\n', mat2str(e), class(e));
f = diff(int32([1 2; 5 8]));
fprintf('diff(int32([1 2;5 8]))  = %s (expect [4 6])\n', mat2str(f));
g = diff(int32([1 2 3; 5 8 13]), 1, 2);
fprintf('diff(int32 dim2)        = %s class=%s (expect [1 1;3 5] int32)\n', mat2str(g), class(g));

fprintf('\n=== double regress ===\n');
fprintf('diff([1 4 9 16]) = %s (expect [3 5 7])\n', mat2str(diff([1 4 9 16])));
