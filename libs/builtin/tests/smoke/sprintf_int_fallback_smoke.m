clear

import compat.*

% Integer conversion (%d/%i/%u/%o/%x) on a non-integer value. Bug fixed
% 2026-05-30: numkit truncated to int (sprintf('%d',3.7) -> '3'). MATLAB
% overrides the conversion to %e. vs MATLAB R2025b.

fprintf('=== non-integer -> %%e fallback ===\n');
fprintf('%%d of 3.7   = [%d]  (expect [3.700000e+00])\n', 3.7);
fprintf('%%i of 3.7   = [%i]  (expect [3.700000e+00])\n', 3.7);
fprintf('%%x of 3.7   = [%x]  (expect [3.700000e+00])\n', 3.7);
fprintf('%%.2d of 3.7 = [%.2d]  (expect [3.70e+00])\n', 3.7);
fprintf('%%d of -2.5  = [%d]  (expect [-2.500000e+00])\n', -2.5);

fprintf('\n=== whole numbers stay integer ===\n');
fprintf('%%d of 5     = [%d]  (expect [5])\n', 5);
fprintf('%%d of 1e10  = [%d]  (expect [10000000000])\n', 1e10);
fprintf('%%x of 255   = [%x]  (expect [ff])\n', 255);

fprintf('\n=== non-finite ===\n');
fprintf('%%d of Inf   = [%d]  (expect [Inf])\n', Inf);
fprintf('%%d of -Inf  = [%d]  (expect [-Inf])\n', -Inf);
fprintf('%%d of NaN   = [%d]  (expect [NaN])\n', NaN);
