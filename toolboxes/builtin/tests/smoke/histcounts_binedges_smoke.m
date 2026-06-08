clear

import compat.*

% histcounts 'BinEdges' name-value + [n, edges] second output. Fixed
% 2026-05-30: 'BinEdges' threw "Not a double array" (the name-value wasn't
% parsed), and histcounts returned counts only (no edges). vs MATLAB R2025b.

fprintf('=== ''BinEdges'' name-value ===\n');
[n, e] = histcounts([1 2 3 4 5], 'BinEdges', [0 2 4 6]);
fprintf('n = %s (expect [1 2 2])\n', mat2str(n));
fprintf('e = %s (expect [0 2 4 6])\n', mat2str(e));

fprintf('\n=== positional edges, 2-output ===\n');
[n2, e2] = histcounts([1 2 3 4 5], [0 2 4 6]);
fprintf('n2 = %s  e2 = %s (expect [1 2 2] / [0 2 4 6])\n', mat2str(n2), mat2str(e2));

fprintf('\n=== ''BinEdges'' + ''Normalization'' ===\n');
p = histcounts([1 2 3 4 5], 'BinEdges', [0 2 4 6], 'Normalization', 'probability');
fprintf('p = %s (expect [0.2 0.4 0.4])\n', mat2str(p));

fprintf('\n=== single output still works ===\n');
fprintf('n3 = %s (expect [1 2 2])\n', mat2str(histcounts([1 2 3 4 5], [0 2 4 6])));
