clear

import compat.*

% rat / rats — rational approximation via regularized continued fractions.
% After the audit ТЗ closure (2026-05-09):
%   rat(x[, tol])     1-output  → nested CF string  '3 + 1/(7 + 1/(16))'
%   [N, D] = rat(...) 2-output  → numeric numerator + denominator
%   rats(x[, len])               → fixed-width 'numer/denom' field
% All bit-identical to MATLAB R2025b on probed inputs.

fprintf('=== 1-output continued-fraction strings ===\n');
fprintf('  rat(pi, 1e-3) = ''%s''\n', rat(pi, 1e-3));
fprintf('  expect MATLAB: 3 + 1/(7 + 1/(16))\n');
fprintf('  rat(0.5)      = ''%s''\n', rat(0.5));
fprintf('  expect MATLAB: 1 + 1/(-2)\n');
fprintf('  rat(1/3)      = ''%s''\n\n', rat(1/3));

fprintf('=== 2-output [N, D] form (the gap that was fixed) ===\n');
[Np, Dp] = rat(pi, 1e-3);
fprintf('  [N, D] = rat(pi, 1e-3) → N=%d, D=%d  (expect 355, 113)\n', Np, Dp);
[Nh, Dh] = rat(0.5);
fprintf('  [N, D] = rat(0.5)      → N=%d, D=%d  (expect 1, 2)\n', Nh, Dh);
[Nv, Dv] = rat([0.5; 1/3; pi]);
fprintf('  [N, D] = rat([0.5; 1/3; pi]):\n');
disp([Nv Dv]);

fprintf('\n=== rats — fixed-width formatting ===\n');
fprintf('  rats(0.5)    = ''%s''  (length %d)\n', rats(0.5),  strlength(rats(0.5)));
fprintf('  rats(pi)     = ''%s''  (length %d)\n', rats(pi),   strlength(rats(pi)));
fprintf('  rats(1/3)    = ''%s''  (length %d)\n', rats(1/3),  strlength(rats(1/3)));
fprintf('  rats(0.5, 5) = ''%s''  (length %d)\n', rats(0.5, 5), strlength(rats(0.5, 5)));
