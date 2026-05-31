clear
import compat.*
% sortrows on COMPLEX matrices: order rows lexicographically; each column
% compares by magnitude |z| then phase angle arg(z). MATLAB R2025b.
% (numkit previously dropped the imaginary part and returned wrong rows.)

A = [3+4i 2; 1 1; 3+4i 0; 1 5i];
[S, ix] = sortrows(A);
fprintf('col1 = %s (expect [1 1 3+4i 3+4i])\n', mat2str(S(:,1).'));
fprintf('col2 = %s (expect [1 5i 0 2])\n', mat2str(S(:,2).'));
fprintf('idx  = %s (expect [2 4 3 1])\n', mat2str(ix(:).'));

D = sortrows(A, -1);
fprintf('desc col1 = %s (expect [3+4i 3+4i 1 1])\n', mat2str(D(:,1).'));

v = sortrows([3+4i; 1; 5i]);
fprintf('colvec = %s (expect [1 3+4i 5i])\n', mat2str(v.'));
