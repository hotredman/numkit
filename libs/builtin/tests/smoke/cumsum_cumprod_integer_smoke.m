clear

import compat.*

% cumsum / cumprod preserve the integer class and accumulate NATIVELY with
% saturation at each step. Added 2026-05-30 (DEEP-PROBE). vs MATLAB R2025b.

fprintf('=== cumsum integer: class + saturation ===\n');
a = cumsum(int8([100 100 100]));
fprintf('cumsum(int8([100 100 100])) = %s class=%s (expect [100 127 127] int8)\n', mat2str(a), class(a));
b = cumsum(int8([100 100 -100]));
fprintf('cumsum(int8([100 100 -100])) = %s (expect [100 127 27], native: clamped 127 carried fwd)\n', mat2str(b));
c = cumsum(uint8([200 100]));
fprintf('cumsum(uint8([200 100])) = %s (expect [200 255])\n', mat2str(c));
e = cumsum(int16([10 20 30]));
fprintf('cumsum(int16([10 20 30])) = %s class=%s (expect [10 30 60] int16)\n', mat2str(e), class(e));

fprintf('\n=== cumprod integer: class + saturation ===\n');
d = cumprod(int8([5 10 10]));
fprintf('cumprod(int8([5 10 10])) = %s class=%s (expect [5 50 127] int8, 500->127)\n', mat2str(d), class(d));
h = cumprod(int8([2 3 4]));
fprintf('cumprod(int8([2 3 4])) = %s (expect [2 6 24])\n', mat2str(h));

fprintf('\n=== per-dim on a matrix keeps class ===\n');
f = cumsum(int32([1 2; 3 4]));
fprintf('cumsum(int32([1 2;3 4]))   = %s (expect [1 2;4 6])\n', mat2str(f));
g = cumsum(int32([1 2; 3 4]), 2);
fprintf('cumsum(int32([1 2;3 4]),2) = %s class=%s (expect [1 3;3 7] int32)\n', mat2str(g), class(g));

fprintf('\n=== double regress ===\n');
fprintf('cumsum([1 2 3 4]) = %s (expect [1 3 6 10])\n', mat2str(cumsum([1 2 3 4])));
fprintf('cumprod([1 2 3 4]) = %s (expect [1 2 6 24])\n', mat2str(cumprod([1 2 3 4])));
