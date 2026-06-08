clear

import compat.*

% mode 3rd output C (2026-05-30): [M,F,C] = mode(X). C is a cell array of
% the modal values -- each cell holds a sorted column vector of all values
% tied for the modal frequency in that slice. numkit previously returned
% only [M,F]. Supported for real double vector / 2-D matrix / 'all'.
% vs MATLAB R2025b.

fprintf('=== vector (2 and 3 both occur twice) ===\n');
[m,f,c] = mode([3 3 1 2 2]);
fprintf('M=%g F=%g C{1}=%s (expect 2 2 [2;3])\n', m, f, mat2str(c{1}));

fprintf('\n=== matrix, per column ===\n');
M = [1 1; 2 2; 1 3; 2 2];
[mm,fm,cm] = mode(M);
fprintf('col1 C=%s (expect [1;2])\n', mat2str(cm{1}));
fprintf('col2 C=%s (expect 2)\n', mat2str(cm{2}));

fprintf('\n=== matrix, per row (dim 2) ===\n');
[mr,fr,cr] = mode(M,2);
fprintf('row3 C=%s (expect [1;3])\n', mat2str(cr{3}));
