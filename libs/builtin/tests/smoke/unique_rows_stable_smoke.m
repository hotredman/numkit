clear
import compat.*
% unique(M,'rows','stable'): keep distinct rows in first-occurrence order
% (not lexicographic sort). DEEP-PROBE 2026-05-31: the 'rows' path used to
% drop 'stable' and always sort. Values pinned vs MATLAB R2025b.

M = [3 0; 1 0; 2 0; 1 0; 3 0];
[cr, iar, icr] = unique(M, 'rows', 'stable');
disp('C (expect [3 0; 1 0; 2 0]):'); disp(cr);
fprintf('ia (expect [1 2 3]): ');     disp(iar');
fprintf('ic (expect [1 2 3 2 1]): '); disp(icr');

% Single-output form keeps the same order.
D = unique([5 5; 1 1; 5 5; 9 9; 1 1], 'rows', 'stable');
disp('D (expect [5 5; 1 1; 9 9]):'); disp(D);

% Default 'sorted' is unchanged.
S = unique(M, 'rows');
disp('sorted (expect [1 0; 2 0; 3 0]):'); disp(S);
