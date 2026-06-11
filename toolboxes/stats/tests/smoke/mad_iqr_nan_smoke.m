clear

import compat.*

% mad / iqr NaN omission (2026-05-30): MATLAB mad and iqr treat NaN as
% missing and remove it per column before computing. numkit previously
% NaN-poisoned. range already omits; geomean/harmmean propagate (correct).
% vs MATLAB R2025b.

Mn = [1 5; 2 NaN; 3 7; 4 100];

fprintf('=== mad / iqr omit NaN per column ===\n');
fprintf('mad(Mn)   -> %s (expect [1 41.778])\n', mat2str(mad(Mn),6));
fprintf('mad(Mn,1) -> %s (expect [1 2], median abs dev)\n', mat2str(mad(Mn,1),6));
fprintf('iqr(Mn)   -> %s (expect [2 71.25])\n', mat2str(iqr(Mn),6));

fprintf('\n=== vector with interior NaN ===\n');
fprintf('mad([1 2 NaN 4 100]) -> %g (expect 36.625)\n', mad([1 2 NaN 4 100]));
fprintf('iqr([1 2 NaN 4 100]) -> %g (expect 50.5)\n', iqr([1 2 NaN 4 100]));

fprintf('\n=== clean data unchanged ===\n');
fprintf('mad([1 5;2 6;3 7;4 100]) -> %s (expect [1 35.25])\n', mat2str(mad([1 5;2 6;3 7;4 100]),6));
fprintf('iqr([1 5;2 6;3 7;4 100]) -> %s (expect [2 48])\n', mat2str(iqr([1 5;2 6;3 7;4 100]),6));
