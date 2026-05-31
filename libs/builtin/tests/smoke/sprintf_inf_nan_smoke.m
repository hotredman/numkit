clear

import compat.*

% sprintf/fprintf %f/%e/%g of Inf/NaN. Bug fixed 2026-05-30: numkit printed
% the C library's lowercase 'inf'/'nan'; MATLAB prints 'Inf'/'NaN'. vs
% MATLAB R2025b.

fprintf('=== plain ===\n');
fprintf('%%f Inf/NaN = [%f] [%f] (expect [Inf] [NaN])\n', Inf, NaN);
fprintf('%%g Inf / %%e -Inf = [%g] [%e] (expect [Inf] [-Inf])\n', Inf, -Inf);

fprintf('\n=== width honoured, precision ignored ===\n');
fprintf('%%8.2f Inf = [%8.2f] (expect [     Inf])\n', Inf);
fprintf('%%-8f Inf = [%-8f] (expect [Inf     ])\n', Inf);

fprintf('\n=== ''+'' flag: +Inf gets a sign, NaN does not ===\n');
fprintf('%%+f Inf / NaN = [%+f] [%+f] (expect [+Inf] [NaN])\n', Inf, NaN);
fprintf('%%f -Inf = [%f] (expect [-Inf])\n', -Inf);

fprintf('\n=== finite values unchanged ===\n');
fprintf('%%.2f / %%g = [%.2f] [%g] (expect [3.14] [100000])\n', 3.14159, 100000);
