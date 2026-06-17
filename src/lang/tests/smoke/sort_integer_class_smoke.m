clear

import compat.*

% sort() on integer types keeps the class on the sorted VALUES; the index
% output stays double. Added 2026-05-30 (DEEP-PROBE). vs MATLAB R2025b.

fprintf('=== integer sort: class preserved on values ===\n');
a = sort(int8([3 -128 5]));
fprintf('sort(int8([3 -128 5]))         = %s class=%s (expect [-128 3 5] int8)\n', mat2str(a), class(a));
b = sort(int8([3 -128 5]), 'descend');
fprintf('sort(int8([3 -128 5]),''descend'') = %s class=%s (expect [5 3 -128] int8)\n', mat2str(b), class(b));
u = sort(uint8([200 5 100]));
fprintf('sort(uint8([200 5 100]))       = %s class=%s (expect [5 100 200] uint8)\n', mat2str(u), class(u));

fprintf('\n=== [s,i]: values keep class, index is double ===\n');
[s, ix] = sort(int8([30 10 20]));
fprintf('s=%s class=%s (expect [10 20 30] int8)\n', mat2str(s), class(s));
fprintf('ix=%s class=%s (expect [2 3 1] double)\n', mat2str(ix), class(ix));

fprintf('\n=== per-dim matrix keeps class ===\n');
c = sort(int32([3 1 2; 6 5 4]), 2);
fprintf('sort(int32 dim2) = %s class=%s (expect [1 2 3;4 5 6] int32)\n', mat2str(c), class(c));
d = sort(int32([3 1; 2 4]));
fprintf('sort(int32 dim1) = %s (expect [2 1;3 4])\n', mat2str(d));

fprintf('\n=== double regress ===\n');
[sd, id] = sort([3 1 2]);
fprintf('sort([3 1 2]) = %s idx=%s class=%s (expect [1 2 3] [2 3 1] double)\n', mat2str(sd), mat2str(id), class(sd));
