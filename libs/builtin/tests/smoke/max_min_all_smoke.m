clear
import compat.*

% max/min(A, [], 'all') — reduce over EVERY element. DEEP-PROBE 2026-06:
% the 'all' option was entirely broken (toScalar on the 'all' string). The
% 2nd output is the linear index. Reference: MATLAB R2025b.

A = [3 1; 4 1; 2 9];
[m, i] = max(A, [], 'all');
fprintf('max(A,[],''all'') = %g  at linear index %g   (expect 9, 6)\n', m, i);
fprintf('min(A,[],''all'') = %g                       (expect 1)\n', min(A, [], 'all'));

% 'all' + 'linear' (index is linear either way).
[m2, i2] = max(A, [], 'all', 'linear');
fprintf('max all+linear   = %g  i = %g               (expect 9, 6)\n', m2, i2);

% 3-D array.
B = reshape(1:24, 2, 3, 4);
fprintf('max(B,[],''all'') = %g                      (expect 24)\n', max(B, [], 'all'));

% omitnan over all.
[mo, io] = min([5 NaN 2 8], [], 'all', 'omitnan');
fprintf('min all omitnan  = %g  i = %g               (expect 2, 3)\n', mo, io);
