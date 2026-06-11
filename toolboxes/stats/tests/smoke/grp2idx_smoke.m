clear

import compat.*

% grp2idx: turn a grouping variable into a 1-based index vector + names.
% Implemented 2026-05-30 (was an undefined function). vs MATLAB R2025b.

fprintf('=== cellstr: first-appearance order ===\n');
[g, gn, gl] = grp2idx({'b','a','b','c'});
fprintf('g  = %s (expect [1;2;1;3])\n', mat2str(g));
fprintf('gn = %s|%s|%s (expect b|a|c)\n', gn{1}, gn{2}, gn{3});
fprintf('gl = %s|%s|%s (gl == gn)\n', gl{1}, gl{2}, gl{3});

fprintf('\n=== numeric: sorted ascending, index = rank ===\n');
[g2, gn2] = grp2idx([3 1 3 2 1]);
fprintf('g  = %s (expect [3;1;3;2;1])\n', mat2str(g2));
fprintf('gn = %s|%s|%s (expect 1|2|3)\n', gn2{1}, gn2{2}, gn2{3});

fprintf('\n=== negative + fractional names via num2str ===\n');
[g3, gn3] = grp2idx([10 -5 10 3]);
fprintf('gn = %s|%s|%s (expect -5|3|10)\n', gn3{1}, gn3{2}, gn3{3});

fprintf('\n=== NaN -> NaN index, excluded from group set ===\n');
[g4, gn4] = grp2idx([3 1 NaN 2]);
fprintf('g  = %s ng = %d (expect [3;1;NaN;2], 3)\n', mat2str(g4), numel(gn4));

fprintf('\n=== logical: sorted false<true ===\n');
[g5, gn5] = grp2idx(logical([1 0 1 0]));
fprintf('g  = %s gn = %s|%s (expect [2;1;2;1], 0|1)\n', mat2str(g5), gn5{1}, gn5{2});
